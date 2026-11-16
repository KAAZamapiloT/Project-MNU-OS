# Project MUN-OS: Comprehensive Usage & Testing Guide
**Version:** 2.0
**Date:** October 8, 2025

This document provides a comprehensive guide on how to build, run, and test all features of the **mun_os** container runtime, including the full container lifecycle and the hybrid configuration system.

---

## 1. Prerequisites

### Required Components
- **Compiled Executable:** `build/mun_os`
- **Prepared Root Filesystem:** `rootfs/` directory created by `scripts/setup_rootfs.sh`
- **JSON Library:** `include/nlohmann/json.hpp`
- **Example Configurations:** Create a `configs/` directory with the following files:

---

### `configs/shell.json`
```json
{
  "hostname": "interactive-shell",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "resources": { "memory_limit_mb": 512, "process_limit": 50 }
}
```

### `configs/background.json`
```json
{
  "hostname": "background-process",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["300"]
}
```

---

## 2. Building the Project

If you have made any code changes, rebuild from your project’s root directory:

```bash
# Clean previous build (optional but recommended after major changes)
rm -rf build

# Rebuild
mkdir -p build && cd build
cmake ..
make
cd ..
```

---

## 3. Usage Syntax & Configuration

The runtime supports a **hybrid configuration model** with the following order of precedence:

1. Hardcoded default values
2. JSON file (`--config <path>`)
3. Environment Variables (e.g., `MUN_OS_MEMORY_LIMIT`)
4. Command-Line Flags (e.g., `--memory <mb>`)

Run This Command for enabling DNS
'''
# Create /etc directory in rootfs
mkdir -p ./rootfs/etc

# Create resolv.conf with DNS servers
cat > ./rootfs/etc/resolv.conf << EOF
nameserver 8.8.8.8
nameserver 8.8.4.4
EOF

# Verify it was created
cat ./rootfs/etc/resolv.conf
ls -la ./rootfs/etc/
'''
---

### Supported Commands

| Command | Description |
|----------|--------------|
| `run` | Run a container in the foreground |
| `start` | Start a container in the background |
| `list` | List all managed containers |
| `stop` | Stop a running container (preserves state) |
| `restart` | Restart a stopped or running container |
| `rm` | Remove a stopped container completely |

---

## 4. Test Procedures

### **Test 1: Foreground Execution (`run` command)**

**Purpose:** Verify `run` command and core namespace isolation.

**Command:**
```bash
sudo ./build/mun_os run --config configs/shell.json
```

You will be dropped into an **interactive shell** inside the container.

#### Verifications (inside container):
- **PID Isolation:**
  ```bash
  ps aux
  ```
  Expected: `/bin/sh` should have PID 1.

- **UTS Isolation:**
  ```bash
  hostname
  ```
  Expected: Prints `interactive-shell`.

- **Mount Isolation:**
  ```bash
  ls /
  ```
  Expected: Lists contents of the `rootfs` directory.

Exit with:
```bash
exit
```

---

### **Test 2: Full Container Lifecycle (`start`, `list`, `stop`)**

**Purpose:** Verify complete lifecycle management for background containers.

1. **Start the container**
   ```bash
   sudo ./build/mun_os start --config configs/background.json
   ```
   Expected: Command returns immediately.

2. **List containers**
   ```bash
   ./build/mun_os list
   ```
   **Expected Output:**
   ```
   CONTAINER NAME       PID        STATUS     CONFIG
   -------------------- ---------- ---------- ----------------------------------------------------
   background           12345      running    /home/user/Project-MUN-OS/configs/background.json
   ```

3. **Stop the container**
   ```bash
   sudo ./build/mun_os stop background
   ```
   Expected: Message confirming the stop.

4. **Verify stopped status**
   ```bash
   ./build/mun_os list
   ```
   **Expected Output:**
   ```
   CONTAINER NAME       PID        STATUS     CONFIG
   -------------------- ---------- ---------- ----------------------------------------------------
   background           12345      stopped    /home/user/Project-MUN-OS/configs/background.json
   ```

---

### **Test 3: Restart and Removal (`restart`, `rm`)**

**Purpose:** Verify restart and permanent removal of a container.

1. **Restart container**
   ```bash
   sudo ./build/mun_os restart background
   ```
   Expected: Confirmation message with a **new PID**.

2. **Verify restart**
   ```bash
   ./build/mun_os list
   ```
   Expected: `background` container listed as **running** with a new PID.

3. **Stop and remove**
   ```bash
   sudo ./build/mun_os stop background
   sudo ./build/mun_os rm background
   ```
   Expected: Messages confirming both stop and removal.

4. **Final verification**
   ```bash
   ./build/mun_os list
   ```
   **Expected Output:**
   ```
   CONTAINER NAME       PID        STATUS     CONFIG
   -------------------- ---------- ---------- --------------------
   No containers are managed. Use 'start' to create one.
   ```

---

### **Test 4: Configuration Precedence**

**Purpose:** Confirm that CLI flags and environment variables correctly override JSON configuration.

1. **Run command with overrides**
   ```bash
   export MUN_OS_MEMORY_LIMIT=128
   sudo ./build/mun_os run --config configs/shell.json --memory 64 /bin/dd if=/dev/zero of=/dev/null bs=1M count=100
   ```

2. **Expected Behavior:**
   - The `dd` process will be **killed** for exceeding the 64MB memory limit.
   - Confirms **CLI flag > Environment variable > JSON file** precedence.

3. **Cleanup:**
   ```bash
   unset MUN_OS_MEMORY_LIMIT
   ```

---

**End of Document**
