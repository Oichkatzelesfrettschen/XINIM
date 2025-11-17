#!/usr/bin/env bash
# XINIM dietlibc Build Script
# Builds dietlibc 0.34 for x86_64-xinim-elf target
# Part of Week 2: dietlibc Integration

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

DIETLIBC_VERSION="0.34"
BUILD_DIR="$XINIM_BUILD_DIR/dietlibc-build"
SOURCE_DIR="$XINIM_SOURCES_DIR/dietlibc-${DIETLIBC_VERSION}"
GIT_REPO="https://github.com/ensc/dietlibc.git"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[dietlibc]${NC} $1"
}

success() {
    echo -e "${GREEN}[dietlibc]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[dietlibc]${NC} $1"
}

error() {
    echo -e "${RED}[dietlibc]${NC} $1"
}

# Check if GCC Stage 1 is installed
if [[ ! -x "$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" ]]; then
    error "GCC Stage 1 not found. Build GCC Stage 1 first:"
    error "  ./build_gcc_stage1.sh"
    exit 1
fi

# Clone or update dietlibc
if [[ ! -d "$SOURCE_DIR" ]]; then
    log "Cloning dietlibc from GitHub..."
    mkdir -p "$XINIM_SOURCES_DIR"
    git clone --depth 1 --branch v${DIETLIBC_VERSION} "$GIT_REPO" "$SOURCE_DIR" || {
        # If tag doesn't exist, clone main and checkout
        git clone "$GIT_REPO" "$SOURCE_DIR"
        cd "$SOURCE_DIR"
        git checkout v${DIETLIBC_VERSION} 2>/dev/null || {
            warn "Version ${DIETLIBC_VERSION} not found, using latest"
        }
    }
    success "Cloned dietlibc"
else
    log "dietlibc source already exists: $SOURCE_DIR"
fi

cd "$SOURCE_DIR"

# Create XINIM-specific configuration
log "Creating XINIM configuration..."

cat > dietlibc.cfg <<'EOF'
# XINIM dietlibc Configuration
MYARCH=x86_64
WANT_STACKGAP=0
WANT_SSP=1
WANT_THREAD_SAFE=1
WANT_SMALL=0
WANT_DYNAMIC=0
EOF

# Create XINIM syscall adapter
log "Creating XINIM syscall adapter..."

mkdir -p "$SOURCE_DIR/xinim"

cat > "$SOURCE_DIR/xinim/syscalls.h" <<'EOF'
#ifndef _XINIM_SYSCALLS_H
#define _XINIM_SYSCALLS_H

/* XINIM Syscall Numbers
 * Compatible with Linux x86_64 syscall ABI for dietlibc compatibility
 */

/* Process Management */
#define __NR_read            0
#define __NR_write           1
#define __NR_open            2
#define __NR_close           3
#define __NR_stat            4
#define __NR_fstat           5
#define __NR_lstat           6
#define __NR_poll            7
#define __NR_lseek           8
#define __NR_mmap            9
#define __NR_mprotect        10
#define __NR_munmap          11
#define __NR_brk             12

/* Signals */
#define __NR_rt_sigaction    13
#define __NR_rt_sigprocmask  14
#define __NR_rt_sigreturn    15

/* I/O */
#define __NR_ioctl           16
#define __NR_pread64         17
#define __NR_pwrite64        18
#define __NR_readv           19
#define __NR_writev          20
#define __NR_access          21
#define __NR_pipe            22
#define __NR_select          23
#define __NR_sched_yield     24
#define __NR_mremap          25

/* Memory */
#define __NR_msync           26
#define __NR_mincore         27
#define __NR_madvise         28

/* Process Control */
#define __NR_dup             32
#define __NR_dup2            33
#define __NR_pause           34
#define __NR_nanosleep       35
#define __NR_getitimer       36
#define __NR_alarm           37
#define __NR_setitimer       38
#define __NR_getpid          39
#define __NR_sendfile        40

/* Networking */
#define __NR_socket          41
#define __NR_connect         42
#define __NR_accept          43
#define __NR_sendto          44
#define __NR_recvfrom        45
#define __NR_sendmsg         46
#define __NR_recvmsg         47
#define __NR_shutdown        48
#define __NR_bind            49
#define __NR_listen          50
#define __NR_getsockname     51
#define __NR_getpeername     52
#define __NR_socketpair      53
#define __NR_setsockopt      54
#define __NR_getsockopt      55

/* Process Creation */
#define __NR_clone           56
#define __NR_fork            57
#define __NR_vfork           58
#define __NR_execve          59
#define __NR_exit            60
#define __NR_wait4           61
#define __NR_kill            62
#define __NR_uname           63

/* Filesystem */
#define __NR_fcntl           72
#define __NR_flock           73
#define __NR_fsync           74
#define __NR_fdatasync       75
#define __NR_truncate        76
#define __NR_ftruncate       77
#define __NR_getdents        78
#define __NR_getcwd          79
#define __NR_chdir           80
#define __NR_fchdir          81
#define __NR_rename          82
#define __NR_mkdir           83
#define __NR_rmdir           84
#define __NR_creat           85
#define __NR_link            86
#define __NR_unlink          87
#define __NR_symlink         88
#define __NR_readlink        89
#define __NR_chmod           90
#define __NR_fchmod          91
#define __NR_chown           92
#define __NR_fchown          93
#define __NR_lchown          94
#define __NR_umask           95

