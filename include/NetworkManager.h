#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <string>
#include <vector>
#include <sys/types.h>
#include <mutex>
struct NetworkConfig {
    bool enable_networking = true;
    std::string bridge_name = "mun-bridge0";
    std::string container_ip = "172.18.0.2/24";
    std::string gateway_ip = "172.18.0.1";
    std::vector<std::string> port_mappings;  // Format: "host_port:container_port"
    bool enable_dns = true;
};

class NetworkManager {
public:
    NetworkManager(const NetworkConfig& config);
    ~NetworkManager();

    // Setup networking for container
    bool setup_container_network(pid_t container_pid, const std::string& container_name);

    // Cleanup networking
    bool cleanup_container_network(const std::string& container_name);

private:
    NetworkConfig config_;
    // ✅ Thread-safe counter for veth naming
        static std::mutex counter_mutex_;
        static int veth_counter_;
        static int get_next_veth_id();
    // Helper functions
    bool create_bridge();
    bool create_veth_pair(const std::string& veth_host, const std::string& veth_container);
    bool attach_veth_to_bridge(const std::string& veth_host);
    bool move_veth_to_namespace(const std::string& veth_container, pid_t pid);
    bool configure_container_network(pid_t pid, const std::string& veth_name);
    bool setup_port_forwarding();
    bool setup_dns(pid_t pid);
};

#endif // NETWORK_MANAGER_H
