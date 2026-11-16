#!/usr/bin/env python3

import subprocess
import os
import sys
import time
import json
import shutil

# --- Configuration ---
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
EXECUTABLE = os.path.join(PROJECT_ROOT, 'build', 'mun_os')
ROOTFS_DIR = os.path.join(PROJECT_ROOT, 'rootfs')
CONFIGS_DIR = os.path.join(PROJECT_ROOT, 'configs')
STATE_DIR = os.path.expanduser('~/.mun-os')

# ANSI Color Codes for beautiful output
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
RESET = '\033[0m'

# --- Helper Functions ---

def print_test_header(name):
    print(f"\n{YELLOW}===== RUNNING TEST: {name} ====={RESET}")

def print_result(test_name, success):
    if success:
        print(f"{GREEN}[PASS]{RESET} {test_name}")
    else:
        print(f"{RED}[FAIL]{RESET} {test_name}")
        cleanup()
        sys.exit(1)

def run_command(command, use_sudo=False, timeout=10, env=None):
    """Runs a command and returns its result."""
    cmd_list = command.split()
    if use_sudo:
        cmd_list.insert(0, 'sudo')
    
    print(f"  > Executing: {' '.join(cmd_list)}")
    try:
        result = subprocess.run(
            cmd_list,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=env
        )
        return result
    except subprocess.TimeoutExpired:
        print(f"{RED}  > TIMEOUT after {timeout} seconds!{RESET}")
        return None
    except FileNotFoundError:
        print(f"{RED}  > Command not found: {cmd_list[0]}{RESET}")
        return None

def setup():
    """Builds the project and prepares the environment."""
    print(f"{YELLOW}--- Setting up test environment ---{RESET}")
    # 1. Build the project
    build_dir = os.path.join(PROJECT_ROOT, 'build')
    os.makedirs(build_dir, exist_ok=True)
    
    # Check if CMake has already been run
    if not os.path.exists(os.path.join(build_dir, 'Makefile')):
        res_cmake = subprocess.run(['cmake', '..'], cwd=build_dir)
        if res_cmake.returncode != 0:
            print(f"{RED}CMake configuration failed!{RESET}")
            sys.exit(1)

    res_make = subprocess.run(['make'], cwd=build_dir)
    if res_make.returncode != 0:
        print(f"{RED}Build failed!{RESET}")
        sys.exit(1)

    # 2. Ensure rootfs exists
    if not os.path.isdir(ROOTFS_DIR):
        print(f"Rootfs not found at {ROOTFS_DIR}. Attempting to run setup script...")
        setup_script = os.path.join(PROJECT_ROOT, 'scripts', 'setup_rootfs.sh')
        if os.path.exists(setup_script):
            subprocess.run(['bash', setup_script], check=True)
        else:
            print(f"{RED}Error: rootfs setup script not found!{RESET}")
            sys.exit(1)

    # 3. Create test config files
    os.makedirs(CONFIGS_DIR, exist_ok=True)
    with open(os.path.join(CONFIGS_DIR, 'test_sleep.json'), 'w') as f:
        json.dump({
            "hostname": "sleepy-container",
            "rootfs_path": ROOTFS_DIR,
            "command": "/bin/sleep",
            "args": ["10"]
        }, f)
        
    with open(os.path.join(CONFIGS_DIR, 'test_precedence.json'), 'w') as f:
        json.dump({
            "hostname": "precedence-test",
            "rootfs_path": ROOTFS_DIR,
            "command": "/bin/dd",
            "args": ["if=/dev/zero", "of=/dev/null", "bs=1M", "count=70"],
            "resources": { "memory_limit_mb": 128 }
        }, f)

    print(f"{GREEN}Setup complete.{RESET}")


def cleanup():
    """Cleans up any running containers and state."""
    print(f"\n{YELLOW}--- Cleaning up test environment ---{RESET}")
    # Stop any running containers by listing them from the state dir
    if os.path.exists(STATE_DIR):
        state_path = os.path.join(STATE_DIR, 'state')
        if os.path.isdir(state_path):
            for container_name in os.listdir(state_path):
                run_command(f"{EXECUTABLE} stop {container_name}", use_sudo=True)
    # The stop command should remove the state, but we remove the top dir just in case
    if os.path.exists(STATE_DIR):
        shutil.rmtree(STATE_DIR, ignore_errors=True)
        
    # Clean up test configs
    if os.path.exists(os.path.join(CONFIGS_DIR, 'test_sleep.json')):
        os.remove(os.path.join(CONFIGS_DIR, 'test_sleep.json'))
    if os.path.exists(os.path.join(CONFIGS_DIR, 'test_precedence.json')):
        os.remove(os.path.join(CONFIGS_DIR, 'test_precedence.json'))


