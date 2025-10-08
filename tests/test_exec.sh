#!/bin/bash

# Test script for container exec functionality

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

BINARY="./build/mun_os"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         Testing Container Exec Functionality              ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}\n"

if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Please run as root (use sudo)${NC}"
    exit 1
fi

# Cleanup
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    $BINARY stop test_exec 2>/dev/null || true
    $BINARY remove test_exec 2>/dev/null || true
}

trap cleanup EXIT

echo -e "${YELLOW}Step 1: Creating test container${NC}"
cat > /tmp/test_exec.json << 'EOF'
{
  "hostname": "test-exec-container",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["300"],
  "memory_limit_mb": 128,
  "process_limit": 50
}
EOF

$BINARY start --config /tmp/test_exec.json
sleep 2

echo -e "\n${YELLOW}Step 2: Listing containers${NC}"
$BINARY list

echo -e "\n${YELLOW}Step 3: Testing simple echo command${NC}"
echo -e "${BLUE}Command: exec test_exec /bin/echo 'Hello from container'${NC}"
$BINARY exec test_exec /bin/echo "Hello from container"

echo -e "\n${YELLOW}Step 4: Testing ls command${NC}"
echo -e "${BLUE}Command: exec test_exec /bin/ls -la /${NC}"
$BINARY exec test_exec /bin/ls -la /

echo -e "\n${YELLOW}Step 5: Testing hostname${NC}"
echo -e "${BLUE}Command: exec test_exec /bin/hostname${NC}"
$BINARY exec test_exec /bin/hostname

echo -e "\n${YELLOW}Step 6: Testing pwd${NC}"
echo -e "${BLUE}Command: exec test_exec /bin/pwd${NC}"
$BINARY exec test_exec /bin/pwd

echo -e "\n${YELLOW}Step 7: Testing env${NC}"
echo -e "${BLUE}Command: exec test_exec /bin/env${NC}"
$BINARY exec test_exec /bin/env | head -10

echo -e "\n${YELLOW}Step 8: Testing ps (if available)${NC}"
echo -e "${BLUE}Command: exec test_exec /bin/ps${NC}"
$BINARY exec test_exec /bin/ps || echo -e "${YELLOW}ps command not available or failed${NC}"

echo -e "\n${YELLOW}Step 9: Testing interactive shell (5 second timeout)${NC}"
echo -e "${BLUE}Command: exec test_exec /bin/sh${NC}"
echo -e "${YELLOW}Type 'exit' or wait 5 seconds...${NC}"
timeout 5 $BINARY exec test_exec /bin/sh || true

echo -e "\n${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║              Exec Tests Completed!                         ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}\n"

rm -f /tmp/test_exec.json