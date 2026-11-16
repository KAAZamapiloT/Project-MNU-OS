#!/bin/bash

# Test for Stage 3: Interactive Shell Access (exec)
# Verifies that a new process can enter the namespaces of a running container.

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

BINARY="./build/mun_os"
CONFIG_FILE="./tests/stage3_exec.json"
CONTAINER_NAME="stage3-exec-target"

echo -e "${YELLOW}===== Running Stage 3 Test: exec Command =====${NC}"

cleanup() {
    echo "--- Cleaning up..."
    sudo $BINARY stop $CONTAINER_NAME > /dev/null 2>&1 || true
    sudo $BINARY rm $CONTAINER_NAME > /dev/null 2>&1 || true
}
trap cleanup EXIT

# 1. Start a background container
echo "--- Starting a long-running container in the background..."
sudo $BINARY start --config $CONFIG_FILE
sleep 1
if ! $BINARY list | grep -q "$CONTAINER_NAME.*running"; then
    echo -e "${RED}✗ FAILED to start the target container.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Target container is running.${NC}"

# 2. Run exec and check the process list
echo "--- Running 'exec' to get a process list from inside the container..."
EXEC_OUTPUT=$(sudo $BINARY exec $CONTAINER_NAME ps aux)

echo "--- exec Output ---"
echo "$EXEC_OUTPUT"
echo "-------------------"

# 3. Verification
echo "--- Verifying Results ---"
SUCCESS=true

# Check if the original 'sleep' command is visible
if echo "$EXEC_OUTPUT" | grep -q "/bin/sleep 600"; then
    echo -e "${GREEN}✓ Original process (sleep) is visible inside exec. PASSED${NC}"
else
    echo -e "${RED}✗ Original process (sleep) is NOT visible inside exec. FAILED${NC}"
    SUCCESS=false
fi

# Check if the 'ps' command itself is visible
if echo "$EXEC_OUTPUT" | grep -q "ps aux"; then
    echo -e "${GREEN}✓ New process (ps) is visible inside exec. PASSED${NC}"
else
    echo -e "${RED}✗ New process (ps) is NOT visible inside exec. FAILED${NC}"
    SUCCESS=false
fi

echo "------------------------"
if [ "$SUCCESS" = true ]; then
    echo -e "${GREEN}Stage 3 Test: ALL PASSED${NC}"
    exit 0
else
    echo -e "${RED}Stage 3 Test: FAILED${NC}"
    exit 1
fi
