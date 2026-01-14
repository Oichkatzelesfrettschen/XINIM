# XINIM Development Container
# Optimized for C++23 development with QEMU debugging support

FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="3.28.0"

# Optionally install the cmake for vcpkg
COPY ./reinstall-cmake.sh /tmp/

RUN if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
        chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh

# Install XINIM development dependencies
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
        # Core build tools
        ninja-build \
        nasm \
        # Cryptography library
        libsodium-dev \
        # QEMU for kernel testing
        qemu-system-x86 \
        qemu-utils \
        # Debugging tools
        gdb \
        valgrind \
        strace \
        # Static analysis
        cppcheck \
        # Coverage tools
        lcov \
        # Python for tooling
        python3-pip \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*

# Install xmake (primary build system)
RUN curl -fsSL https://xmake.io/shget.text | bash \
    && ln -s /root/.local/bin/xmake /usr/local/bin/xmake

# Install Python tools
RUN pip3 install --break-system-packages --no-cache-dir \
    lizard \
    pytest

# Configure GDB for kernel debugging
RUN echo 'set architecture i386:x86-64' >> /home/vscode/.gdbinit \
    && echo 'set disassembly-flavor intel' >> /home/vscode/.gdbinit \
    && echo 'set pagination off' >> /home/vscode/.gdbinit \
    && chown vscode:vscode /home/vscode/.gdbinit
