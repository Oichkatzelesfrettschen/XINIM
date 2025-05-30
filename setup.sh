#!/bin/sh
# setup.sh - Install dependencies for building and testing this project.
# This script installs clang and related tools so that the sources can be
# compiled as C90 and analyzed with clang-tidy and clang-format.

set -euo pipefail

# Update package lists.
sudo apt-get update

# Install build utilities and clang tools.
sudo apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    nasm \
    clang \
    clang-tidy \
    clang-format \
    clang-tools \
    clangd \
    lld \
    llvm \
    llvm-dev \
    libclang-dev

# Ensure ack is installed for convenient searching
if ! command -v ack >/dev/null 2>&1; then
    # Install the ACK compiler suite along with development headers
    sudo apt-get install -y --no-install-recommends \
        ack \
        ack-dev \
        ack-clang \
        ack-grep
fi

# Install optional ack helpers for Python and Node
sudo pip3 install ack
sudo npm install -g ack

# Attempt to install libfuzzer development package if available.
sudo apt-get install -y --no-install-recommends libfuzzer-dev || true

