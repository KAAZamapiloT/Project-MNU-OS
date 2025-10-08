# Project MNU-OS

**Project MNU-OS** is a minimal, command-line container runtime for Linux, built in **C++17**, designed to create isolated process environments using fundamental Linux kernel features.

It acts as a lightweight container manager — creating and running sandboxed environments where applications can execute securely, with limited access to system resources.

---

## 🚀 Key Objectives

The goal of **MNU-OS** is to provide a foundation for learning and experimenting with containerization.
It can:

- **Process Isolation**  
  Leverage Linux **Namespaces** (`PID`, `Mount`, `UTS`, etc.) to isolate processes, hostnames, and filesystems.

- **Resource Control**  
  Use **Control Groups (cgroups v2)** to enforce **memory**, **CPU**, and **process count** limits.

- **Sandboxed Filesystem**  
  Provide a **separate root filesystem** for processes via `chroot` and mount namespaces, isolating containers from the host.

- **Command-Line Interface (CLI)**  
  A simple CLI to launch containers from a **user-defined configuration**.

---

## 🛠️ Core Technologies

- **Programming Language**: C++17
- **Operating System**: Linux
- **Kernel Features Used**:
  - Linux **Namespaces**
  - Linux **Control Groups (cgroups v2)**
- **Build System**: [CMake](https://cmake.org/)
- **Interface**: Command-Line Interface (CLI)

---

## 🏗️ What It Does (In Simple Terms)

Think of your operating system as a **large apartment building**.
**Project MNU-OS** is the **building manager** that creates new private apartments for applications to live in:

- 🧱 **Namespaces = Walls**  
  Put up walls so an application can't see or interfere with its neighbors.

- ⚡ **cgroups = Utility Meters**  
  Install meters to ensure no application uses too much electricity or water (CPU, RAM, processes).

- 🚪 **Filesystem Isolation = Private Rooms**  
  Give each application its own set of rooms (root filesystem), completely separated from the host.

Once the apartment is ready, the application moves in and does its work in a secure, private space.

---

## 📂 Project Structure

```
MNU-OS/
├── src/                    # Source code
├── include/                # Header files
├── cmake/                  # CMake modules
├── configs/                # Example container configuration files
├── CMakeLists.txt
└── README.md
```

---

## ⚡ Getting Started

### Prerequisites

- Linux system with **namespace** and **cgroup v2** support
- Root privileges (required for namespace + cgroup operations)
- Build tools: `g++`, `cmake`, `make`

### Build

```bash
git clone https://github.com/yourusername/MNU-OS.git
cd MNU-OS
mkdir build && cd build
cmake ..
make
```

### Usage

Run an isolated containerized command:

```bash
sudo ./mnu-os run --config ../configs/sample.json
```

This will:
1. Create new namespaces (PID, UTS, Mount).
2. Set up a new hostname.
3. Apply filesystem isolation with `chroot`.
4. Enforce **memory / CPU / process limits** via cgroups.
5. Launch the specified application.

---

## 🛣️ Roadmap

### Phase 1: Core Foundation ✅
- [ ] **Basic Process Isolation**
  - [ ] Implement PID namespace isolation
  - [ ] Implement Mount namespace isolation
  - [ ] Implement UTS namespace isolation
- [ ] **Resource Management**
  - [ ] Set up cgroups v2 integration
  - [ ] Implement memory limits
  - [ ] Implement CPU limits
  - [ ] Implement process count limits
- [ ] **Filesystem Isolation**
  - [ ] Basic chroot implementation
  - [ ] Mount namespace setup
  - [ ] Root filesystem isolation

### Phase 2: Configuration & CLI ✅
- [ ] **Configuration System**
  - [ ] JSON configuration parser
  - [ ] Environment variable handling
  - [ ] Resource limit configuration
- [ ] **Enhanced CLI Interface**
  - [ ] Container lifecycle management (start, stop, restart)
  - [ ] Container listing and status
  - [ ] Interactive container shell access
  - [ ] Configuration validation

### Phase 3: Security Enhancements 🔄
- [ ] **Advanced Filesystem Security**
  - [ ] Replace chroot with pivot_root
  - [ ] Read-only filesystem support
  - [ ] Bind mount management
  - [ ] Temporary filesystem (tmpfs) integration
- [ ] **User Security**
  - [ ] User namespace implementation
  - [ ] Non-root container execution
  - [ ] Capability dropping
  - [ ] Seccomp profile support

### Phase 4: Networking & Communication 📡
- [ ] **Network Isolation**
  - [ ] Network namespace implementation
  - [ ] Virtual ethernet (veth) pair creation
  - [ ] Basic bridge networking
  - [ ] Container-to-container communication
- [ ] **Port Management**
  - [ ] Port forwarding from host to container
  - [ ] Network policy enforcement
  - [ ] DNS resolution in containers

### Phase 5: Advanced Features 🎯
- [ ] **Logging & Monitoring**
  - [ ] Container output logging
  - [ ] Resource usage monitoring
  - [ ] Event logging system
  - [ ] Performance metrics collection
- [ ] **Image Management**
  - [ ] Simple image layer system
  - [ ] Container image creation
  - [ ] Image registry integration
  - [ ] Dockerfile-like build system

### Phase 6: Performance & Scalability 🚀
- [ ] **Performance Optimizations**
  - [ ] Startup time improvements
  - [ ] Memory usage optimization
  - [ ] Multi-threading support
  - [ ] Async I/O operations
- [ ] **Enterprise Features**
  - [ ] Container orchestration basics
  - [ ] Health check system
  - [ ] Auto-restart policies
  - [ ] Load balancing support

---

### 🎯 Milestone Timeline

| Phase | Target Completion | Key Deliverable |
|-------|------------------|-----------------|
| Phase 1 | ✅ **Completed** | Core container runtime |
| Phase 2 | ✅ **Completed** | Production-ready CLI |
| Phase 3 | **Q3 2025** | Security hardened |
| Phase 4 | **Q4 2025** | Network isolation |
| Phase 5 | **Q1 2026** | Monitoring & images |
| Phase 6 | **Q2 2026** | Performance & scale |

---

### 🔧 Technical Debt & Improvements

- [ ] **Code Quality**
  - [ ] Add comprehensive unit tests
  - [ ] Implement integration tests
  - [ ] Add code coverage reporting
  - [ ] Static analysis integration
- [ ] **Documentation**
  - [ ] API documentation
  - [ ] Architecture documentation
  - [ ] Contribution guidelines
  - [ ] Security best practices guide
- [ ] **Development Experience**
  - [ ] Docker development environment
  - [ ] Continuous integration pipeline
  - [ ] Automated releases
  - [ ] Package distribution

---

### 📋 Known Limitations & Future Work

| Limitation | Priority | Planned Solution |
|------------|----------|------------------|
| No network isolation | High | Phase 4: Network namespaces |
| Basic chroot security | High | Phase 3: pivot_root implementation |
| No image management | Medium | Phase 5: Layer system |
| Single-threaded execution | Low | Phase 6: Multi-threading |
| No orchestration | Low | Phase 6: Basic orchestration |

---