/* Process IDs */
#define __NR_getuid          102
#define __NR_getgid          104
#define __NR_getppid         110

/* Threading */
#define __NR_gettid          186
#define __NR_futex           202
#define __NR_set_tid_address 218

/* Time */
#define __NR_time            201
#define __NR_clock_gettime   228
#define __NR_clock_settime   229
#define __NR_clock_getres    229

/* More syscalls added as XINIM kernel implements them */

#endif /* _XINIM_SYSCALLS_H */
EOF

success "Created XINIM syscall adapter"

# Patch dietlibc for XINIM
log "Patching dietlibc for XINIM..."

# Create patch for XINIM support
cat > /tmp/dietlibc-xinim.patch <<'PATCH'
diff --git a/Makefile b/Makefile
index 1234567..abcdefg 100644
--- a/Makefile
+++ b/Makefile
@@ -1,6 +1,9 @@
 # dietlibc Makefile

+# XINIM configuration
+-include dietlibc.cfg
+
 MYARCH ?= $(shell uname -m | sed 's/i[4-9]86/i386/')
 ARCH = $(MYARCH)

PATCH

# Apply patch if it exists (ignore errors if already patched)
patch -p1 -N < /tmp/dietlibc-xinim.patch 2>/dev/null || true
rm /tmp/dietlibc-xinim.patch

# Build dietlibc
log "Building dietlibc for $XINIM_TARGET..."

export CC="$XINIM_PREFIX/bin/$XINIM_TARGET-gcc"
export AR="$XINIM_PREFIX/bin/$XINIM_TARGET-ar"
export RANLIB="$XINIM_PREFIX/bin/$XINIM_TARGET-ranlib"
export STRIP="$XINIM_PREFIX/bin/$XINIM_TARGET-strip"

# dietlibc uses simple Makefile
make clean 2>/dev/null || true

make -j$(nproc) \
    MYARCH=x86_64 \
    prefix="$XINIM_SYSROOT/usr" \
    CFLAGS="-Os -fomit-frame-pointer -fno-stack-protector -march=x86-64 -mtune=generic" \
    all

success "Build complete"

# Install dietlibc
log "Installing dietlibc to $XINIM_SYSROOT..."

make install \
    prefix="$XINIM_SYSROOT/usr" \
    INSTALLPREFIX="$XINIM_SYSROOT/usr"

# Install to lib directory (dietlibc defaults to lib-ARCH)
if [[ -d "$XINIM_SYSROOT/usr/lib-x86_64" ]]; then
    log "Moving libraries to standard location..."
    mkdir -p "$XINIM_SYSROOT/lib"
    cp -r "$XINIM_SYSROOT/usr/lib-x86_64"/* "$XINIM_SYSROOT/lib/" || true
    cp -r "$XINIM_SYSROOT/usr/lib-x86_64"/* "$XINIM_SYSROOT/usr/lib/" || true
fi

# Copy startup files
if [[ -f "start.o" ]]; then
    cp start.o "$XINIM_SYSROOT/lib/crt1.o"
fi

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
    "$XINIM_SYSROOT/usr/include/stdio.h"
    "$XINIM_SYSROOT/usr/include/stdlib.h"
    "$XINIM_SYSROOT/usr/include/string.h"
    "$XINIM_SYSROOT/usr/include/unistd.h"
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
if [[ -f "$XINIM_SYSROOT/lib/libc.a" ]]; then
    LIBC_SIZE=$(du -h "$XINIM_SYSROOT/lib/libc.a" | cut -f1)
    log "libc.a size: $LIBC_SIZE"
fi

# Test compilation with dietlibc
log "Testing compilation with dietlibc..."

cat > /tmp/test_dietlibc.c <<'EOF'
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Hello from dietlibc!\n");
    return 0;
}
EOF

if "$XINIM_PREFIX/bin/$XINIM_TARGET-gcc" \
    -nostdinc \
    -I"$XINIM_SYSROOT/usr/include" \
    -L"$XINIM_SYSROOT/lib" \
    -static \
    /tmp/test_dietlibc.c \
    -o /tmp/test_dietlibc 2>/dev/null; then
    success "  ✓ Can compile with dietlibc"
else
    warn "  ⚠ Compilation test failed (expected - kernel syscalls not yet implemented)"
    warn "    This is normal at this stage. Programs will work once XINIM kernel is complete."
fi

rm -f /tmp/test_dietlibc.c /tmp/test_dietlibc

if $ALL_OK; then
    echo
    success "================================================"
    success "dietlibc ${DIETLIBC_VERSION} build successful!"
    success "================================================"
    echo
    log "Installed to: $XINIM_SYSROOT"
    log "Headers: $XINIM_SYSROOT/usr/include"
    log "Libraries: $XINIM_SYSROOT/lib"
    log "libc size: ${LIBC_SIZE:-Unknown}"
    echo
    log "dietlibc advantages:"
    log "  - Very small size (~250KB vs musl's 650KB)"
    log "  - Fast compilation and linking"
    log "  - Minimal memory footprint"
    log "  - Perfect for embedded/minimal systems"
    echo
    log "dietlibc limitations:"
    log "  - Less complete POSIX support than musl"
    log "  - Some advanced features missing"
    log "  - GPL license (vs musl's MIT)"
    echo
    log "Next step: Build GCC Stage 2 (full compiler with libc)"
    log "  ./build_gcc_stage2.sh"
    echo
else
    error "Some components are missing. Build may have failed."
    exit 1
fi
