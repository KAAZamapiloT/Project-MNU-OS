#!/bin/bash

# Test for Stage 1: Core Isolation (Robust Version)
# Verifies PID, UTS, and Mount namespaces using the 'exec' command.

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

BINARY="./build/mun_os"
# We will now generate the config file within the script to ensure correct formatting
CONFIG_DIR="./tests"
CONFIG_FILE="${CONFIG_DIR}/stage1_test_config.json"

# --- FIX: The container name must match the filename of the config, without the extension ---
# This aligns with the naming logic in your C++ code.
CONTAINER_NAME=$(basename "$CONFIG_FILE" .json)

echo -e "${YELLOW}===== Running Stage 1 Test: Core Isolation =====${NC}"

# --- Generate the JSON config file on the fly ---
generate_config() {
    echo "--- Generating temporary test config file at ${CONFIG_FILE}"
    mkdir -p "$CONFIG_DIR"
    cat > "$CONFIG_FILE" <<EOF
{
  "hostname": "stage1-isolation-test",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": [
    "600"
  ]
}
EOF
}

cleanup() {
    echo "--- Cleaning up Stage 1 container..."
    sudo $BINARY stop $CONTAINER_NAME > /dev/null 2>&1 || true
    sudo $BINARY rm $CONTAINER_NAME > /dev/null 2>&1 || true
    # Clean up the generated config file
    rm -f "$CONFIG_FILE"
}
trap cleanup EXIT

# Generate the config before starting
generate_config

# 1. Start a simple, long-running container
echo "--- Starting a background container to test against..."
# Note: The container name is derived from the filename, so it will now correctly be "stage1_test_config"
sudo $BINARY start --config $CONFIG_FILE
sleep 1
if ! $BINARY list | grep -q "$CONTAINER_NAME.*running"; then
    echo -e "${RED}✗ FAILED to start the target container for testing.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Target container is running.${NC}"

# 2. Perform Atomic Verifications using 'exec'
echo "--- Verifying Results ---"
SUCCESS=true

# Check hostname
echo "  - Verifying Hostname isolation..."
HOSTNAME_OUTPUT=$(sudo $BINARY exec $CONTAINER_NAME hostname)
# The hostname in the generated config is "stage1-isolation-test"
if [[ "$HOSTNAME_OUTPUT" == "stage1-isolation-test" ]]; then
    echo -e "    ${GREEN}✓ Hostname isolation PASSED${NC}"
else
    echo -e "    ${RED}✗ Hostname isolation FAILED (Expected 'stage1-isolation-test', got '$HOSTNAME_OUTPUT')${NC}"
    SUCCESS=false
fi

# Check filesystem
echo "  - Verifying Filesystem isolation..."
LS_OUTPUT=$(sudo $BINARY exec $CONTAINER_NAME ls /)
if echo "$LS_OUTPUT" | grep -q "bin" && echo "$LS_OUTPUT" | grep -q "etc"; then
    echo -e "    ${GREEN}✓ Filesystem isolation PASSED${NC}"
else
    echo -e "    ${RED}✗ Filesystem isolation FAILED${NC}"
    SUCCESS=false
fi

# Check PID
echo "  - Verifying PID isolation..."
PS_OUTPUT=$(sudo $BINARY exec $CONTAINER_NAME ps)
if echo "$PS_OUTPUT" | grep -w "1" | grep -q "/bin/sleep"; then
    echo -e "    ${GREEN}✓ PID isolation PASSED${NC}"
else
    echo -e "    ${RED}✗ PID isolation FAILED (Expected PID 1 to be '/bin/sleep')${NC}"
    echo "      Actual 'ps' output:"
    echo "$PS_OUTPUT"
    SUCCESS=false
fi

echo "------------------------"
if [ "$SUCCESS" = true ]; then
    echo -e "${GREEN}Stage 1 Test: ALL PASSED${NC}"
    exit 0
else
    echo -e "${RED}Stage 1 Test: FAILED${NC}"
    exit 1
fi

