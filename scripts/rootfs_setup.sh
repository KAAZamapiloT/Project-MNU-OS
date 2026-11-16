#!/bin/bash

# ==============================================================================
# Project MUN-OS: Root Filesystem Setup Script
#
# This script automates the download and extraction of the Alpine Linux
# mini rootfs, which is required to run containers.
# It ensures a clean, repeatable setup for all team members.
# ==============================================================================

# Exit immediately if any command fails
set -e

# --- Configuration ---
ALPINE_VERSION="3.18.3"
ALPINE_ARCH="x86_64"
TARBALL_NAME="alpine-minirootfs-${ALPINE_VERSION}-${ALPINE_ARCH}.tar.gz"
DOWNLOAD_URL="https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/${ALPINE_ARCH}/${TARBALL_NAME}"

# Determine the project's root directory (where this script is located)
# This makes the script runnable from any location.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
ROOTFS_DIR="${PROJECT_ROOT}/rootfs"

# --- Main Logic ---

echo ">>> Setting up Alpine Linux root filesystem..."
echo "    Project Root: ${PROJECT_ROOT}"
echo "    Target Dir:   ${ROOTFS_DIR}"

# 1. Clean up any previous setup
if [ -d "$ROOTFS_DIR" ]; then
    echo "--- Found existing rootfs directory. Cleaning up first..."
    # 'sudo' is required because the directory contains root-owned files
    sudo rm -rf "$ROOTFS_DIR"
fi

# 2. Create the new rootfs directory
echo "--- Creating new rootfs directory."
mkdir "$ROOTFS_DIR"

# 3. Download the tarball
echo "--- Downloading Alpine Mini Rootfs from ${DOWNLOAD_URL}..."
wget -q --show-progress -P "${PROJECT_ROOT}" "$DOWNLOAD_URL"

# 4. Extract the tarball into the rootfs directory
echo "--- Extracting filesystem. This requires sudo..."
# 'sudo' is critical here to preserve the correct file ownership (root)
# and permissions from the tarball.
sudo tar -xzf "${PROJECT_ROOT}/${TARBALL_NAME}" -C "$ROOTFS_DIR"

# 5. Clean up the downloaded tarball
echo "--- Cleaning up downloaded tarball."
rm "${PROJECT_ROOT}/${TARBALL_NAME}"

echo ""
echo "✅ Rootfs setup complete!"
echo "   You can now build and run your container."

