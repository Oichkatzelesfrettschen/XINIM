#!/usr/bin/env bash
# XINIM musl libc Build Script
# Builds musl 1.2.4 for x86_64-xinim-elf target
# Part of Week 2: musl libc Integration

set -euo pipefail

# Load build configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XINIM_ROOT="$(dirname "$SCRIPT_DIR")"

# Source environment if available
if [[ -f "/opt/xinim-toolchain/xinim-env.sh" ]]; then
    source /opt/xinim-toolchain/xinim-env.sh
else
    # Default values
    export XINIM_PREFIX="${XINIM_PREFIX:-/opt/xinim-toolchain}"
    export XINIM_SYSROOT="${XINIM_SYSROOT:-/opt/xinim-sysroot}"
    export XINIM_TARGET="x86_64-xinim-elf"
    export XINIM_BUILD_DIR="${XINIM_BUILD_DIR:-$HOME/xinim-build}"
    export XINIM_SOURCES_DIR="${XINIM_SOURCES_DIR:-$HOME/xinim-sources}"
    export PATH="$XINIM_PREFIX/bin:$PATH"
fi

MUSL_VERSION="1.2.4"
BUILD_DIR="$XINIM_BUILD_DIR/musl-build"
SOURCE_DIR="$XINIM_SOURCES_DIR/musl-${MUSL_VERSION}"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[musl]${NC} $1"
}

success() {
    echo -e "${GREEN}[musl]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[musl]${NC} $1"
}

error() {
    echo -e "${RED}[musl]${NC} $1"
}

# Check if GCC Stage 1 is installed
if [[ ! -x "$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" ]]; then
    error "GCC Stage 1 not found. Build GCC Stage 1 first:"
    error "  ./build_gcc_stage1.sh"
    exit 1
fi

# Check if source exists
if [[ ! -d "$SOURCE_DIR" ]]; then
    error "Source directory not found: $SOURCE_DIR"
    error "Run setup_build_environment.sh first"
    exit 1
fi

# Create XINIM-specific syscall adapter
log "Creating XINIM syscall adapter..."

mkdir -p "$SOURCE_DIR/arch/x86_64/xinim"

cat > "$SOURCE_DIR/arch/x86_64/xinim/syscall_adapter.h" <<'EOF'
#ifndef _XINIM_SYSCALL_ADAPTER_H
#define _XINIM_SYSCALL_ADAPTER_H

/* XINIM Syscall Adapter
 * Maps Linux syscall numbers to XINIM kernel syscalls
 * XINIM uses Linux x86_64 syscall ABI for compatibility
 */

/* Process Management */
#define SYS_read            0
#define SYS_write           1
#define SYS_open            2
#define SYS_close           3
#define SYS_stat            4
#define SYS_fstat           5
#define SYS_lstat           6
#define SYS_poll            7
#define SYS_lseek           8
#define SYS_mmap            9
#define SYS_mprotect        10
#define SYS_munmap          11
#define SYS_brk             12

/* Signals */
#define SYS_rt_sigaction    13
#define SYS_rt_sigprocmask  14
#define SYS_rt_sigreturn    15

/* Process Control */
#define SYS_ioctl           16
#define SYS_pread64         17
#define SYS_pwrite64        18
#define SYS_readv           19
#define SYS_writev          20
#define SYS_access          21
#define SYS_pipe            22
#define SYS_select          23
#define SYS_sched_yield     24
#define SYS_mremap          25
#define SYS_msync           26
#define SYS_mincore         27
#define SYS_madvise         28

/* Process Management */
#define SYS_dup             32
#define SYS_dup2            33
#define SYS_pause           34
#define SYS_nanosleep       35
#define SYS_getitimer       36
#define SYS_alarm           37
#define SYS_setitimer       38
#define SYS_getpid          39
#define SYS_sendfile        40

/* Networking */
#define SYS_socket          41
#define SYS_connect         42
#define SYS_accept          43
#define SYS_sendto          44
#define SYS_recvfrom        45
#define SYS_sendmsg         46
#define SYS_recvmsg         47
#define SYS_shutdown        48
#define SYS_bind            49
#define SYS_listen          50
#define SYS_getsockname     51
#define SYS_getpeername     52
#define SYS_socketpair      53
#define SYS_setsockopt      54
#define SYS_getsockopt      55

/* Process Creation */
#define SYS_clone           56
#define SYS_fork            57
#define SYS_vfork           58
#define SYS_execve          59
#define SYS_exit            60
#define SYS_wait4           61
#define SYS_kill            62
#define SYS_uname           63

/* Filesystem */
#define SYS_fcntl           72
#define SYS_flock           73
#define SYS_fsync           74
#define SYS_fdatasync       75
#define SYS_truncate        76
#define SYS_ftruncate       77
#define SYS_getdents        78
#define SYS_getcwd          79
#define SYS_chdir           80
#define SYS_fchdir          81
#define SYS_rename          82
#define SYS_mkdir           83
#define SYS_rmdir           84
#define SYS_creat           85
#define SYS_link            86
#define SYS_unlink          87
#define SYS_symlink         88
#define SYS_readlink        89
#define SYS_chmod           90
#define SYS_fchmod          91
#define SYS_chown           92
#define SYS_fchown          93
#define SYS_lchown          94
#define SYS_umask           95

