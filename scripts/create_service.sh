#!/bin/bash

# MUN-OS Systemd Service Generator
# Creates systemd service files for containers to run as system services

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

BINARY_PATH="$(pwd)/build/mun_os"
CONFIG_DIR="$(pwd)/configs"
SERVICE_DIR="/etc/systemd/system"

print_header() {
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║     MUN-OS Systemd Service Generator                      ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}\n"
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
        exit 1
    fi
}

generate_service_file() {
    local container_name="$1"
    local config_path="$2"
    local service_name="mun-os-${container_name}.service"
    local service_path="${SERVICE_DIR}/${service_name}"
    
    echo -e "${BLUE}Generating service file: ${service_name}${NC}"
    
    cat > "$service_path" << EOF
[Unit]
Description=MUN-OS Container: ${container_name}
After=network.target
Documentation=https://github.com/yourusername/MUN-OS

[Service]
Type=forking
ExecStart=${BINARY_PATH} start --config ${config_path}
ExecStop=${BINARY_PATH} stop ${container_name}
Restart=on-failure
RestartSec=5s
StandardOutput=journal
StandardError=journal

# Security settings
PrivateTmp=yes
NoNewPrivileges=true

[Install]
WantedBy=multi-user.target
EOF
    
    echo -e "${GREEN}✓ Service file created: ${service_path}${NC}"
}

create_service() {
    local config_name="$1"
    local config_path="${CONFIG_DIR}/${config_name}.json"
    
    if [ ! -f "$config_path" ]; then
        echo -e "${RED}Error: Config file not found: ${config_path}${NC}"
        echo -e "${YELLOW}Available configs:${NC}"
        ls -1 "$CONFIG_DIR"/*.json 2>/dev/null | xargs -n1 basename | sed 's/.json$//'
        exit 1
    fi
    
    generate_service_file "$config_name" "$config_path"
    
    # Reload systemd
    echo -e "${YELLOW}Reloading systemd daemon...${NC}"
    systemctl daemon-reload
    
    echo -e "\n${GREEN}Service created successfully!${NC}\n"
    echo -e "${CYAN}Commands:${NC}"
    echo -e "  Start service:   ${YELLOW}sudo systemctl start mun-os-${config_name}${NC}"
    echo -e "  Stop service:    ${YELLOW}sudo systemctl stop mun-os-${config_name}${NC}"
    echo -e "  Enable on boot:  ${YELLOW}sudo systemctl enable mun-os-${config_name}${NC}"
    echo -e "  Service status:  ${YELLOW}sudo systemctl status mun-os-${config_name}${NC}"
    echo -e "  View logs:       ${YELLOW}sudo journalctl -u mun-os-${config_name} -f${NC}"
}

remove_service() {
    local config_name="$1"
    local service_name="mun-os-${config_name}.service"
    local service_path="${SERVICE_DIR}/${service_name}"
    
    if [ ! -f "$service_path" ]; then
        echo -e "${RED}Error: Service not found: ${service_name}${NC}"
        exit 1
    fi
    
    echo -e "${YELLOW}Stopping and removing service: ${service_name}${NC}"
    
    # Stop and disable the service
    systemctl stop "$service_name" 2>/dev/null || true
    systemctl disable "$service_name" 2>/dev/null || true
    
    # Remove the service file
    rm -f "$service_path"
    
    # Reload systemd
    systemctl daemon-reload
    
    echo -e "${GREEN}✓ Service removed successfully!${NC}"
}

list_services() {
    echo -e "${CYAN}MUN-OS Container Services:${NC}\n"
    
    SERVICES=$(systemctl list-units --type=service --all | grep "mun-os-" | awk '{print $1}')
    
    if [ -z "$SERVICES" ]; then
        echo -e "${YELLOW}No MUN-OS services found${NC}"
        echo -e "${YELLOW}Create one with: sudo $0 create <config_name>${NC}"
        return
    fi
    
    printf "%-30s %-15s %-15s\n" "SERVICE" "STATUS" "ENABLED"
    printf "%-30s %-15s %-15s\n" "------------------------------" "---------------" "---------------"
    
    for service in $SERVICES; do
        STATUS=$(systemctl is-active "$service" 2>/dev/null || echo "inactive")
        ENABLED=$(systemctl is-enabled "$service" 2>/dev/null || echo "disabled")
        printf "%-30s %-15s %-15s\n" "$service" "$STATUS" "$ENABLED"
    done
}

create_all_services() {
    echo -e "${BLUE}Creating services for all available configs...${NC}\n"
    
    for config_file in "$CONFIG_DIR"/*.json; do
        if [ -f "$config_file" ]; then
            config_name=$(basename "$config_file" .json)
            echo -e "${CYAN}Processing: $config_name${NC}"
            generate_service_file "$config_name" "$config_file"
            echo ""
        fi
    done
    
    systemctl daemon-reload
    
    echo -e "${GREEN}All services created!${NC}"
    echo -e "${YELLOW}Run 'sudo $0 list' to see all services${NC}"
}

remove_all_services() {
    echo -e "${YELLOW}Removing all MUN-OS services...${NC}\n"
    
    SERVICES=$(systemctl list-units --type=service --all | grep "mun-os-" | awk '{print $1}')
    
    if [ -z "$SERVICES" ]; then
        echo -e "${YELLOW}No services to remove${NC}"
        return
    fi
    
    for service in $SERVICES; do
        echo -e "${BLUE}Removing: $service${NC}"
        systemctl stop "$service" 2>/dev/null || true
        systemctl disable "$service" 2>/dev/null || true
        rm -f "${SERVICE_DIR}/${service}"
    done
    
    systemctl daemon-reload
    
    echo -e "${GREEN}All services removed!${NC}"
}

print_usage() {
    echo -e "${YELLOW}Usage:${NC}"
    echo "  $0 create <config_name>     - Create systemd service for a container"
    echo "  $0 remove <config_name>     - Remove systemd service"
    echo "  $0 list                     - List all MUN-OS services"
    echo "  $0 create-all               - Create services for all configs"
    echo "  $0 remove-all               - Remove all MUN-OS services"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  $0 create bg                # Create service for configs/bg.json"
    echo "  $0 list                     # List all services"
    echo "  $0 remove bg                # Remove the service"
    echo ""
    echo -e "${YELLOW}After creating a service:${NC}"
    echo "  sudo systemctl start mun-os-bg      # Start the service"
    echo "  sudo systemctl enable mun-os-bg     # Enable on boot"
    echo "  sudo systemctl status mun-os-bg     # Check status"
    echo "  sudo journalctl -u mun-os-bg -f     # View logs"
    echo ""
}

main() {
    print_header
    check_root
    
    case "${1:-}" in
        create)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Config name required${NC}"
                print_usage
                exit 1
            fi
            create_service "$2"
            ;;
        remove)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Config name required${NC}"
                print_usage
                exit 1
            fi
            remove_service "$2"
            ;;
        list)
            list_services
            ;;
        create-all)
            create_all_services
            ;;
        remove-all)
            remove_all_services
            ;;
        help|-h|--help)
            print_usage
            ;;
        *)
            echo -e "${RED}Error: Unknown command '${1:-}'${NC}\n"
            print_usage
            exit 1
            ;;
    esac
}

main "$@"