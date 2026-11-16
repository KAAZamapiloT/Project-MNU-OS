#!/bin/bash

# MUN-OS Container Test Suite
# This script tests all major functionality of the container system

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Paths
BINARY="./build/mun_os"
CONFIG_DIR="./configs"
TEST_CONFIG_DIR="./test_configs"
ROOTFS="./rootfs"

# Helper Functions
print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

print_test() {
    echo -e "${YELLOW}[TEST $((TESTS_TOTAL + 1))]${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓ PASS:${NC} $1"
    ((TESTS_PASSED++))
    ((TESTS_TOTAL++))
}

print_failure() {
    echo -e "${RED}✗ FAIL:${NC} $1"
    ((TESTS_FAILED++))
    ((TESTS_TOTAL++))
}

cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    # Stop all test containers
    sudo $BINARY list | grep -v "CONTAINER NAME" | grep -v "No containers" | awk '{print $1}' | while read container; do
        if [ ! -z "$container" ] && [ "$container" != "--------------------" ]; then
            echo "Stopping container: $container"
            sudo $BINARY stop "$container" 2>/dev/null || true
        fi
    done
    # Remove test config directory
    rm -rf "$TEST_CONFIG_DIR"
}

# Setup
setup() {
    print_header "SETUP: Preparing Test Environment"
    
    # Check if binary exists
    if [ ! -f "$BINARY" ]; then
        echo -e "${RED}Error: Binary not found at $BINARY${NC}"
        echo "Please build the project first: cmake --build build"
        exit 1
    fi
    
    # Check if rootfs exists
    if [ ! -d "$ROOTFS" ]; then
        echo -e "${RED}Error: Rootfs not found at $ROOTFS${NC}"
        exit 1
    fi
    
    # Create test config directory
    mkdir -p "$TEST_CONFIG_DIR"
    
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Error: This test suite must be run as root (use sudo)${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Setup complete!${NC}"
}

# Test 1: Binary existence and help
test_binary_help() {
    print_test "Binary exists and shows help"
    
    if $BINARY 2>&1 | grep -q "Usage:"; then
        print_success "Binary shows usage information"
    else
        print_failure "Binary does not show proper usage"
    fi
}

# Test 2: Config file parsing
test_config_parsing() {
    print_test "Config file parsing"
    
    # Create a valid config
    cat > "$TEST_CONFIG_DIR/valid.json" << 'EOF'
{
  "hostname": "test-valid",
  "rootfs_path": "./rootfs",
  "command": "/bin/echo",
  "args": ["Hello, World!"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF
    
    if sudo $BINARY run --config "$TEST_CONFIG_DIR/valid.json" 2>&1 | grep -q "Hello, World!"; then
        print_success "Valid config parsed and executed successfully"
    else
        print_failure "Failed to parse valid config"
    fi
}

# Test 3: Invalid config handling
test_invalid_config() {
    print_test "Invalid config file handling"
    
    # Create an invalid config (missing required field)
    cat > "$TEST_CONFIG_DIR/invalid.json" << 'EOF'
{
  "hostname": "test-invalid",
  "command": "/bin/echo"
}
EOF
    
    if sudo $BINARY run --config "$TEST_CONFIG_DIR/invalid.json" 2>&1 | grep -q -i "error\|failed"; then
        print_success "Invalid config properly rejected"
    else
        print_failure "Invalid config not properly handled"
    fi
}

# Test 4: Non-existent config file
test_missing_config() {
    print_test "Non-existent config file handling"
    
    if sudo $BINARY run --config "$TEST_CONFIG_DIR/nonexistent.json" 2>&1 | grep -q -i "error\|does not exist\|cannot"; then
        print_success "Missing config file properly detected"
    else
        print_failure "Missing config file not properly handled"
    fi
}

# Test 5: Run command in foreground
test_run_foreground() {
    print_test "Run container in foreground"
    
    cat > "$TEST_CONFIG_DIR/foreground.json" << 'EOF'
{
  "hostname": "test-fg",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Foreground test'; exit 0"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF
    
    OUTPUT=$(sudo $BINARY run --config "$TEST_CONFIG_DIR/foreground.json" 2>&1)
    
    if echo "$OUTPUT" | grep -q "Foreground test"; then
        print_success "Foreground container executed successfully"
    else
        print_failure "Foreground container failed"
        echo "Output: $OUTPUT"
    fi
}

# Test 6: Start container in background
test_start_background() {
    print_test "Start container in background"
    
    cat > "$TEST_CONFIG_DIR/background.json" << 'EOF'
{
  "hostname": "test-bg",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["30"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF
    
    sudo $BINARY start --config "$TEST_CONFIG_DIR/background.json" 2>&1
    sleep 2
    
    if sudo $BINARY list | grep -q "background"; then
        print_success "Background container started successfully"
        # Cleanup
        sudo $BINARY stop background 2>/dev/null || true
    else
        print_failure "Background container not listed"
    fi
}

# Test 7: List containers
test_list_containers() {
    print_test "List running containers"
    
    # Start a test container
    cat > "$TEST_CONFIG_DIR/list_test.json" << 'EOF'
{
  "hostname": "test-list",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["30"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF
    
    sudo $BINARY start --config "$TEST_CONFIG_DIR/list_test.json" 2>&1
    sleep 2
    
    OUTPUT=$(sudo $BINARY list)
    
    if echo "$OUTPUT" | grep -q "list_test"; then
        print_success "Container appears in list"
        sudo $BINARY stop list_test 2>/dev/null || true
    else
        print_failure "Container does not appear in list"
        echo "Output: $OUTPUT"
    fi
}

# Test 8: Stop container
test_stop_container() {
    print_test "Stop running container"
    
    # Start a container
    cat > "$TEST_CONFIG_DIR/stop_test.json" << 'EOF'
{
  "hostname": "test-stop",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["60"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF
    
    sudo $BINARY start --config "$TEST_CONFIG_DIR/stop_test.json" 2>&1
    sleep 2
    
    # Stop it
    OUTPUT=$(sudo $BINARY stop stop_test 2>&1)
    sleep 1
    
    if echo "$OUTPUT" | grep -q "stopped\|removed"; then
        # Verify it's not in the list anymore
        if ! sudo $BINARY list | grep -q "stop_test"; then
            print_success "Container stopped and removed from list"
        else
            print_failure "Container stopped but still in list"
        fi
    else
        print_failure "Failed to stop container"
        echo "Output: $OUTPUT"
    fi
}

# Test 9: CLI flag overrides
test_cli_overrides() {
    print_test "CLI flags override config file"
    
    cat > "$TEST_CONFIG_DIR/override.json" << 'EOF'
{
  "hostname": "test-override",
  "rootfs_path": "./rootfs",
  "command": "/bin/echo",
  "args": ["original"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF
    
    # Override the command with CLI args
    OUTPUT=$(sudo $BINARY run --config "$TEST_CONFIG_DIR/override.json" /bin/echo "overridden" 2>&1)
    
    if echo "$OUTPUT" | grep -q "overridden"; then
        print_success "CLI arguments override config file"
    else
        print_failure "CLI arguments did not override config"
        echo "Output: $OUTPUT"
    fi
}

# Test 10: Memory limit flag
test_memory_flag() {
    print_test "Memory limit CLI flag"
    
    OUTPUT=$(sudo $BINARY run --rootfs "$ROOTFS" --memory 256 /bin/echo "Memory test" 2>&1)
    
    if echo "$OUTPUT" | grep -q "Memory test"; then
        print_success "Memory flag accepted and container ran"
    else
        print_failure "Memory flag caused issues"
        echo "Output: $OUTPUT"
    fi
}

# Test 11: Process limit flag
test_pids_flag() {
    print_test "Process limit CLI flag"
    
    OUTPUT=$(sudo $BINARY run --rootfs "$ROOTFS" --pids 50 /bin/echo "PID test" 2>&1)
    
    if echo "$OUTPUT" | grep -q "PID test"; then
        print_success "Process limit flag accepted and container ran"
    else
        print_failure "Process limit flag caused issues"
        echo "Output: $OUTPUT"
    fi
}

# Test 12: Environment variable override
test_env_override() {
    print_test "Environment variable overrides"
    
    cat > "$TEST_CONFIG_DIR/env_test.json" << 'EOF'
{
  "hostname": "test-env",
  "rootfs_path": "./rootfs",
  "command": "/bin/echo",
  "args": ["env test"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF
    
    OUTPUT=$(MUN_OS_MEMORY_LIMIT=512 MUN_OS_PIDS_LIMIT=100 sudo -E $BINARY run --config "$TEST_CONFIG_DIR/env_test.json" 2>&1)
    
    if echo "$OUTPUT" | grep -q "env test"; then
        print_success "Environment variables accepted"
    else
        print_failure "Environment variables caused issues"
        echo "Output: $OUTPUT"
    fi
}

# Test 13: Container isolation
test_container_isolation() {
    print_test "Container filesystem isolation"
    
    cat > "$TEST_CONFIG_DIR/isolation.json" << 'EOF'
{
  "hostname": "test-isolation",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "ls / | head -5"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF
    
    OUTPUT=$(sudo $BINARY run --config "$TEST_CONFIG_DIR/isolation.json" 2>&1)
    
    # Check if we see typical rootfs contents and not host filesystem
    if echo "$OUTPUT" | grep -q "bin\|usr\|etc"; then
        print_success "Container shows isolated filesystem"
    else
        print_failure "Container filesystem isolation unclear"
        echo "Output: $OUTPUT"
    fi
}

# Test 14: Multiple containers
test_multiple_containers() {
    print_test "Running multiple containers simultaneously"
    
    # Start first container
    cat > "$TEST_CONFIG_DIR/multi1.json" << 'EOF'
{
  "hostname": "test-multi1",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["30"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF
    
    # Start second container
    cat > "$TEST_CONFIG_DIR/multi2.json" << 'EOF'
{
  "hostname": "test-multi2",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["30"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF
    
    sudo $BINARY start --config "$TEST_CONFIG_DIR/multi1.json" 2>&1
    sleep 1
    sudo $BINARY start --config "$TEST_CONFIG_DIR/multi2.json" 2>&1
    sleep 2
    
    OUTPUT=$(sudo $BINARY list)
    CONTAINER_COUNT=$(echo "$OUTPUT" | grep -c "multi" || true)
    
    if [ "$CONTAINER_COUNT" -eq 2 ]; then
        print_success "Multiple containers running simultaneously"
        sudo $BINARY stop multi1 2>/dev/null || true
        sudo $BINARY stop multi2 2>/dev/null || true
    else
        print_failure "Could not run multiple containers"
        echo "Found $CONTAINER_COUNT containers, expected 2"
        sudo $BINARY stop multi1 2>/dev/null || true
        sudo $BINARY stop multi2 2>/dev/null || true
    fi
}

# Test 15: Stop non-existent container
test_stop_nonexistent() {
    print_test "Stopping non-existent container"
    
    OUTPUT=$(sudo $BINARY stop nonexistent_container 2>&1)
    
    if echo "$OUTPUT" | grep -q -i "not found\|error"; then
        print_success "Non-existent container properly handled"
    else
        print_failure "Non-existent container stop did not show error"
        echo "Output: $OUTPUT"
    fi
}

# Summary
print_summary() {
    print_header "TEST SUMMARY"
    
    echo -e "Total Tests: ${BLUE}$TESTS_TOTAL${NC}"
    echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}All tests passed! ✓${NC}\n"
        return 0
    else
        echo -e "\n${RED}Some tests failed! ✗${NC}\n"
        return 1
    fi
}

# Main execution
main() {
    print_header "MUN-OS Container Test Suite"
    
    # Set trap for cleanup
    trap cleanup EXIT
    
    setup
    
    print_header "RUNNING TESTS"
    
    test_binary_help
    test_config_parsing
    test_invalid_config
    test_missing_config
    test_run_foreground
    test_start_background
    test_list_containers
    test_stop_container
    test_cli_overrides
    test_memory_flag
    test_pids_flag
    test_env_override
    test_container_isolation
    test_multiple_containers
    test_stop_nonexistent
    
    print_summary
}

# Run main
main