# --- Test Cases ---

def test_run_foreground():
    test_name = "Run container in foreground"
    print_test_header(test_name)
    res = run_command(f"{EXECUTABLE} run --rootfs {ROOTFS_DIR} /bin/echo 'Hello Foreground'", use_sudo=True)
    success = res is not None and res.returncode == 0 and "Hello Foreground" in res.stdout
    print_result(test_name, success)

def test_lifecycle():
    test_name = "Container Lifecycle (start, list, stop)"
    print_test_header(test_name)
    
    # Start
    res_start = run_command(f"{EXECUTABLE} start {CONFIGS_DIR}/test_sleep.json", use_sudo=True)
    if not (res_start and "Container started in background" in res_start.stdout):
        print_result(test_name, False)
        return
    
    time.sleep(1) # Give it a moment to write state
    
    # List
    res_list = run_command(f"{EXECUTABLE} list")
    if not (res_list and "test_sleep" in res_list.stdout and "running" in res_list.stdout):
        print_result(test_name, False)
        return
        
    # Stop
    res_stop = run_command(f"{EXECUTABLE} stop test_sleep", use_sudo=True)
    if not (res_stop and "stopped and state removed" in res_stop.stdout):
        print_result(test_name, False)
        return
        
    # List again to confirm
    res_list_final = run_command(f"{EXECUTABLE} list")
    if not (res_list_final and "No containers are running" in res_list_final.stdout):
        print_result(test_name, False)
        return

    print_result(test_name, True)

def test_memory_limit():
    test_name = "Memory limit (cgroup)"
    print_test_header(test_name)
    # Try to allocate 50MB with a 20MB limit. Should be killed.
    res = run_command(f"{EXECUTABLE} run --rootfs {ROOTFS_DIR} --memory 20 /bin/dd if=/dev/zero of=/dev/null bs=1M count=50", use_sudo=True)
    # A process killed by OOM killer usually has a non-zero exit code.
    # The parent process (mun_os) might return -1 or a signal code.
    success = res is not None and res.returncode != 0
    print_result(test_name, success)

def test_pid_limit():
    test_name = "PID limit (cgroup)"
    print_test_header(test_name)
    # The fork bomb should fail with "can't fork"
    res = run_command(f"{EXECUTABLE} run --rootfs {ROOTFS_DIR} --pids 10 /bin/sh -c 'bomb() {{ bomb | bomb & }}; bomb'", use_sudo=True)
    success = res is not None and res.returncode != 0 and "can't fork" in res.stderr
    print_result(test_name, success)

def test_config_precedence():
    test_name = "Configuration Precedence (CLI > Env > JSON)"
    print_test_header(test_name)
    
    # JSON sets mem to 128MB.
    # Env Var will set it to 64MB.
    # CLI flag will set it to 32MB.
    # We will test the final 32MB limit by trying to allocate 40MB.
    
    my_env = os.environ.copy()
    my_env["MUN_OS_MEMORY_LIMIT"] = "64"
    
    res = run_command(
        f"{EXECUTABLE} run --config {CONFIGS_DIR}/test_precedence.json --memory 32",
        use_sudo=True,
        env=my_env
    )
    
    # The process should be killed for exceeding the 32MB limit.
    success = res is not None and res.returncode != 0
    print_result(test_name, success)


def main():
    """Main test runner function."""
    if os.geteuid() != 0:
        print(f"{RED}This script must be run with sudo.{RESET}")
        print("Example: sudo python3 tests/test_runner.py")
        sys.exit(1)

    try:
        setup()
        test_run_foreground()
        test_lifecycle()
        test_memory_limit()
        test_pid_limit()
        test_config_precedence()
        print(f"\n{GREEN}✅ ALL TESTS PASSED!{RESET}")
    finally:
        cleanup()

if __name__ == "__main__":
    main()
