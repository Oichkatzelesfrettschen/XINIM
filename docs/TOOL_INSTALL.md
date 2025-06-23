# Toolchain Installation

The `tools/setup.sh` script installs all dependencies automatically. When
network access is available, run:

```bash
./tools/setup.sh
```

For manual installation on Ubuntu systems, execute:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake nasm ninja-build \
    clang-18 lld-18 lldb-18 clang-tidy clang-format clang-tools clangd \
    llvm llvm-dev libclang-dev libclang-cpp-dev \
    gcc-x86-64-linux-gnu g++-x86-64-linux-gnu binutils-x86-64-linux-gnu \
    cppcheck valgrind lcov strace gdb \
    doxygen graphviz python3-sphinx python3-breathe python3-sphinx-rtd-theme \
    python3-pip libsodium-dev libsodium23 nlohmann-json3-dev \
    qemu-system-x86 qemu-utils qemu-user \
    rustc cargo rustfmt afl++ tmux cloc cscope ack \
    linux-tools-common linux-tools-generic linux-tools-$(uname -r)
```

These packages supply the full Clang/LLVM tool suite, analysis utilities,
QEMU for virtualization, and standard Linux performance tools. Install
`libfuzzer-dev` if available for fuzzing support, and use `pip install --user
lizard` to enable complexity metrics.
