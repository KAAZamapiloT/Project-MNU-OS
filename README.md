# Project MNU-OS 🐧📦

[![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux%20Kernel%205.x%2B-orange.svg)](https://kernel.org)
[![Build: CMake](https://img.shields.io/badge/Build-CMake%203.10%2B-brightgreen.svg)](https://cmake.org)
[![Security: Seccomp & Libcap](https://img.shields.io/badge/Security-Seccomp%20%7C%20Capabilities-red.svg)](https://man7.org/linux/man-pages/man2/seccomp.2.html)
[![Networking: veth & bridge](https://img.shields.io/badge/Networking-veth%20%7C%20Bridge%20%7C%20NAT-purple.svg)](https://wiki.linuxfoundation.org/networking/bridge)

**Project MNU-OS** is a modular, lightweight container runtime engineered from scratch in **C++17**. It delivers Docker-like isolation and lifecycle control by directly programming core Linux kernel primitives — including **Namespaces**, **Control Groups (cgroups v2)**, **pivot_root**, **Seccomp BPF filters**, **Linux Capabilities**, and **Virtual Bridge Networking**.

Designed for systems programming education, runtime research, and secure process sandboxing, MNU-OS manages isolated environments with strict resource constraints and hardened security boundaries without relying on third-party container engines.

---

## 🏛️ Architectural Overview

```
                      +---------------------------------------+
                      |               mun_os CLI              |
                      |  (run, start, stop, restart, list...)  |
                      +-------------------+-------------------+
                                          |
          +-------------------------------+-------------------------------+
          |                               |                               |
          v                               v                               v
+-------------------+           +-------------------+           +-------------------+
|   ConfigParser    |           |   StateManager    |           |   CgroupManager   |
| (JSON / Env / CLI)|           | (PID / Daemon /   |           | (cgroups v2: mem, |
| Hybrid Precedence |           |  Lifecycle State) |           |  cpu, pids.max)   |
+-------------------+           +-------------------+           +-------------------+
          |                                                               |
          +-------------------------------+-------------------------------+
                                          |
                                          v
                              +-----------------------+
                              |    Container Engine   |
                              |   (clone syscall &    |
                              |    PTY allocation)    |
                              +-----------+-----------+
                                          |
     +------------------------------------+------------------------------------+
     |                                    |                                    |
     v                                    v                                    v
+------------------------+  +---------------------------+  +--------------------------+
|    Linux Namespaces    |  |     Security Hardening    |  |    Network Isolation     |
| • PID (PID 1 inside)   |  | • pivot_root (safe rootfs)|  | • CLONE_NEWNET           |
| • Mount (private tree) |  | • Read-only rootfs        |  | • Host bridge mun-bridge0|
| • UTS (hostname)       |  | • Writable tmpfs on /tmp  |  | • Dedicated veth pairs   |
| • Network (veth pair)  |  | • Host bind mounts        |  | • Dynamic IP assignment  |
| • User (UID/GID map)   |  | • Dropped capabilities    |  | • iptables NAT & routing |
| • IPC                  |  | • Seccomp BPF syscalls    |  | • Automated DNS setup    |
+------------------------+  +---------------------------+  +--------------------------+
```

---

## ✨ Implemented Core Features

### 1. 🛡️ Linux Namespace Isolation
* **PID Namespace (`CLONE_NEWPID`):** Isolates the process table; the container process becomes `PID 1` inside its namespace.
* **Mount Namespace (`CLONE_NEWNS`):** Creates an independent mount tree preventing container mounts from affecting the host.
* **UTS Namespace (`CLONE_NEWUTS`):** Custom container hostnames independent of the host OS.
* **Network Namespace (`CLONE_NEWNET`):** Dedicated network stack per container with isolated interfaces, routing tables, and firewall rules.
* **User Namespace (`CLONE_NEWUSER`):** Maps container root (`UID 0`) to an unprivileged host user (`UID 1000`) for non-root execution.
* **IPC Namespace (`CLONE_NEWIPC`):** Prevents shared memory and IPC channel leakage.

### 2. ⚡ Resource Limits (cgroups v2)
Uses Linux unified cgroups v2 (`/sys/fs/cgroup/`):
* **Memory Limit:** Enforces hard thresholds via `memory.max` with automated OOM termination upon breach.
* **Process Limit:** Prevents fork-bombs and runaway sub-processes using `pids.max`.
* **CPU Limit:** Throttles CPU quotas through `cpu.max`.

### 3. 🔒 Security & Filesystem Sandboxing
* **Filesystem Pivot:** Replaces insecure `chroot` with modern **`pivot_root`** and `MS_REC|MS_PRIVATE` unshares.
* **Read-Only Root Filesystem:** Protects underlying rootfs layers from tampering (`readonly_rootfs`).
* **Temporary Filesystem (tmpfs):** Configurable in-memory scratch space mounted at `/tmp` (`tmpfs_size_mb`).
* **Bind Mounts:** Safely mounts specified host directories into the container with optional read-only flags.
* **Linux Capability Dropping:** Drops superuser capabilities via `libcap`, retaining only strictly designated privileges (e.g. `CAP_NET_RAW`).
* **Seccomp Syscall Filtering:** BPF filter profiles blocking dangerous syscalls (such as `mount`, raw kernel ring operations, and module loading).

### 4. 🌐 Virtual Bridge Networking
* **Virtual Ethernet Pairs:** Generates point-to-point `veth` interfaces, moving the container end (`eth0`) into the container network namespace.
* **Software Bridge (`mun-bridge0`):** Manages an internal Linux bridge with configurable subnet (default `172.18.0.1/24`).
* **IP Allocation & Routing:** Assigns unique IPs (`172.18.0.x/24`) and sets the container default gateway.
* **NAT & Port Forwarding:** Automatically programs `iptables` NAT (`PREROUTING`, `POSTROUTING MASQUERADE`, and loopback `OUTPUT` rules).
* **DNS Resolution:** Automatically configures container DNS via `/etc/resolv.conf`.

### 5. 🎛️ Container Lifecycle & Interactive CLI
* **Foreground Execution:** `run` command with pseudo-terminal allocation (**PTY/TTY** via `-t / --tty`) and custom environment variables (`-e / --env`).
* **Background Daemonization:** `start` containers detached in the background with PID tracking in `.container_state`.
* **Lifecycle Management:** Dedicated commands to `stop`, `restart`, `list`, `remove`, `kill-all`, `prune`, and `cleanup`.
* **Hybrid Configuration Hierarchy:** Evaluates runtime settings in strict order:
  $$\text{Hardcoded Defaults} \longrightarrow \text{JSON Configuration} \longrightarrow \text{Environment Variables} \longrightarrow \text{CLI Flags}$$

---

## 📂 Project Directory Structure

```
Project-MNU-OS/
├── CMakeLists.txt              # CMake build configuration with libcap linking
├── README.md                   # Project documentation
├── usage.md                    # In-depth usage, lifecycle, and testing manual
├── plan.md                     # Initial design specifications
├── include/                    # Header files
│   ├── CgroupManager.h         # cgroups v2 resource controller
│   ├── Config.h                # Master configuration data structures
│   ├── ConfigParser.h          # JSON configuration parser
│   ├── NetworkManager.h        # veth, bridge, NAT, and DNS management
│   ├── Security.h              # pivot_root, capabilities, seccomp & user namespaces
│   ├── StateManager.h          # Daemon state persistence & tracking
│   ├── container.h             # Core container lifecycle engine & clone handler
│   └── nlohmann/               # Embedded modern JSON library
├── src/                        # Implementation files
│   ├── CgroupManager.cpp
│   ├── ConfigParser.cpp
│   ├── Container.cpp
│   ├── NetworkManager.cpp
│   ├── Security.cpp
│   ├── StateManager.cpp
│   └── main.cpp                # CLI entry point and dispatch logic
├── configs/                    # Ready-to-run container configuration presets
│   ├── alpine_test.json
│   ├── background.json         # Background daemon template
│   ├── basic.json              # Minimal container configuration
│   ├── example.json            # Full network & resource configuration
│   ├── full_security.json      # Hardened security preset (seccomp, caps, tmpfs)
│   ├── network_test.json       # Network verification configuration
│   └── shell.json              # Interactive shell profile
├── scripts/                    # Automation & helper utilities
│   ├── rootfs_setup.sh         # Automated Alpine mini-rootfs downloader & unpacker
│   ├── manage_containers.sh    # Lifecycle batch helper script
│   └── run_all_tests.sh        # Test suite execution harness
└── tests/                      # Automated milestone test scripts
    ├── stage1.sh               # Milestone 1: Process & isolation tests
    ├── stage2.sh               # Milestone 2: CLI & daemon lifecycle tests
    ├── stage3.sh               # Milestone 3: Security, capabilities & seccomp tests
    └── test_runner.py          # Python test runner
```

---

## 🛠️ Prerequisites & Installation

### System Requirements
* **Operating System:** Linux (Kernel 5.4 or higher recommended)
* **Kernel Configuration:** Control Groups v2 (`cgroup2` mounted on `/sys/fs/cgroup`)
* **Privileges:** Root / `sudo` (required to manipulate namespaces, cgroups, veth, and iptables)

### Dependencies
On Ubuntu / Debian systems:
```bash
sudo apt update
sudo apt install -y build-essential cmake g++ libcap-dev iptables iproute2 wget
```

### Build Instructions
```bash
# Clone the repository
git clone https://github.com/KAAZamapiloT/Project-MNU-OS.git
cd Project-MNU-OS

# Download and extract the Alpine Linux root filesystem
chmod +x scripts/rootfs_setup.sh
sudo ./scripts/rootfs_setup.sh

# Build the executable using CMake
mkdir -p build && cd build
cmake ..
make
cd ..
```
The compiled binary is generated at `build/mun_os`.

---

## 🚀 Usage Guide

### CLI Commands Syntax
```bash
sudo ./build/mun_os <command> [options]
```

| Command | Arguments | Description |
|---|---|---|
| `run` | `[options] [<cmd>...]` | Create and execute an isolated container in foreground. |
| `start` | `--config <path>` | Start a container in detached background mode. |
| `stop` | `<container_name>` | Stop a running container process (preserves state). |
| `restart`| `<container_name>` | Restart a stopped or active background container. |
| `list` | *(none)* | List all managed containers, PIDs, and running statuses. |
| `remove`| `<container_name>` | Remove a stopped container's state entry. |
| `kill-all` | *(none)* | Immediately stop all active running containers. |
| `prune` | *(none)* | Remove state entries for all stopped containers. |
| `cleanup` | *(none)* | Stop all running containers and remove all state. |

### CLI Options

| Flag | Parameter | Description |
|---|---|---|
| `--config` | `<path>` | Path to JSON container configuration file. |
| `--name` | `<string>` | Custom container instance name (overrides default). |
| `--rootfs` | `<path>` | Path to root filesystem directory. |
| `--hostname`| `<string>` | Hostname to apply inside the container. |
| `--memory` | `<MB>` | Maximum memory ceiling in Megabytes (cgroups v2). |
| `--pids` | `<count>` | Maximum number of concurrent processes permitted. |
| `-e`, `--env`| `<KEY=VALUE>` | Inject environment variable into container. |
| `-t`, `--tty`| *(flag)* | Allocate a pseudo-TTY for interactive terminal usage. |

---

## 💡 Configuration Examples

### 1. Interactive Shell (`configs/shell.json`)
```json
{
  "hostname": "interactive-shell",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "resources": {
    "memory_limit_mb": 256,
    "process_limit": 50
  }
}
```
Run with interactive TTY:
```bash
sudo ./build/mun_os run --config configs/shell.json -t /bin/sh
```

### 2. Full Security Hardening (`configs/full_security.json`)
```json
{
  "hostname": "secure-node",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "resources": {
    "memory_limit_mb": 128
  },
  "security": {
    "use_pivot_root": true,
    "readonly_rootfs": true,
    "drop_capabilities": true,
    "use_seccomp": true,
    "setup_tmpfs": true,
    "tmpfs_size_mb": 32,
    "bind_mounts": [
      {
        "source": "./shared_data",
        "target": "/shared"
      }
    ]
  }
}
```

### 3. Networked Container with Port Forwarding (`configs/example.json`)
```json
{
  "hostname": "web-server",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "security": {
    "use_pivot_root": false,
    "use_seccomp": true,
    "readonly_rootfs": false
  },
  "network": {
    "enable_networking": true,
    "bridge_name": "mun-bridge0",
    "container_ip": "172.18.0.2/24",
    "gateway_ip": "172.18.0.1/24",
    "enable_dns": true,
    "port_mappings": ["8080:80"]
  }
}
```

---

## 🧪 Testing & Verification

Project MNU-OS includes automated test scripts located in `tests/`:

```bash
# Test Core Isolation (Milestone 1: Namespaces, PID 1, cgroups limits)
sudo ./tests/stage1.sh

# Test CLI Lifecycle & Precedence (Milestone 2: start, stop, restart, list, rm)
sudo ./tests/stage2.sh

# Test Security Features (Milestone 3: pivot_root, capabilities, seccomp, tmpfs)
sudo ./tests/stage3.sh
```

---

## 📈 Roadmap & Milestone Status

| Phase | Milestone | Deliverable | Status |
|:---:|---|---|:---:|
| **Phase 1** | **Core Runtime Isolation** | PID, UTS, Mount namespaces & cgroups v2 resource quotas | ✅ **Completed** |
| **Phase 2** | **CLI & Daemon Lifecycle** | JSON config parser, CLI flag overrides, lifecycle management | ✅ **Completed** |
| **Phase 3** | **Security Hardening** | `pivot_root`, read-only root, tmpfs, capability drop, Seccomp BPF | ✅ **Completed** |
| **Phase 4** | **Virtual Networking** | Network namespaces, `veth` pair, bridge routing, NAT port-forwarding | ✅ **Completed** |
| **Phase 5** | **Image Layering & Distribution** | OCI rootfs images, overlayfs union mounting, tarball registry | 🚧 *Planned* |
| **Phase 6** | **Runtime Optimizations** | Multi-threaded event loop, metrics exporter & health monitoring | 🚧 *Planned* |

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](https://github.com/KAAZamapiloT/Project-MNU-OS/issues).

---

## 📜 License

This project is open source and available under the **MIT License**.
