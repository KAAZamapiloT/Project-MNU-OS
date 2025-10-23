#include "NetworkManager.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>   // ← ADD THIS
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <arpa/inet.h>

// Initialize static members
std::mutex NetworkManager::counter_mutex_;
int NetworkManager::veth_counter_ = 0;

int NetworkManager::get_next_veth_id() {
    std::lock_guard<std::mutex> lock(counter_mutex_);
    return ++veth_counter_;
}

NetworkManager::NetworkManager(const NetworkConfig& config)
    : config_(config) {}

NetworkManager::~NetworkManager() {}

bool NetworkManager::setup_container_network(pid_t container_pid, const std::string& container_name) {
    if (!config_.enable_networking) {
        return true;
    }

    std::cout << "[Network] Setting up networking for container " << container_name << std::endl;

    // 1. Create bridge (if not exists)
    if (!create_bridge()) {
        std::cerr << "[Network] Failed to create bridge" << std::endl;
        return false;
    }

    // 2. Create veth pair
    int veth_id = get_next_veth_id();
    std::string veth_host = "veth" + std::to_string(veth_id);
    std::string veth_container = "veth0";

    std::cout << "[Network] Creating veth pair: " << veth_host << " <-> " << veth_container << std::endl;

    if (!create_veth_pair(veth_host, veth_container)) {
        std::cerr << "[Network] Failed to create veth pair" << std::endl;
        return false;
    }

    // 3. Attach host veth to bridge
    if (!attach_veth_to_bridge(veth_host)) {
        std::cerr << "[Network] Failed to attach veth to bridge" << std::endl;
        return false;
    }

    // 4. Move container veth to container's network namespace
    if (!move_veth_to_namespace(veth_container, container_pid)) {
        std::cerr << "[Network] Failed to move veth to container namespace" << std::endl;
        return false;
    }

    // Small delay to ensure namespace setup
    usleep(100000); // 100ms

    // 5. Configure container network
    if (!configure_container_network(container_pid, veth_container)) {
        std::cerr << "[Network] Failed to configure container network" << std::endl;
        return false;
    }

    // 6. Setup port forwarding
    if (!setup_port_forwarding()) {
        std::cerr << "[Network] Warning: Port forwarding setup failed" << std::endl;
    }

    // 7. Setup DNS
    if (config_.enable_dns) {
        if (!setup_dns(container_pid)) {
            std::cerr << "[Network] Warning: DNS setup failed" << std::endl;
        }
    }

    std::cout << "[Network] Container networking configured successfully" << std::endl;
    std::cout << "[Network] Container IP: " << config_.container_ip << std::endl;
    std::cout << "[Network] Gateway IP: " << config_.gateway_ip << std::endl;

    return true;
}

bool NetworkManager::create_bridge() {
    std::string cmd = "ip link show " + config_.bridge_name + " 2>/dev/null";

    if (system(cmd.c_str()) != 0) {
        std::cout << "[Network] Creating bridge: " << config_.bridge_name << std::endl;

        cmd = "ip link add " + config_.bridge_name + " type bridge";
        if (system(cmd.c_str()) != 0) {
            return false;
        }

        std::string gateway_with_netmask = config_.gateway_ip;
        if (gateway_with_netmask.find('/') == std::string::npos) {
            gateway_with_netmask += "/24";  // Add /24 if not present
        }
        cmd = "ip addr add " + gateway_with_netmask + " dev " + config_.bridge_name;
        system(cmd.c_str());

        cmd = "ip link set " + config_.bridge_name + " up";
        if (system(cmd.c_str()) != 0) {
            return false;
        }

        std::cout << "[Network] Configuring NAT and IP forwarding..." << std::endl;

        // Enable IP forwarding
        system("sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1");

        // ✅ CRITICAL FIX: Disable bridge netfilter
        system("modprobe br_netfilter 2>/dev/null");
        system("sysctl -w net.bridge.bridge-nf-call-iptables=0 >/dev/null 2>&1");
        system("sysctl -w net.bridge.bridge-nf-call-ip6tables=0 >/dev/null 2>&1");
        system("sysctl -w net.bridge.bridge-nf-call-arptables=0 >/dev/null 2>&1");

        std::string subnet = config_.gateway_ip.substr(0, config_.gateway_ip.rfind('.')) + ".0/24";

        // iptables rules
        cmd = "iptables -I FORWARD 1 -i " + config_.bridge_name + " -j ACCEPT 2>/dev/null";
        system(cmd.c_str());

        cmd = "iptables -I FORWARD 1 -o " + config_.bridge_name + " -j ACCEPT 2>/dev/null";
        system(cmd.c_str());

        cmd = "iptables -I FORWARD 1 -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null";
        system(cmd.c_str());

        cmd = "iptables -t nat -A POSTROUTING -s " + subnet + " ! -o " + config_.bridge_name + " -j MASQUERADE 2>/dev/null";
        system(cmd.c_str());

        std::cout << "[Network] Bridge created with NAT successfully" << std::endl;
    }

    return true;
}


