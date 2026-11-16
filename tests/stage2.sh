#!/bin/bash

# Test for Stage 2: Lifecycle Management and Resource Limits
# Verifies start, list, stop, and cgroup enforcement.

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

BINARY="./build/mun_os"
CONFIG_FILE="./tests/stage2_lifecycle_and_limits.json"
CONTAINER_NAME="stage2-lifecycle-test"

echo -e "${YELLOW}===== Running Stage 2 Test: Lifecycle & Limits =====${NC}"

cleanup() {
    echo "--- Cleaning up..."
    sudo $BINARY stop $CONTAINER_NAME > /dev/null 2>&1 || true
    sudo $BINARY rm $CONTAINER_NAME > /dev/null 2>&1 || true
}
trap cleanup EXIT

# 1. Test Memory Limit
echo "--- Testing Memory Limit (should be killed)..."
OUTPUT=$(sudo $BINARY run --config $CONFIG_FILE 2>&1 || true)
if echo "$OUTPUT" | grep -q -i "Killed"; then
    echo -e "${GREEN}✓ Memory limit PASSED (Process was killed as expected)${NC}"
else
    echo -e "${RED}✗ Memory limit FAILED (Process was not killed)${NC}"
    exit 1
fi

# 2. Test Lifecycle
echo "--- Testing container lifecycle (start, list, stop)..."
# Use a different config for lifecycle test
LIFECYCLE_CONFIG="./tests/stage3_exec.json" # Re-using this as it's a sleep command
LIFECYCLE_NAME="stage3-exec-target"

sudo $BINARY start --config $LIFECYCLE_CONFIG
sleep 1

# Check if running
if $BINARY list | grep -q "$LIFECYCLE_NAME" && $BINARY list | grep -q "running"; then
    echo -e "${GREEN}✓ 'start' and 'list' PASSED${NC}"
else
    echo -e "${RED}✗ 'start' or 'list' FAILED${NC}"
    exit 1
fi

# Stop the container
sudo $BINARY stop $LIFECYCLE_NAME
sleep 1

# Check if stopped
if $BINARY list | grep -q "$LIFECYCLE_NAME" && $BINARY list | grep -q "stopped"; then
    echo -e "${GREEN}✓ 'stop' PASSED${NC}"
else
    echo -e "${RED}✗ 'stop' FAILED${NC}"
    exit 1
fi

echo "------------------------"
echo -e "${GREEN}Stage 2 Test: ALL PASSED${NC}"
exit 0
