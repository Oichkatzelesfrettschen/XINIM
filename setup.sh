#!/bin/sh
# setup.sh - Install dependencies for building and testing this project.
# This script installs clang and related tools so that the sources can be
# compiled as C90 and analyzed with clang-tidy and clang-format.

set -e

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

# Attempt to install libfuzzer development package if available.
sudo apt-get install -y --no-install-recommends libfuzzer-dev || true