bool NetworkManager::create_veth_pair(const std::string& veth_host, const std::string& veth_container) {
    std::string cmd = "ip link add " + veth_host + " type veth peer name " + veth_container;
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Command failed: " << cmd << std::endl;
        return false;
    }

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
        std::cerr << "[Network] Failed to attach " << veth_host << " to bridge" << std::endl;
        return false;
    }

    std::cout << "[Network] Successfully attached veth to bridge" << std::endl;
    return true;
}

bool NetworkManager::move_veth_to_namespace(const std::string& veth_container, pid_t pid) {
    std::cout << "[Network] Moving " << veth_container << " to container namespace" << std::endl;

    std::string cmd = "ip link set " + veth_container + " netns " + std::to_string(pid);
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to move veth to namespace" << std::endl;
        return false;
    }

    return true;
}

bool NetworkManager::configure_container_network(pid_t pid, const std::string& veth_name) {
    std::string ns_path = "/proc/" + std::to_string(pid) + "/ns/net";

    // Bring up loopback
    std::string cmd = "nsenter --net=" + ns_path + " ip link set lo up";
    system(cmd.c_str());

    // Rename veth to eth0
    cmd = "nsenter --net=" + ns_path + " ip link set " + veth_name + " name eth0 2>/dev/null";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to rename " << veth_name << " to eth0" << std::endl;
        return false;
    }

    // Configure IP
    cmd = "nsenter --net=" + ns_path + " ip addr add " + config_.container_ip + " dev eth0";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to add IP address" << std::endl;
        return false;
    }

    // Bring up eth0
    cmd = "nsenter --net=" + ns_path + " ip link set eth0 up";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Failed to bring up eth0" << std::endl;
        return false;
    }

    // Add default route
    cmd = "nsenter --net=" + ns_path + " ip route add default via " + config_.gateway_ip;
    if (system(cmd.c_str()) != 0) {
        std::cerr << "[Network] Warning: Failed to add default route" << std::endl;
    }

    return true;
}

bool NetworkManager::setup_port_forwarding() {
    for (const auto& mapping : config_.port_mappings) {
        size_t colon = mapping.find(':');
        if (colon == std::string::npos) continue;

        std::string host_port = mapping.substr(0, colon);
        std::string container_port = mapping.substr(colon + 1);
        std::string container_ip = config_.container_ip.substr(0, config_.container_ip.find('/'));

        std::string cmd = "iptables -t nat -A PREROUTING -p tcp --dport " + host_port +
                         " -j DNAT --to-destination " + container_ip + ":" + container_port;
        system(cmd.c_str());

        std::cout << "[Network] Port forwarding: host:" << host_port
                  << " -> container:" << container_port << std::endl;
    }

    return true;
}

bool NetworkManager::setup_dns(pid_t pid) {
    // Ensure /etc exists
    std::string etc_dir = "/proc/" + std::to_string(pid) + "/root/etc";
    struct stat st;
    if (stat(etc_dir.c_str(), &st) != 0) {
        std::cout << "[Network] Creating /etc directory" << std::endl;
        if (mkdir(etc_dir.c_str(), 0755) != 0) {
            std::cerr << "[Network] Failed to create /etc directory" << std::endl;
            return false;
        }
    }

    std::string resolv_conf = etc_dir + "/resolv.conf";
    FILE* f = fopen(resolv_conf.c_str(), "w");
    if (f) {
        fprintf(f, "nameserver 8.8.8.8\n");
        fprintf(f, "nameserver 8.8.4.4\n");
        fclose(f);
        std::cout << "[Network] DNS configured successfully" << std::endl;
        return true;
    }

    std::cerr << "[Network] Failed to write resolv.conf" << std::endl;
    return false;
}

bool NetworkManager::cleanup_container_network(const std::string& container_name) {
    std::cout << "[Network] Cleaning up network for " << container_name << std::endl;
    return true;
}
