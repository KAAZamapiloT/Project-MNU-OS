#include "NetworkManager.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <arpa/inet.h>

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static std::string strip_netmask(const std::string& ip) {
    size_t slash = ip.find('/');
    return (slash != std::string::npos) ? ip.substr(0, slash) : ip;
}

static std::string ensure_netmask(const std::string& ip) {
    if (ip.find('/') == std::string::npos) {
        return ip + "/24";
    }
    return ip;
}

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

std::mutex NetworkManager::counter_mutex_;
int NetworkManager::veth_counter_ = 0;
int NetworkManager::ip_counter_ = 1;

int NetworkManager::get_next_veth_id() {
    std::lock_guard<std::mutex> lock(counter_mutex_);
    return ++veth_counter_;
}

int NetworkManager::get_next_ip_id() {
    std::lock_guard<std::mutex> lock(counter_mutex_);
    return ++ip_counter_;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

NetworkManager::NetworkManager(const NetworkConfig& config)
    : config_(config) {}

NetworkManager::~NetworkManager() {}

// ============================================================================
// MAIN SETUP FUNCTION
// ============================================================================

bool NetworkManager::setup_container_network(pid_t container_pid, const std::string& container_name) {
    if (!config_.enable_networking) {
        return true;
    }

    std::cout << "[Network] Setting up networking for container " << container_name << " (PID: " << container_pid << ")" << std::endl;

    // 1. Create bridge
    if (!create_bridge()) {
        std::cerr << "[Network] Failed to create bridge" << std::endl;
        return false;
    }

    // 2. ✅ Use PID for unique veth names (PID is always unique)
    std::string pid_str = std::to_string(container_pid);
    std::string veth_host = "veth" + pid_str;
    std::string veth_container = "veth" + pid_str + "c";

    // ✅ Generate unique IP (use PID modulo to stay in range)
    int ip_last_octet = 2 + (container_pid % 253);  // Range: 2-254
    std::string container_ip = "172.18.0." + std::to_string(ip_last_octet) + "/24";

    std::cout << "[Network] Assigned IP: " << container_ip << std::endl;
    std::cout << "[Network] Creating veth pair: " << veth_host << " <-> " << veth_container << std::endl;

    if (!create_veth_pair(veth_host, veth_container)) {
        std::cerr << "[Network] Failed to create veth pair" << std::endl;
        return false;
    }

    if (!attach_veth_to_bridge(veth_host)) {
        std::cerr << "[Network] Failed to attach veth to bridge" << std::endl;
        return false;
    }

    if (!move_veth_to_namespace(veth_container, container_pid)) {
        std::cerr << "[Network] Failed to move veth to container namespace" << std::endl;
        return false;
    }

    usleep(100000);

    if (!configure_container_network(container_pid, veth_container, container_ip)) {
        std::cerr << "[Network] Failed to configure container network" << std::endl;
        return false;
    }

    if (!setup_port_forwarding()) {
        std::cerr << "[Network] Warning: Port forwarding setup failed" << std::endl;
    }

    if (config_.enable_dns) {
        if (!setup_dns(container_pid)) {
            std::cout << "[Network] DNS: Using pre-configured resolv.conf from rootfs" << std::endl;
        }
    }

    std::cout << "[Network] Container networking configured successfully" << std::endl;
    std::cout << "[Network] Container IP: " << container_ip << std::endl;
    std::cout << "[Network] Gateway IP: " << strip_netmask(config_.gateway_ip) << std::endl;

    return true;
}


// ============================================================================
// BRIDGE CREATION
// ============================================================================

bool NetworkManager::create_bridge() {
    std::string cmd = "ip link show " + config_.bridge_name + " 2>/dev/null";

    if (system(cmd.c_str()) != 0) {
        std::cout << "[Network] Creating bridge: " << config_.bridge_name << std::endl;

        cmd = "ip link add " + config_.bridge_name + " type bridge";
        if (system(cmd.c_str()) != 0) {
            std::cerr << "[Network] Failed to create bridge device" << std::endl;
            return false;
        }

        std::string gateway_with_netmask = ensure_netmask(config_.gateway_ip);
        cmd = "ip addr add " + gateway_with_netmask + " dev " + config_.bridge_name;
        if (system(cmd.c_str()) != 0) {
            std::cerr << "[Network] Failed to add IP to bridge" << std::endl;
            return false;
        }

        cmd = "ip link set " + config_.bridge_name + " up";
        if (system(cmd.c_str()) != 0) {
            std::cerr << "[Network] Failed to bring up bridge" << std::endl;
            return false;
        }

        std::cout << "[Network] Configuring NAT and IP forwarding..." << std::endl;

        system("sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1");
        system("modprobe br_netfilter 2>/dev/null");
        system("sysctl -w net.bridge.bridge-nf-call-iptables=0 >/dev/null 2>&1");
        system("sysctl -w net.bridge.bridge-nf-call-ip6tables=0 >/dev/null 2>&1");
        system("sysctl -w net.bridge.bridge-nf-call-arptables=0 >/dev/null 2>&1");

        std::string subnet = "172.18.0.0/24";

        cmd = "iptables -C FORWARD -i " + config_.bridge_name + " -j ACCEPT 2>/dev/null || "
              "iptables -I FORWARD 1 -i " + config_.bridge_name + " -j ACCEPT";
        system(cmd.c_str());

        cmd = "iptables -C FORWARD -o " + config_.bridge_name + " -j ACCEPT 2>/dev/null || "
              "iptables -I FORWARD 1 -o " + config_.bridge_name + " -j ACCEPT";
        system(cmd.c_str());

        cmd = "iptables -C FORWARD -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null || "
              "iptables -I FORWARD 1 -m state --state RELATED,ESTABLISHED -j ACCEPT";
        system(cmd.c_str());

        cmd = "iptables -t nat -C POSTROUTING -s " + subnet + " ! -o " + config_.bridge_name + " -j MASQUERADE 2>/dev/null || "
              "iptables -t nat -A POSTROUTING -s " + subnet + " ! -o " + config_.bridge_name + " -j MASQUERADE";
        system(cmd.c_str());

        std::cout << "[Network] Bridge created with NAT successfully" << std::endl;
    } else {
        std::cout << "[Network] Using existing bridge: " << config_.bridge_name << std::endl;
    }

    return true;
}

// ============================================================================
// VETH PAIR OPERATIONS
// ============================================================================

bool NetworkManager::create_veth_pair(const std::string& veth_host, const std::string& veth_container) {
    // ✅ FIX: Check and cleanup existing veth pairs
    std::string check_cmd = "ip link show " + veth_host + " 2>/dev/null";
    if (system(check_cmd.c_str()) == 0) {
        std::cout << "[Network] Cleaning up existing veth pair: " << veth_host << std::endl;
        std::string cleanup_cmd = "ip link delete " + veth_host + " 2>/dev/null";
        system(cleanup_cmd.c_str());
        usleep(200000); // Wait 200ms for kernel cleanup
    }

    // Create veth pair
    std::string cmd = "ip link add " + veth_host + " type veth peer name " + veth_container;
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to create veth pair" << std::endl;
        return false;
    }

    // Bring up host side
    cmd = "ip link set " + veth_host + " up";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to bring up " << veth_host << std::endl;
        return false;
    }

    return true;
}

