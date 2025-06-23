#!/bin/sh
# setup.sh - Install dependencies for building and testing this project.
# This script installs clang and related tools so that the sources can be
# compiled as C++23 and analyzed with clang-tidy and clang-format.

set -euo pipefail

# Required packages:
# - libsodium-dev/libsodium23 for encryption features used by unit tests and
#   networking components.

# Update package lists.
sudo apt-get update

# Install build utilities, analysis tools and documentation generators.
# Prefer clang-18. If unavailable try clang-20 from the LLVM apt repository
# and finally fall back to the distro default clang package.
if ! sudo apt-get install -y --no-install-recommends clang-18 lld-18; then
    # Attempt to use the official LLVM installer for clang-20.
    if curl -fsSL https://apt.llvm.org/llvm.sh | sudo bash -s -- 20; then
        sudo apt-get install -y --no-install-recommends clang-20 lld-20
    else
        sudo apt-get install -y --no-install-recommends clang lld
    fi
fi

sudo apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    nasm \
    clang-tidy \
    clang-format \
    clang-tools \
    clangd \
    llvm \
    llvm-dev \
    libclang-dev \
    libclang-cpp-dev \
    cppcheck \
    valgrind \
    lcov \
    strace \
    gdb \
    rustc \
    cargo \
    rustfmt \
    g++ \
    afl++ \
    ninja-build \
    doxygen \
    graphviz \
    python3-sphinx \
    python3-breathe \
    python3-sphinx-rtd-theme \
    python3-pip \
    # libsodium provides crypto primitives required by lattice networking
    libsodium-dev \
    libsodium23 \
    # JSON parser required by service manager
    nlohmann-json3-dev \
    qemu-system-x86 \
    qemu-utils \
    qemu-user \
    tmux \
    cloc \
    cscope \
    gcc-x86-64-linux-gnu \
    g++-x86-64-linux-gnu \
    binutils-x86-64-linux-gnu

# Ensure ack is installed for convenient searching
if ! command -v ack >/dev/null 2>&1; then
    sudo apt-get install -y --no-install-recommends ack
fi

# Install optional ack helpers for Python and Node
# ack helpers are optional; skip if pip or npm are restricted

# Install lizard via pip for complexity metrics
python3 -m pip install --user lizard

# Attempt to install libfuzzer development package if available.
sudo apt-get install -y --no-install-recommends libfuzzer-dev || true

