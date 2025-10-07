#!/bin/bash

# MUN-OS Background Container Manager
# This script helps manage background containers easily

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

BINARY="./build/mun_os"
CONFIG_DIR="./configs"

print_header() {
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║     MUN-OS Background Container Manager                   ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}\n"
}

print_usage() {
    echo -e "${YELLOW}Usage:${NC}"
    echo "  $0 start <config_name>     - Start a container in background"
    echo "  $0 stop <container_name>   - Stop a background container"
    echo "  $0 list                    - List all running containers"
    echo "  $0 stopall                 - Stop all running containers"
    echo "  $0 restart <container>     - Restart a container"
    echo "  $0 logs <container_name>   - Show container info"
    echo "  $0 quick <command>         - Quick background container"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  $0 start bg                # Starts configs/bg.json in background"
    echo "  $0 start long_running      # Starts configs/long_running.json"
    echo "  $0 quick 'sleep 300'       # Quick background container"
    echo "  $0 list                    # Show all running containers"
    echo "  $0 stop bg                 # Stop the 'bg' container"
    echo ""
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
        exit 1
    fi
}

check_binary() {
    if [ ! -f "$BINARY" ]; then
        echo -e "${RED}Error: Binary not found at $BINARY${NC}"
        echo "Please build the project first"
        exit 1
    fi
}