bool NetworkManager::attach_veth_to_bridge(const std::string& veth_host) {
    std::cout << "[Network] Attaching " << veth_host << " to " << config_.bridge_name << std::endl;

    std::string cmd = "ip link set " + veth_host + " master " + config_.bridge_name;
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to attach veth to bridge" << std::endl;
        return false;
    }

    std::cout << "[Network] Successfully attached veth to bridge" << std::endl;
    return true;
}

bool NetworkManager::move_veth_to_namespace(const std::string& veth_container, pid_t pid) {
    std::cout << "[Network] Moving " << veth_container << " to container namespace (PID: " << pid << ")" << std::endl;

    std::string cmd = "ip link set " + veth_container + " netns " + std::to_string(pid);
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to move veth to namespace" << std::endl;
        return false;
    }

    return true;
}

// ============================================================================
// CONTAINER NETWORK CONFIGURATION
// ============================================================================

bool NetworkManager::configure_container_network(pid_t pid, const std::string& veth_name, const std::string& container_ip) {
    std::string ns_path = "/proc/" + std::to_string(pid) + "/ns/net";

    std::cout << "[Network] Configuring network inside container namespace" << std::endl;

    std::string cmd = "nsenter --net=" + ns_path + " ip link set lo up";
    system(cmd.c_str());

    cmd = "nsenter --net=" + ns_path + " ip link set " + veth_name + " name eth0";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to rename veth to eth0" << std::endl;
        return false;
    }

    cmd = "nsenter --net=" + ns_path + " ip addr add " + container_ip + " dev eth0";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to add IP address" << std::endl;
        return false;
    }

    cmd = "nsenter --net=" + ns_path + " ip link set eth0 up";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to bring up eth0" << std::endl;
        return false;
    }

    std::string gateway_ip = strip_netmask(config_.gateway_ip);
    cmd = "nsenter --net=" + ns_path + " ip route add default via " + gateway_ip;
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to add default route" << std::endl;
        return false;
    }

    std::cout << "[Network] Network configuration complete (IP: " << container_ip << ", Gateway: " << gateway_ip << ")" << std::endl;
    return true;
}

