# Development Environment Setup

This document outlines the steps previously automated by `setup.sh` for
provisioning the development environment. Execute each command manually as
needed.

## Update Package Lists

```bash
sudo apt-get update || echo "Warning: apt update failed; using stale lists"
```

## Install Clang/LLVM Toolchain

```bash
sudo apt-get install -y --no-install-recommends clang-18 lld-18 lldb-18 \
  || (curl -fsSL https://apt.llvm.org/llvm.sh | sudo bash -s -- 20 && \
      sudo apt-get install -y --no-install-recommends clang-20 lld-20 lldb-20) \
  || sudo apt-get install -y --no-install-recommends clang lld lldb
```

## Core Build and Analysis Packages

```bash
sudo apt-get install -y --no-install-recommends \
  build-essential cmake nasm \
  clang-tidy clang-format clang-tools clangd \
  llvm llvm-dev libclang-dev libclang-cpp-dev \
  cppcheck valgrind lcov strace gdb \
  rustc cargo rustfmt \
  g++ afl++ ninja-build \
  doxygen graphviz \
  python3-sphinx python3-breathe python3-sphinx-rtd-theme \
  python3-pip \
  libsodium-dev libsodium23 nlohmann-json3-dev \
  qemu-system-x86 qemu-utils qemu-user \
  tmux cloc cscope \
  gcc-x86-64-linux-gnu g++-x86-64-linux-gnu binutils-x86-64-linux-gnu
```

## Optional Extras

```bash
command -v ack >/dev/null 2>&1 \
  || sudo apt-get install -y --no-install-recommends ack
sudo apt-get install -y --no-install-recommends libfuzzer-dev || true
python3 -m pip install --user --quiet lizard
npm install --global markdownlint-cli
```

## Verify Tool Versions

```bash
clang++ --version | head -n1
ld.lld --version | head -n1
lldb --version | head -n1
```

The environment is ready for building with Clang and Ninja.