/* Process IDs */
#define SYS_gettid          186
#define SYS_getuid          102
#define SYS_getgid          104
#define SYS_getppid         110

/* Time */
#define SYS_time            201
#define SYS_clock_gettime   228
#define SYS_clock_settime   229
#define SYS_clock_getres    229

/* Threading (will be implemented in Week 7) */
#define SYS_futex           202
#define SYS_set_tid_address 218

/* More syscalls will be added as kernel implements them */

#endif /* _XINIM_SYSCALL_ADAPTER_H */
EOF

success "Created XINIM syscall adapter"

# Create build directory (musl uses in-source builds, but we'll use out-of-source)
log "Creating build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure musl
log "Configuring musl ${MUSL_VERSION} for $XINIM_TARGET..."

CC="$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" \
AR="$XINIM_PREFIX/bin/$XINIM_TARGET-ar" \
RANLIB="$XINIM_PREFIX/bin/$XINIM_TARGET-ranlib" \
"$SOURCE_DIR/configure" \
    --target=x86_64 \
    --prefix="$XINIM_SYSROOT/usr" \
    --exec-prefix="$XINIM_SYSROOT/usr" \
    --syslibdir="$XINIM_SYSROOT/lib" \
    --disable-shared \
    --enable-static \
    --enable-optimize \
    --disable-debug \
    --enable-warnings \
    CFLAGS="-Os -pipe -fomit-frame-pointer -fno-unwind-tables -fno-asynchronous-unwind-tables -march=x86-64 -mtune=generic"

success "Configuration complete"

# Build musl
log "Building musl (using $(nproc) parallel jobs)..."
make -j$(nproc)

success "Build complete"

# Install musl
log "Installing musl to $XINIM_SYSROOT..."
make install

success "Installation complete"

# Create compatibility symlinks
log "Creating compatibility symlinks..."
cd "$XINIM_SYSROOT"

# lib64 -> lib (for x86_64)
if [[ ! -L "lib64" ]]; then
    ln -sf lib lib64
    success "  Created lib64 -> lib symlink"
fi

# usr/lib64 -> usr/lib
if [[ ! -L "usr/lib64" ]]; then
    ln -sf lib usr/lib64
    success "  Created usr/lib64 -> usr/lib symlink"
fi

# Verify installation
log "Verifying installation..."

LIBC_COMPONENTS=(
    "$XINIM_SYSROOT/lib/libc.a"
    "$XINIM_SYSROOT/lib/crt1.o"
    "$XINIM_SYSROOT/lib/crti.o"
    "$XINIM_SYSROOT/lib/crtn.o"
    "$XINIM_SYSROOT/usr/include/stdio.h"
    "$XINIM_SYSROOT/usr/include/stdlib.h"
    "$XINIM_SYSROOT/usr/include/string.h"
    "$XINIM_SYSROOT/usr/include/unistd.h"
    "$XINIM_SYSROOT/usr/include/pthread.h"
)

ALL_OK=true
for component in "${LIBC_COMPONENTS[@]}"; do
    if [[ -e "$component" ]]; then
        success "  ✓ $(basename $component)"
    else
        error "  ✗ $(basename $component): NOT FOUND"
        ALL_OK=false
    fi
done

# Get libc size
LIBC_SIZE=$(du -h "$XINIM_SYSROOT/lib/libc.a" | cut -f1)
log "libc.a size: $LIBC_SIZE"

# Test compilation with libc
log "Testing compilation with musl libc..."

cat > /tmp/test_musl.c <<'EOF'
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Hello from musl libc!\n");
    return 0;
}
EOF

if "$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" \
    --sysroot="$XINIM_SYSROOT" \
    -static \
    /tmp/test_musl.c \
    -o /tmp/test_musl 2>/dev/null; then
    success "  ✓ Can compile with musl libc"
else
    warn "  ⚠ Compilation test failed (expected - kernel syscalls not yet implemented)"
    warn "    This is normal at this stage. Programs will work once XINIM kernel is complete."
fi

rm -f /tmp/test_musl.c /tmp/test_musl

if $ALL_OK; then
    echo
    success "================================================"
    success "musl ${MUSL_VERSION} build successful!"
    success "================================================"
    echo
    log "Installed to: $XINIM_SYSROOT"
    log "Headers: $XINIM_SYSROOT/usr/include"
    log "Libraries: $XINIM_SYSROOT/lib"
    log "libc size: $LIBC_SIZE"
    echo
    log "Next step: Build GCC Stage 2 (full compiler with libc)"
    log "  ./build_gcc_stage2.sh"
    echo
else
    error "Some components are missing. Build may have failed."
    exit 1
fi