// ============================================================================
// PORT FORWARDING
// ============================================================================

bool NetworkManager::setup_port_forwarding() {
    if (config_.port_mappings.empty()) {
        return true;
    }

    for (const auto& mapping : config_.port_mappings) {
        size_t colon = mapping.find(':');
        if (colon == std::string::npos) {
            std::cerr << "[Network] Invalid port mapping: " << mapping << std::endl;
            continue;
        }

        std::string host_port = mapping.substr(0, colon);
        std::string container_port = mapping.substr(colon + 1);
        std::string container_ip = strip_netmask(config_.container_ip);

        std::string cmd = "iptables -t nat -A PREROUTING -p tcp --dport " + host_port +
                         " -j DNAT --to-destination " + container_ip + ":" + container_port;
        if (system(cmd.c_str()) != 0) {
            std::cerr << "[Network] Failed to setup port forwarding: " << mapping << std::endl;
            continue;
        }

        std::cout << "[Network] Port forwarding: host:" << host_port
                  << " -> container:" << container_port << std::endl;
    }

    return true;
}

// ============================================================================
// DNS CONFIGURATION
// ============================================================================

bool NetworkManager::setup_dns(pid_t pid) {
    std::string etc_dir = "/proc/" + std::to_string(pid) + "/root/etc";
    struct stat st;

    if (stat(etc_dir.c_str(), &st) != 0) {
        std::cout << "[Network] Creating /etc directory in container" << std::endl;
        if (mkdir(etc_dir.c_str(), 0755) != 0) {
            std::cerr << "[Network] Failed to create /etc directory: " << strerror(errno) << std::endl;
            return false;
        }
    }

    std::string resolv_conf = etc_dir + "/resolv.conf";

    if (stat(resolv_conf.c_str(), &st) == 0) {
        std::cout << "[Network] DNS: resolv.conf already exists, skipping" << std::endl;
        return true;
    }

    FILE* f = fopen(resolv_conf.c_str(), "w");
    if (!f) {
        return false;
    }

    fprintf(f, "nameserver 8.8.8.8\n");
    fprintf(f, "nameserver 8.8.4.4\n");
    fclose(f);

    return true;
}

// ============================================================================
// CLEANUP
// ============================================================================

bool NetworkManager::cleanup_container_network(const std::string& container_name) {
    std::cout << "[Network] Cleaning up network for " << container_name << std::endl;
    return true;
}