start_container() {
    local config_name="$1"
    local config_path="$CONFIG_DIR/${config_name}.json"
    
    echo -e "${BLUE}Starting container '${config_name}' in background...${NC}"
    
    if [ ! -f "$config_path" ]; then
        echo -e "${RED}Error: Config file not found: $config_path${NC}"
        echo -e "${YELLOW}Available configs:${NC}"
        ls -1 "$CONFIG_DIR"/*.json 2>/dev/null | xargs -n1 basename | sed 's/.json$//'
        exit 1
    fi
    
    # Start the container
    OUTPUT=$($BINARY start --config "$config_path" 2>&1)
    
    if echo "$OUTPUT" | grep -q -i "error\|failed"; then
        echo -e "${RED}Failed to start container!${NC}"
        echo "$OUTPUT"
        exit 1
    fi
    
    sleep 2
    
    # Verify it's running
    if $BINARY list | grep -q "$config_name"; then
        echo -e "${GREEN}✓ Container '$config_name' started successfully!${NC}\n"
        
        # Show container info
        echo -e "${CYAN}Container Details:${NC}"
        $BINARY list | head -2
        $BINARY list | grep "$config_name"
    else
        echo -e "${RED}✗ Container started but not found in list${NC}"
        exit 1
    fi
}

stop_container() {
    local container_name="$1"
    
    echo -e "${YELLOW}Stopping container '$container_name'...${NC}"
    
    OUTPUT=$($BINARY stop "$container_name" 2>&1)
    
    if echo "$OUTPUT" | grep -q -i "stopped\|removed"; then
        echo -e "${GREEN}✓ Container '$container_name' stopped successfully!${NC}"
    else
        echo -e "${RED}✗ Failed to stop container${NC}"
        echo "$OUTPUT"
        exit 1
    fi
}

list_containers() {
    echo -e "${CYAN}Running Containers:${NC}\n"
    $BINARY list
}

stop_all_containers() {
    echo -e "${YELLOW}Stopping all containers...${NC}\n"
    
    CONTAINERS=$($BINARY list | grep -v "CONTAINER NAME" | grep -v "No containers" | grep -v "^-" | awk '{print $1}')
    
    if [ -z "$CONTAINERS" ]; then
        echo -e "${YELLOW}No containers running${NC}"
        exit 0
    fi
    
    for container in $CONTAINERS; do
        if [ ! -z "$container" ]; then
            echo -e "${BLUE}Stopping: $container${NC}"
            $BINARY stop "$container" 2>/dev/null || echo -e "${RED}  Failed to stop $container${NC}"
        fi
    done
    
    echo -e "\n${GREEN}All containers stopped!${NC}"
}

restart_container() {
    local container_name="$1"
    
    echo -e "${YELLOW}Restarting container '$container_name'...${NC}\n"
    
    # Get the config path
    CONFIG_PATH=$($BINARY list | grep "$container_name" | awk '{print $NF}')
    
    if [ -z "$CONFIG_PATH" ]; then
        echo -e "${RED}Error: Container '$container_name' not found${NC}"
        exit 1
    fi
    
    echo -e "${BLUE}Step 1: Stopping container...${NC}"
    stop_container "$container_name"
    
    sleep 2
    
    echo -e "${BLUE}Step 2: Starting container...${NC}"
    OUTPUT=$($BINARY start --config "$CONFIG_PATH" 2>&1)
    
    sleep 2
    
    if $BINARY list | grep -q "$container_name"; then
        echo -e "${GREEN}✓ Container restarted successfully!${NC}"
    else
        echo -e "${RED}✗ Failed to restart container${NC}"
        exit 1
    fi
}

show_container_logs() {
    local container_name="$1"
    
    echo -e "${CYAN}Container Information:${NC}\n"
    
    INFO=$($BINARY list | grep "$container_name")
    
    if [ -z "$INFO" ]; then
        echo -e "${RED}Error: Container '$container_name' not found${NC}"
        exit 1
    fi
    
    echo "$INFO" | awk '{
        print "Name:        " $1
        print "PID:         " $2
        print "Status:      " $3
        print "Config:      " $4
    }'
    
    PID=$(echo "$INFO" | awk '{print $2}')
    
    echo -e "\n${CYAN}Process Information:${NC}"
    ps -p "$PID" -o pid,ppid,cmd,etime,%cpu,%mem 2>/dev/null || echo "Process information not available"
    
    echo -e "\n${CYAN}Process Tree:${NC}"
    pstree -p "$PID" 2>/dev/null || echo "pstree not available"
}

quick_container() {
    local command="$1"
    
    if [ -z "$command" ]; then
        echo -e "${RED}Error: Command required${NC}"
        echo "Example: $0 quick 'sleep 300'"
        exit 1
    fi
    
    # Create a temporary config
    TEMP_CONFIG="/tmp/mun_os_quick_$$.json"
    CONTAINER_NAME="quick_$$"
    
    echo -e "${BLUE}Creating quick background container...${NC}"
    
    cat > "$TEMP_CONFIG" << EOF
{
  "hostname": "$CONTAINER_NAME",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "$command"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF
    
    echo -e "${YELLOW}Starting container...${NC}"
    $BINARY start --config "$TEMP_CONFIG" 2>&1
    
    sleep 2
    
    if $BINARY list | grep -q "$CONTAINER_NAME"; then
        echo -e "${GREEN}✓ Quick container started!${NC}"
        echo -e "${CYAN}Container name: $CONTAINER_NAME${NC}"
        echo -e "${CYAN}Command: $command${NC}\n"
        $BINARY list | grep "$CONTAINER_NAME"
    else
        echo -e "${RED}✗ Failed to start container${NC}"
        rm -f "$TEMP_CONFIG"
        exit 1
    fi
    
    # Keep the temp config for now (will be used when stopping)
    echo -e "\n${YELLOW}Note: To stop this container, run:${NC}"
    echo "  sudo $0 stop $CONTAINER_NAME"
}

interactive_menu() {
    while true; do
        echo -e "\n${CYAN}╔════════════════════════════════════════╗${NC}"
        echo -e "${CYAN}║   MUN-OS Background Container Menu    ║${NC}"
        echo -e "${CYAN}╚════════════════════════════════════════╝${NC}\n"
        echo -e "${YELLOW}1.${NC} Start a container"
        echo -e "${YELLOW}2.${NC} Stop a container"
        echo -e "${YELLOW}3.${NC} List running containers"
        echo -e "${YELLOW}4.${NC} Stop all containers"
        echo -e "${YELLOW}5.${NC} Restart a container"
        echo -e "${YELLOW}6.${NC} Show container info"
        echo -e "${YELLOW}7.${NC} Quick container"
        echo -e "${YELLOW}8.${NC} Exit"
        echo -e -n "\n${CYAN}Choose an option [1-8]:${NC} "
        
        read choice
        
        case $choice in
            1)
                echo -e "\n${YELLOW}Available configs:${NC}"
                ls -1 "$CONFIG_DIR"/*.json 2>/dev/null | xargs -n1 basename | sed 's/.json$//' | nl
                echo -e -n "\n${CYAN}Enter config name:${NC} "
                read config_name
                start_container "$config_name"
                ;;
            2)
                list_containers
                echo -e -n "\n${CYAN}Enter container name to stop:${NC} "
                read container_name
                stop_container "$container_name"
                ;;
            3)
                list_containers
                ;;
            4)
                stop_all_containers
                ;;
            5)
                list_containers
                echo -e -n "\n${CYAN}Enter container name to restart:${NC} "
                read container_name
                restart_container "$container_name"
                ;;
            6)
                list_containers
                echo -e -n "\n${CYAN}Enter container name:${NC} "
                read container_name
                show_container_logs "$container_name"
                ;;
            7)
                echo -e -n "\n${CYAN}Enter command to run:${NC} "
                read command
                quick_container "$command"
                ;;
            8)
                echo -e "\n${GREEN}Goodbye!${NC}\n"
                exit 0
                ;;
            *)
                echo -e "\n${RED}Invalid option!${NC}"
                ;;
        esac
        
        echo -e -n "\n${YELLOW}Press Enter to continue...${NC}"
        read
    done
}

main() {
    print_header
    check_root
    check_binary
    
    if [ $# -eq 0 ]; then
        interactive_menu
    fi
    
    case "$1" in
        start)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Config name required${NC}"
                print_usage
                exit 1
            fi
            start_container "$2"
            ;;
        stop)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Container name required${NC}"
                print_usage
                exit 1
            fi
            stop_container "$2"
            ;;
        list)
            list_containers
            ;;
        stopall)
            stop_all_containers
            ;;
        restart)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Container name required${NC}"
                print_usage
                exit 1
            fi
            restart_container "$2"
            ;;
        logs|info)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Container name required${NC}"
                print_usage
                exit 1
            fi
            show_container_logs "$2"
            ;;
        quick)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Command required${NC}"
                print_usage
                exit 1
            fi
            quick_container "$2"
            ;;
        help|-h|--help)
            print_usage
            ;;
        *)
            echo -e "${RED}Error: Unknown command '$1'${NC}\n"
            print_usage
            exit 1
            ;;
    esac
}

main "$@"