#!/bin/bash

# Generate Test Configuration Files for MUN-OS
# This script creates various test configuration files in the configs/ directory

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

CONFIG_DIR="./configs"
ROOTFS_PATH="./rootfs"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}MUN-OS Test Config Generator${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Create configs directory if it doesn't exist
if [ ! -d "$CONFIG_DIR" ]; then
    echo -e "${YELLOW}Creating configs directory...${NC}"
    mkdir -p "$CONFIG_DIR"
fi

# Check if rootfs exists
if [ ! -d "$ROOTFS_PATH" ]; then
    echo -e "${YELLOW}Warning: Rootfs not found at $ROOTFS_PATH${NC}"
    echo -e "${YELLOW}Make sure to set up your rootfs before running containers${NC}\n"
fi

echo -e "${GREEN}Generating test configuration files...${NC}\n"

# 1. Basic example config
echo -e "Creating ${BLUE}example.json${NC} - Basic container configuration"
cat > "$CONFIG_DIR/example.json" << 'EOF'
{
  "hostname": "mun-os-container",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": [],
  "memory_limit_mb": 256,
  "process_limit": 50
}
EOF

# 2. Interactive shell config
echo -e "Creating ${BLUE}shell.json${NC} - Interactive shell container"
cat > "$CONFIG_DIR/shell.json" << 'EOF'
{
  "hostname": "mun-shell",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": [],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

# 3. Background service config
echo -e "Creating ${BLUE}bg.json${NC} - Background service container"
cat > "$CONFIG_DIR/bg.json" << 'EOF'
{
  "hostname": "mun-background",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["3600"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF

# 4. Test command config
echo -e "Creating ${BLUE}test.json${NC} - Test command container"
cat > "$CONFIG_DIR/test.json" << 'EOF'
{
  "hostname": "mun-test",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Testing MUN-OS Container'; echo 'Current directory:'; pwd; echo 'Listing files:'; ls -la /; echo 'Process list:'; ps aux 2>/dev/null || ps; echo 'Done!'"],
  "memory_limit_mb": 256,
  "process_limit": 50
}
EOF

# 5. Echo test config
echo -e "Creating ${BLUE}echo.json${NC} - Simple echo test"
cat > "$CONFIG_DIR/echo.json" << 'EOF'
{
  "hostname": "mun-echo",
  "rootfs_path": "./rootfs",
  "command": "/bin/echo",
  "args": ["Hello from MUN-OS Container!"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF

# 6. Low memory config
echo -e "Creating ${BLUE}low_memory.json${NC} - Low memory container (64MB)"
cat > "$CONFIG_DIR/low_memory.json" << 'EOF'
{
  "hostname": "mun-low-mem",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Low memory container'; free -m 2>/dev/null || echo 'free command not available'"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF

# 7. High memory config
echo -e "Creating ${BLUE}high_memory.json${NC} - High memory container (512MB)"
cat > "$CONFIG_DIR/high_memory.json" << 'EOF'
{
  "hostname": "mun-high-mem",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": [],
  "memory_limit_mb": 512,
  "process_limit": 100
}
EOF

# 8. Limited processes config
echo -e "Creating ${BLUE}limited_pids.json${NC} - Container with limited processes"
cat > "$CONFIG_DIR/limited_pids.json" << 'EOF'
{
  "hostname": "mun-limited-pids",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Limited to 5 processes'; echo 'Current processes:'; ps"],
  "memory_limit_mb": 128,
  "process_limit": 5
}
EOF

# 9. Long running service
echo -e "Creating ${BLUE}long_running.json${NC} - Long running service (sleep 7200)"
cat > "$CONFIG_DIR/long_running.json" << 'EOF'
{
  "hostname": "mun-long-running",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["7200"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF

# 10. Network info test
echo -e "Creating ${BLUE}network_test.json${NC} - Network interface test"
cat > "$CONFIG_DIR/network_test.json" << 'EOF'
{
  "hostname": "mun-network-test",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Network Test'; ip addr 2>/dev/null || ifconfig 2>/dev/null || echo 'No network tools available'"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

# 11. Filesystem exploration
echo -e "Creating ${BLUE}fs_explore.json${NC} - Filesystem exploration"
cat > "$CONFIG_DIR/fs_explore.json" << 'EOF'
{
  "hostname": "mun-fs-explore",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Filesystem Exploration'; echo '--- Root Directory ---'; ls -la /; echo '--- /bin Directory ---'; ls -la /bin 2>/dev/null || echo '/bin not found'; echo '--- /proc Directory ---'; ls /proc 2>/dev/null || echo '/proc not mounted'"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

# 12. Multi-command test
echo -e "Creating ${BLUE}multi_command.json${NC} - Multiple commands test"
cat > "$CONFIG_DIR/multi_command.json" << 'EOF'
{
  "hostname": "mun-multi-cmd",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Step 1: Creating test file'; echo 'test content' > /tmp/test.txt; echo 'Step 2: Reading test file'; cat /tmp/test.txt; echo 'Step 3: Cleaning up'; rm /tmp/test.txt; echo 'Complete!'"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

# 13. Quick test (fast exit)
echo -e "Creating ${BLUE}quick_test.json${NC} - Quick test (exits immediately)"
cat > "$CONFIG_DIR/quick_test.json" << 'EOF'
{
  "hostname": "mun-quick",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Quick test completed'; exit 0"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF

# 14. Environment test
echo -e "Creating ${BLUE}env_test.json${NC} - Environment variables test"
cat > "$CONFIG_DIR/env_test.json" << 'EOF'
{
  "hostname": "mun-env-test",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Environment Variables:'; env | sort; echo 'Hostname:'; hostname"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

# 15. Stress test config
echo -e "Creating ${BLUE}stress_test.json${NC} - Stress test configuration"
cat > "$CONFIG_DIR/stress_test.json" << 'EOF'
{
  "hostname": "mun-stress",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Starting stress test'; i=0; while [ $i -lt 100 ]; do echo 'Iteration: '$i; i=$((i+1)); done; echo 'Stress test complete'"],
  "memory_limit_mb": 256,
  "process_limit": 50
}
EOF

# 16. Minimal config (no limits)
echo -e "Creating ${BLUE}minimal.json${NC} - Minimal configuration (no resource limits)"
cat > "$CONFIG_DIR/minimal.json" << 'EOF'
{
  "hostname": "mun-minimal",
  "rootfs_path": "./rootfs",
  "command": "/bin/echo",
  "args": ["Minimal configuration test"],
  "memory_limit_mb": 0,
  "process_limit": 0
}
EOF

# 17. Alpine package manager test (if using Alpine)
echo -e "Creating ${BLUE}alpine_test.json${NC} - Alpine Linux specific test"
cat > "$CONFIG_DIR/alpine_test.json" << 'EOF'
{
  "hostname": "mun-alpine",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Alpine Linux Test'; cat /etc/alpine-release 2>/dev/null || echo 'Not Alpine Linux'; apk --version 2>/dev/null || echo 'apk not available'"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

# 18. Process tree test
echo -e "Creating ${BLUE}process_tree.json${NC} - Process tree visualization"
cat > "$CONFIG_DIR/process_tree.json" << 'EOF'
{
  "hostname": "mun-proc-tree",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Process Tree:'; ps auxf 2>/dev/null || ps aux 2>/dev/null || ps"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

# 19. Short-lived background service
echo -e "Creating ${BLUE}short_bg.json${NC} - Short-lived background service (30s)"
cat > "$CONFIG_DIR/short_bg.json" << 'EOF'
{
  "hostname": "mun-short-bg",
  "rootfs_path": "./rootfs",
  "command": "/bin/sleep",
  "args": ["30"],
  "memory_limit_mb": 64,
  "process_limit": 10
}
EOF

# 20. Custom hostname test
echo -e "Creating ${BLUE}custom_hostname.json${NC} - Custom hostname test"
cat > "$CONFIG_DIR/custom_hostname.json" << 'EOF'
{
  "hostname": "my-custom-container",
  "rootfs_path": "./rootfs",
  "command": "/bin/sh",
  "args": ["-c", "echo 'Hostname test'; hostname; cat /etc/hostname 2>/dev/null || echo '/etc/hostname not found'"],
  "memory_limit_mb": 128,
  "process_limit": 25
}
EOF

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Configuration files created successfully!${NC}"
echo -e "${GREEN}========================================${NC}\n"

# Print summary
echo -e "${BLUE}Summary:${NC}"
echo "Total config files created: 20"
echo "Location: $CONFIG_DIR/"
echo ""

# List all created configs
echo -e "${BLUE}Available configurations:${NC}"
ls -1 "$CONFIG_DIR"/*.json | while read file; do
    basename "$file"
done

echo ""
echo -e "${YELLOW}Usage examples:${NC}"
echo "  # Run in foreground:"
echo "  sudo ./build/mun_os run --config configs/example.json"
echo ""
echo "  # Start in background:"
echo "  sudo ./build/mun_os start --config configs/bg.json"
echo ""
echo "  # Quick test:"
echo "  sudo ./build/mun_os run --config configs/quick_test.json"
echo ""
echo "  # Interactive shell:"
echo "  sudo ./build/mun_os run --config configs/shell.json"
echo ""
echo -e "${GREEN}Done!${NC}"