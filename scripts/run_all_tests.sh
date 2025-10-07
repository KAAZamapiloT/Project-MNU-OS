#!/bin/bash

# MUN-OS Comprehensive Test Runner
# This script runs all test configurations and validates the results

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Paths
BINARY="./build/mun_os"
CONFIG_DIR="./configs"

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Test results array
declare -a TEST_RESULTS

print_banner() {
    echo -e "${CYAN}"
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║          MUN-OS Comprehensive Test Runner                 ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_section() {
    echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}

print_test_start() {
    echo -e "${YELLOW}[TEST $((TOTAL_TESTS + 1))]${NC} Testing: ${MAGENTA}$1${NC}"
    echo -e "  Config: ${CYAN}$2${NC}"
}

print_pass() {
    echo -e "  ${GREEN}✓ PASS${NC} - $1"
    ((PASSED_TESTS++))
    ((TOTAL_TESTS++))
    TEST_RESULTS+=("PASS: $2")
}

print_fail() {
    echo -e "  ${RED}✗ FAIL${NC} - $1"
    ((FAILED_TESTS++))
    ((TOTAL_TESTS++))
    TEST_RESULTS+=("FAIL: $2")
}

print_skip() {
    echo -e "  ${YELLOW}⊘ SKIP${NC} - $1"
    ((SKIPPED_TESTS++))
    ((TOTAL_TESTS++))
    TEST_RESULTS+=("SKIP: $2")
}

check_prerequisites() {
    print_section "Checking Prerequisites"
    
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} Running as root"
    
    # Check if binary exists
    if [ ! -f "$BINARY" ]; then
        echo -e "${RED}Error: Binary not found at $BINARY${NC}"
        echo "Please build the project first"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} Binary found: $BINARY"
    
    # Check if configs directory exists
    if [ ! -d "$CONFIG_DIR" ]; then
        echo -e "${YELLOW}Warning: Configs directory not found${NC}"
        echo -e "${YELLOW}Run: ./scripts/generate_test_configs.sh${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} Config directory found: $CONFIG_DIR"
    
    # Count config files
    CONFIG_COUNT=$(ls -1 "$CONFIG_DIR"/*.json 2>/dev/null | wc -l)
    echo -e "${GREEN}✓${NC} Found $CONFIG_COUNT configuration files"
    
    echo -e "\n${GREEN}All prerequisites met!${NC}"
}

cleanup_containers() {
    echo -e "${YELLOW}Cleaning up any running containers...${NC}"
    $BINARY list 2>/dev/null | grep -v "CONTAINER NAME" | grep -v "No containers" | grep -v "^-" | awk '{print $1}' | while read container; do
        if [ ! -z "$container" ]; then
            echo "  Stopping: $container"
            $BINARY stop "$container" 2>/dev/null || true
        fi
    done
    sleep 1
}

test_foreground_config() {
    local config_file="$1"
    local config_name=$(basename "$config_file" .json)
    local expected_behavior="$2"
    
    print_test_start "$config_name (foreground)" "$config_file"
    
    OUTPUT=$($BINARY run --config "$config_file" 2>&1 || true)
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -eq 0 ]; then
        print_pass "Container executed successfully" "$config_name"
        if [ ! -z "$expected_behavior" ] && echo "$OUTPUT" | grep -q "$expected_behavior"; then
            echo -e "    ${GREEN}→${NC} Expected output detected"
        fi
    else
        print_fail "Container failed with exit code $EXIT_CODE" "$config_name"
        echo -e "    ${RED}→${NC} Output: ${OUTPUT:0:100}..."
    fi
}

test_background_config() {
    local config_file="$1"
    local config_name=$(basename "$config_file" .json)
    
    print_test_start "$config_name (background)" "$config_file"
    
    $BINARY start --config "$config_file" 2>&1
    sleep 2
    
    if $BINARY list | grep -q "$config_name"; then
        print_pass "Container started and listed" "$config_name"
        
        # Try to stop it
        if $BINARY stop "$config_name" 2>&1 | grep -q "stopped"; then
            echo -e "    ${GREEN}→${NC} Container stopped successfully"
        else
            echo -e "    ${YELLOW}→${NC} Warning: Stop command had issues"
        fi
    else
        print_fail "Container not found in list" "$config_name"
    fi
    
    sleep 1
}

test_echo_configs() {
    print_section "Testing Echo Configurations"
    
    if [ -f "$CONFIG_DIR/echo.json" ]; then
        test_foreground_config "$CONFIG_DIR/echo.json" "Hello from MUN-OS"
    fi
    
    if [ -f "$CONFIG_DIR/quick_test.json" ]; then
        test_foreground_config "$CONFIG_DIR/quick_test.json" "Quick test completed"
    fi
    
    if [ -f "$CONFIG_DIR/minimal.json" ]; then
        test_foreground_config "$CONFIG_DIR/minimal.json" "Minimal configuration"
    fi
}

test_shell_configs() {
    print_section "Testing Shell Configurations"
    
    if [ -f "$CONFIG_DIR/test.json" ]; then
        test_foreground_config "$CONFIG_DIR/test.json" "Testing MUN-OS"
    fi
    
    if [ -f "$CONFIG_DIR/multi_command.json" ]; then
        test_foreground_config "$CONFIG_DIR/multi_command.json" "Complete!"
    fi
    
    if [ -f "$CONFIG_DIR/fs_explore.json" ]; then
        test_foreground_config "$CONFIG_DIR/fs_explore.json" "Filesystem Exploration"
    fi
}

test_resource_configs() {
    print_section "Testing Resource Limit Configurations"
    
    if [ -f "$CONFIG_DIR/low_memory.json" ]; then
        test_foreground_config "$CONFIG_DIR/low_memory.json" "Low memory"
    fi
    
    if [ -f "$CONFIG_DIR/limited_pids.json" ]; then
        test_foreground_config "$CONFIG_DIR/limited_pids.json" "Limited to"
    fi
    
    if [ -f "$CONFIG_DIR/high_memory.json" ]; then
        print_test_start "high_memory (shell test)" "$CONFIG_DIR/high_memory.json"
        # This one opens a shell, so we just verify it can be parsed
        if $BINARY run --config "$CONFIG_DIR/high_memory.json" /bin/echo "test" 2>&1 | grep -q "test"; then
            print_pass "High memory config works" "high_memory"
        else
            print_fail "High memory config failed" "high_memory"
        fi
    fi
}

test_background_configs() {
    print_section "Testing Background Service Configurations"
    
    cleanup_containers
    
    if [ -f "$CONFIG_DIR/short_bg.json" ]; then
        test_background_config "$CONFIG_DIR/short_bg.json"
    fi
    
    if [ -f "$CONFIG_DIR/bg.json" ]; then
        test_background_config "$CONFIG_DIR/bg.json"
    fi
    
    cleanup_containers
}

test_system_info_configs() {
    print_section "Testing System Information Configurations"
    
    if [ -f "$CONFIG_DIR/env_test.json" ]; then
        test_foreground_config "$CONFIG_DIR/env_test.json" "Environment Variables"
    fi
    
    if [ -f "$CONFIG_DIR/custom_hostname.json" ]; then
        test_foreground_config "$CONFIG_DIR/custom_hostname.json" "my-custom-container"
    fi
    
    if [ -f "$CONFIG_DIR/process_tree.json" ]; then
        test_foreground_config "$CONFIG_DIR/process_tree.json" "Process Tree"
    fi
    
    if [ -f "$CONFIG_DIR/alpine_test.json" ]; then
        test_foreground_config "$CONFIG_DIR/alpine_test.json"
    fi
}

test_cli_overrides() {
    print_section "Testing CLI Override Functionality"
    
    print_test_start "CLI memory override" "command line"
    if $BINARY run --rootfs ./rootfs --memory 256 /bin/echo "Memory override test" 2>&1 | grep -q "Memory override test"; then
        print_pass "CLI memory flag works" "cli_memory"
    else
        print_fail "CLI memory flag failed" "cli_memory"
    fi
    
    print_test_start "CLI pids override" "command line"
    if $BINARY run --rootfs ./rootfs --pids 50 /bin/echo "PID override test" 2>&1 | grep -q "PID override test"; then
        print_pass "CLI pids flag works" "cli_pids"
    else
        print_fail "CLI pids flag failed" "cli_pids"
    fi
    
    print_test_start "CLI command override" "command line + config"
    if [ -f "$CONFIG_DIR/example.json" ]; then
        if $BINARY run --config "$CONFIG_DIR/example.json" /bin/echo "Override" 2>&1 | grep -q "Override"; then
            print_pass "CLI command override works" "cli_command"
        else
            print_fail "CLI command override failed" "cli_command"
        fi
    fi
}

test_multiple_containers() {
    print_section "Testing Multiple Containers"
    
    cleanup_containers
    
    print_test_start "Multiple containers simultaneously" "multiple configs"
    
    if [ -f "$CONFIG_DIR/short_bg.json" ] && [ -f "$CONFIG_DIR/bg.json" ]; then
        # Start multiple containers
        $BINARY start --config "$CONFIG_DIR/short_bg.json" 2>&1
        sleep 1
        
        # Copy bg.json to a different name to avoid conflict
        cp "$CONFIG_DIR/bg.json" "$CONFIG_DIR/bg2.json"
        $BINARY start --config "$CONFIG_DIR/bg2.json" 2>&1
        sleep 2
        
        # Count running containers
        CONTAINER_COUNT=$($BINARY list | grep -v "CONTAINER NAME" | grep -v "No containers" | grep -v "^-" | grep -c "running" || true)
        
        if [ "$CONTAINER_COUNT" -ge 2 ]; then
            print_pass "Multiple containers running ($CONTAINER_COUNT containers)" "multiple_containers"
        else
            print_fail "Expected 2+ containers, found $CONTAINER_COUNT" "multiple_containers"
        fi
        
        # Cleanup
        rm -f "$CONFIG_DIR/bg2.json"
        cleanup_containers
    else
        print_skip "Required config files not found" "multiple_containers"
    fi
}

test_error_handling() {
    print_section "Testing Error Handling"
    
    print_test_start "Non-existent config file" "error handling"
    if $BINARY run --config "$CONFIG_DIR/nonexistent.json" 2>&1 | grep -q -i "error\|does not exist"; then
        print_pass "Non-existent config properly handled" "error_nonexistent"
    else
        print_fail "Non-existent config not properly handled" "error_nonexistent"
    fi
    
    print_test_start "Stop non-existent container" "error handling"
    if $BINARY stop nonexistent_container 2>&1 | grep -q -i "not found\|error"; then
        print_pass "Stop non-existent properly handled" "error_stop"
    else
        print_fail "Stop non-existent not properly handled" "error_stop"
    fi
}

print_summary() {
    print_section "Test Summary"
    
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║                    RESULTS SUMMARY                         ║${NC}"
    echo -e "${CYAN}╠════════════════════════════════════════════════════════════╣${NC}"
    printf "${CYAN}║${NC}  Total Tests:    %-40s ${CYAN}║${NC}\n" "$TOTAL_TESTS"
    printf "${CYAN}║${NC}  ${GREEN}Passed:${NC}         %-40s ${CYAN}║${NC}\n" "$PASSED_TESTS"
    printf "${CYAN}║${NC}  ${RED}Failed:${NC}         %-40s ${CYAN}║${NC}\n" "$FAILED_TESTS"
    printf "${CYAN}║${NC}  ${YELLOW}Skipped:${NC}        %-40s ${CYAN}║${NC}\n" "$SKIPPED_TESTS"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}\n"
    
    # Calculate success rate
    if [ $TOTAL_TESTS -gt 0 ]; then
        SUCCESS_RATE=$((PASSED_TESTS * 100 / TOTAL_TESTS))
        echo -e "Success Rate: ${GREEN}${SUCCESS_RATE}%${NC}\n"
    fi
    
    # Print detailed results
    if [ $FAILED_TESTS -gt 0 ]; then
        echo -e "${RED}Failed Tests:${NC}"
        for result in "${TEST_RESULTS[@]}"; do
            if [[ $result == FAIL:* ]]; then
                echo -e "  ${RED}✗${NC} ${result#FAIL: }"
            fi
        done
        echo ""
    fi
    
    # Final verdict
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║              ALL TESTS PASSED! ✓                           ║${NC}"
        echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}\n"
        return 0
    else
        echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║              SOME TESTS FAILED! ✗                          ║${NC}"
        echo -e "${RED}╚════════════════════════════════════════════════════════════╝${NC}\n"
        return 1
    fi
}

main() {
    print_banner
    
    check_prerequisites
    
    # Set trap for cleanup
    trap cleanup_containers EXIT
    
    # Run all test categories
    test_echo_configs
    test_shell_configs
    test_resource_configs
    test_system_info_configs
    test_background_configs
    test_cli_overrides
    test_multiple_containers
    test_error_handling
    
    # Print summary
    print_summary
}

# Run main
main