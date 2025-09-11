Got it 👍 — here’s a clean **`PLAN.md`** file you can drop into your repo alongside the `README.md`.
It’s team-oriented, file-specific, and sprint-based so everyone knows what to do and how the pieces fit.

---

# 📌 Project MNU-OS — Development Plan

This document defines the **development roadmap, file ownership, and sprint workflow** for **MNU-OS**, a minimal Linux container runtime in **C++17**.

---

## 👥 Team Division

### **Person A – CLI & Config (User Interaction)**

**Goal:** Handle input parsing, command-line interface, and JSON config → runtime spec.
**Files:**

* `include/cli.hpp` → CLI declarations
* `src/cli.cpp` → CLI argument parser (subcommands like `run`, `stop`)
* `include/config.hpp` → Config structures
* `src/config.cpp` → JSON parser (nlohmann/json)
* `configs/sample.json` → Example config file
* Update **README.md** (usage examples)

---

### **Person B – Core Container Runtime (Namespaces + Exec)**

**Goal:** Implement actual container creation & execution.
**Files:**

* `include/container.hpp` → ContainerSpec + run() declaration
* `src/container.cpp` → Core runtime (`clone()`, `sethostname`, `execvp`)
* `include/filesystem.hpp` → Helpers for chroot/pivot\_root, mount ops
* `src/filesystem.cpp` → Implements chroot, `/proc` mount, bind mounts
* `src/main.cpp` → Entry point (connects CLI → Config → Runtime)

---

### **Person C – Resource Control & Utilities (cgroups + Logging)**

**Goal:** Manage resource limits via cgroups v2 and add debugging/logging utilities.
**Files:**

* `include/cgroups.hpp` → Cgroup interface
* `src/cgroups.cpp` → Implements memory, CPU, pids limits
* `include/logger.hpp` → Logging macros (INFO, WARN, ERROR)
* `src/logger.cpp` → Logging implementation
* `CMakeLists.txt` → Keep build up-to-date with all modules

---

## 📂 Project Structure

```
MNU-OS/
├── PLAN.md                 # Development plan (this file)
├── README.md               # Project overview
├── CMakeLists.txt          # Build config
├── configs/
│   └── sample.json         # Example container config
├── include/
│   ├── cli.hpp
│   ├── config.hpp
│   ├── container.hpp
│   ├── cgroups.hpp
│   ├── filesystem.hpp
│   └── logger.hpp
└── src/
    ├── main.cpp
    ├── cli.cpp
    ├── config.cpp
    ├── container.cpp
    ├── cgroups.cpp
    ├── filesystem.cpp
    └── logger.cpp
```

---

## 🏗️ Sprint Plan

### **Sprint 1 – Core Isolation (Week 1)**

* ✅ Setup repo + CMake
* ✅ Implement `clone()` + PID/UTS/Mount namespaces
* ✅ Basic `chroot` + mount `/proc`
* ✅ Launch a command inside container

**Deliverable:**
Run:

```bash
sudo ./mnu-os /bin/bash
```

→ isolated shell with new PID namespace, hostname, and /proc

---

### **Sprint 2 – CLI & Config (Week 2)**

* ✅ CLI parser (`run --config`)
* ✅ JSON config reader (`rootfs`, `hostname`, `cmd`, `limits`)
* ✅ Pass config into runtime

**Deliverable:**

```bash
sudo ./mnu-os run --config ../configs/sample.json
```

→ container launched using config file

---

### **Sprint 3 – Resource Control (Week 3)**

* ✅ Implement cgroups v2 memory/cpu/pids limits
* ✅ Apply limits to container processes
* ✅ Add simple logging system

**Deliverable:**
Run container with enforced resource limits.
Check with:

```bash
cat /sys/fs/cgroup/mnu-os/<id>/memory.current
```

---

## 🔄 Collaboration Workflow

1. **Branch per feature** (`cli-dev`, `runtime-dev`, `cgroups-dev`).
2. Commit **stubs first** (headers + TODOs) → integration easier.
3. PRs merged weekly into `main` after sprint demos.
4. Keep integration tested in `src/main.cpp`.

---

## 🎯 Milestone Roadmap

| Phase     | Focus                                         | Owner(s) |
| --------- | --------------------------------------------- | -------- |
| Sprint 1  | Basic isolation (`clone`, `chroot`, `/proc`)  | Person B |
| Sprint 2  | CLI + Config system                           | Person A |
| Sprint 3  | Cgroups v2 + Logging                          | Person C |
| Sprint 4+ | Security (pivot\_root, seccomp, capabilities) | All      |
| Sprint 5+ | Networking (veth, bridge, port mapping)       | All      |

---

## 📋 Notes

* Root privileges required for namespaces + cgroups.
* Use `debootstrap`/`apk` to prepare rootfs in `configs/sample.json`.
* Debug with `unshare`, `nsenter`, and `/proc/<pid>/status`.
* Future: replace `chroot` with `pivot_root`, add user namespaces.

---

👉 Do you also want me to generate **stub code for all `.hpp` and `.cpp` files** (with just function signatures + TODOs for each person) so your team can immediately clone & start coding?
