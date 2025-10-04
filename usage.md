# Project MUN-OS: Testing & Usage Guide
**Version:** 1.0  
**Date:** October 4, 2025  

This document provides a comprehensive guide on how to build, run, and test the core features of the `mun_os` container runtime. These tests will verify that both isolation (namespaces) and resource limiting (cgroups) are working correctly.

---

## 1. Prerequisites
Before running the tests, ensure you have the following ready in your **Project-MNU-OS** directory:

- **A Compiled Executable**: Your `mun_os` binary should be present in the `build/` directory.  
- **A Prepared Root Filesystem**: A `rootfs/` directory containing a minimal Linux distribution (e.g., Alpine Linux).  
- **Sudo Privileges**: Creating namespaces and managing cgroups requires root access.  

---

## 2. Building the Project
If you have made any code changes, rebuild the project from your project's root directory:

```bash
# Navigate to the build directory (or create it)
mkdir -p build && cd build

# Configure with CMake and compile with make
cmake ..
make

# Navigate back to the project root
cd ..
```

---

## 3. Basic Usage Syntax
The general command to run a container is:

```bash
sudo ./build/mun_os [options] <command> [args...]
```

### Available Options
- `--rootfs <path>`: **Required.** Path to the container's root filesystem.  
- `--memory <mb>`: Optional. Memory limit in megabytes.  
- `--pids <limit>`: Optional. Maximum number of processes.  

---

## 4. Test Procedures

### **Test 1: Verifying Core Isolation (Namespaces)**
This test confirms that the **PID**, **UTS**, and **Mount** namespaces are isolating the container from the host.

1. **Command:**
```bash
sudo ./build/mun_os --rootfs ./rootfs /bin/sh
```

You will now be inside the container, indicated by a `#` prompt.

2. **Verifications (inside the container shell):**

- **Check PID Isolation:**
```bash
ps aux
```
**Expected Output:** You should only see one or two processes.  
Your shell (`/bin/sh`) will have PID **1**, proving the PID namespace is working.

- **Check UTS (Hostname) Isolation:**
```bash
hostname
```
**Expected Output:** It should print `mun-os-container`, proving the UTS namespace is working.

- **Check Mount Isolation:**
```bash
ls /
```
**Expected Output:** You should see files and folders from your `rootfs` directory (`bin`, `etc`, `lib`, etc.), not your host machine’s filesystem.  
This proves the Mount namespace and `chroot` are working.

3. **Exit:**
```bash
exit
```

---

### **Test 2: Running a Custom Binary**
This test confirms that custom, statically-compiled programs can be executed within the container. (`star_pattern` binary is already in `rootfs/bin`).

1. **Command:**
```bash
sudo ./build/mun_os --rootfs ./rootfs /bin/star_pattern
```

2. **Expected Output:**
```
--- Running Custom Star Pattern Program ---
* * * * * * * * * * * * * * * -----------------------------------------
```
This proves the program executed successfully within the isolated filesystem.

---

### **Test 3: Verifying Memory Limit (Memory Cgroup)**
This test confirms that the **CgroupManager** can successfully limit the container's memory usage.  
We will run a command that tries to allocate more memory than allowed and observe the kernel kill it.

1. **Command:**
```bash
sudo ./build/mun_os --rootfs ./rootfs --memory 20 /bin/dd if=/dev/zero of=/dev/null bs=1M count=30
```

2. **Expected Output:**
```
[Main] Starting container...
[CgroupManager] Cgroup setup complete at /sys/fs/cgroup/mun-os-container
[CgroupManager] Applied cgroup to PID ...
Killed
[Main] Container finished with exit code: -1
[CgroupManager] Cgroup teardown complete.
```

This proves your **memory cgroup** is working perfectly.

---

### **Test 4: Verifying Process Limit (PID Cgroup)**
This test confirms that the **CgroupManager** can limit the number of processes a container can create, preventing a "fork bomb".

1. **Command:**
```bash
sudo ./build/mun_os --rootfs ./rootfs --pids 5 /bin/sh
```

2. **Verifications (inside the container shell):**

- Attempt a fork bomb with:
```bash
# bomb() { bomb | bomb & }; bomb
```

**Expected Output:**  
The shell will create a few processes but will quickly fail once it hits the cgroup limit of **5 processes**.  
You will see an error message like:

```
/bin/sh: can't fork: Resource temporarily unavailable
```

This proves the **PID cgroup** is working correctly.

---
