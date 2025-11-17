# dietlibc Integration Guide for XINIM

**Version:** 1.0
**Date:** 2025-11-17
**Target:** XINIM OS developers integrating dietlibc into userland

---

## Overview

This guide explains how to integrate dietlibc 0.34 into XINIM's userland, including:
1. Building userland programs with dietlibc
2. Syscall adapter layer for XINIM kernel
3. POSIX compliance extensions
4. Performance optimization
5. Debugging and testing

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [Compiler Configuration](#2-compiler-configuration)
3. [XINIM Syscall Adapter](#3-xinim-syscall-adapter)
4. [Building Userland Programs](#4-building-userland-programs)
5. [POSIX Compliance Extensions](#5-posix-compliance-extensions)
6. [Performance Tuning](#6-performance-tuning)
7. [Debugging with dietlibc](#7-debugging-with-dietlibc)
8. [Common Issues](#8-common-issues)

---

## 1. Quick Start

### 1.1 Prerequisites

Ensure the toolchain is built:
```bash
cd /home/user/XINIM/scripts

# 1. Setup environment
sudo ./setup_build_environment.sh

# 2. Source environment variables
source /opt/xinim-toolchain/xinim-env.sh

# 3. Build toolchain in order
./build_binutils.sh       # ~10 minutes
./build_gcc_stage1.sh     # ~30 minutes
./build_dietlibc.sh       # ~5 minutes
./build_gcc_stage2.sh     # ~60 minutes
```

### 1.2 Verify Installation

```bash
# Check dietlibc installation
ls -lh /opt/xinim-sysroot/lib/libc.a
# Should show: ~200-300 KB

# Check headers
ls /opt/xinim-sysroot/usr/include/stdio.h
ls /opt/xinim-sysroot/usr/include/stdlib.h

# Test compilation
x86_64-xinim-elf-gcc --version
# Should show: GCC 13.2.0
```

---

## 2. Compiler Configuration

### 2.1 Environment Variables

After sourcing `/opt/xinim-toolchain/xinim-env.sh`, you'll have:

```bash
export XINIM_PREFIX=/opt/xinim-toolchain
export XINIM_SYSROOT=/opt/xinim-sysroot
export XINIM_TARGET=x86_64-xinim-elf
export CC=x86_64-xinim-elf-gcc
export CXX=x86_64-xinim-elf-g++
export AR=x86_64-xinim-elf-ar
export LD=x86_64-xinim-elf-ld
export RANLIB=x86_64-xinim-elf-ranlib
export PATH=/opt/xinim-toolchain/bin:$PATH
```

### 2.2 GCC Flags for dietlibc

**Standard flags:**
```bash
CFLAGS="-Os -fomit-frame-pointer -fno-stack-protector \
        -march=x86-64 -mtune=generic \
        --sysroot=/opt/xinim-sysroot"
```

**Explanation:**
- `-Os`: Optimize for size (dietlibc's strength)
- `-fomit-frame-pointer`: Save one register, slightly smaller code
- `-fno-stack-protector`: dietlibc doesn't have stack protector support
- `-march=x86-64`: Baseline x86-64 (maximum compatibility)
- `--sysroot=/opt/xinim-sysroot`: Use dietlibc headers and libraries

**Linker flags:**
```bash
LDFLAGS="-static -L/opt/xinim-sysroot/lib"
```

**Explanation:**
- `-static`: Statically link libc (XINIM has no dynamic linker yet)
- `-L/opt/xinim-sysroot/lib`: Search for libraries in sysroot

---

## 3. XINIM Syscall Adapter

### 3.1 Syscall Number Mapping

XINIM uses Linux x86_64 syscall numbers for compatibility with dietlibc.

**Location:** `/opt/xinim-sysroot/usr/include/xinim/syscalls.h`

**Sample mapping:**
```c
#define __NR_read            0
#define __NR_write           1
#define __NR_open            2
#define __NR_close           3
#define __NR_stat            4
// ... 300 total syscalls
```

**Why Linux-compatible?**
- dietlibc expects Linux syscall numbers
- Allows reuse of dietlibc's syscall wrappers
- Simplifies porting of Linux software

### 3.2 Syscall Convention (x86_64)

**Register usage:**
```
RAX = syscall number
RDI = arg1
RSI = arg2
RDX = arg3
R10 = arg4
R8  = arg5
R9  = arg6
Return value = RAX (negative for error)
```

**Example (read syscall):**
```c
// dietlibc wrapper (generated)
ssize_t read(int fd, void* buf, size_t count) {
    register long rax asm("rax") = __NR_read;
    register long rdi asm("rdi") = fd;
    register long rsi asm("rsi") = (long)buf;
    register long rdx asm("rdx") = count;

    asm volatile(
        "syscall"
        : "+r"(rax)
        : "r"(rdi), "r"(rsi), "r"(rdx)
        : "rcx", "r11", "memory"
    );

    if (rax < 0) {
        errno = -rax;
        return -1;
    }
    return rax;
}
```

### 3.3 XINIM Kernel Syscall Dispatcher

**Location:** `src/kernel/syscall.cpp` (to be implemented in Week 5)

**Dispatch table:**
```cpp
namespace xinim::syscall {

using syscall_fn_t = int64_t (*)(uint64_t, uint64_t, uint64_t,
                                  uint64_t, uint64_t, uint64_t);

static constexpr syscall_fn_t syscall_table[] = {
    sys_read,    // 0
    sys_write,   // 1
    sys_open,    // 2
    sys_close,   // 3
    // ... 300 entries
};

int64_t dispatch(uint64_t syscall_num, uint64_t arg1, uint64_t arg2,
                  uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    if (syscall_num >= std::size(syscall_table)) {
        return -ENOSYS;
    }

    return syscall_table[syscall_num](arg1, arg2, arg3, arg4, arg5, arg6);
}

} // namespace xinim::syscall
```

**Called from assembly:**
```asm
; x86_64 syscall entry point
syscall_entry:
    ; Save user context
    swapgs
    mov [gs:current_task_rsp], rsp
    mov rsp, [gs:kernel_stack]

    push r11  ; Save RFLAGS
    push rcx  ; Save RIP

    ; RAX = syscall number
    ; RDI, RSI, RDX, R10, R8, R9 = args
    ; R10 -> RCX for kernel calling convention
    mov rcx, r10

    ; Call C++ dispatcher
    call xinim::syscall::dispatch

    ; Restore user context and return
    pop rcx
    pop r11
    mov rsp, [gs:current_task_rsp]
    swapgs
    sysretq
```

---

## 4. Building Userland Programs

### 4.1 Simple C Program

**hello.c:**
```c
#include <stdio.h>

int main(void) {
    printf("Hello from XINIM with dietlibc!\n");
    return 0;
}
```

**Build:**
```bash
x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    -Os -static \
    hello.c -o hello

# Check binary size
ls -lh hello
# Should be ~8-10 KB (much smaller than with musl/glibc)

# Verify it's x86_64 ELF
file hello
# Output: ELF 64-bit LSB executable, x86-64
```

### 4.2 C++ Program

**hello.cpp:**
```cpp
#include <iostream>

int main() {
    std::cout << "Hello from XINIM with dietlibc!" << std::endl;
    return 0;
}
```

**Build:**
```bash
x86_64-xinim-elf-g++ --sysroot=/opt/xinim-sysroot \
    -Os -static \
    hello.cpp -o hello_cpp

# Size will be larger due to libstdc++
ls -lh hello_cpp
# ~100-150 KB (still smaller than musl/glibc)
```

### 4.3 Using xmake

**xmake.lua:**
```lua
set_toolchains("cross", {
    sdkdir = "/opt/xinim-toolchain",
    sysroot = "/opt/xinim-sysroot"
})

target("hello")
    set_kind("binary")
    set_targetdir("$(buildir)/bin")
    add_files("src/hello.c")
    add_cflags("-Os", "-static", {force = true})
    add_ldflags("-static", {force = true})
```

**Build:**
```bash
xmake
# Output: build/bin/hello
```

### 4.4 XINIM Userland Utilities

**Location:** `userland/coreutils/`

**Example: cat implementation**
```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int cat(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(filename);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t n;

    while ((n = read(fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(STDOUT_FILENO, buffer, n) != n) {
            perror("write");
            close(fd);
            return 1;
        }
    }

    if (n < 0) {
        perror("read");
    }

    close(fd);
    return (n < 0) ? 1 : 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file...>\n", argv[0]);
        return 1;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        ret |= cat(argv[i]);
    }

    return ret;
}
```

**Build:**
```bash
x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    -Os -static \
    cat.c -o cat

ls -lh cat
# ~8 KB (compare: GNU coreutils cat is ~40 KB with glibc)
```

---

## 5. POSIX Compliance Extensions

### 5.1 What dietlibc Provides

**✅ Core POSIX APIs (95% coverage):**
- File I/O: `open`, `read`, `write`, `close`, `lseek`, `dup`, `dup2`
- Process: `fork`, `exec*`, `wait*`, `exit`, `kill`
- Memory: `malloc`, `free`, `realloc`, `calloc`, `mmap`, `munmap`
- Strings: All `string.h` functions
- Math: Full `libm` (linked into `libc.a`)
- Networking: `socket`, `bind`, `connect`, `send`, `recv`, etc.
- Threads: Basic `pthread` support (mutexes, condition variables)

### 5.2 What dietlibc Lacks

**❌ Advanced POSIX (5% coverage):**
- Full locale/i18n support
- Wide character (`wchar_t`) support
- Full regex (`regcomp`, `regexec` - only basic)
- Message queues (`mq_*`)
- POSIX semaphores (`sem_*` - incomplete)
- Advanced stdio buffering

### 5.3 XINIM Extensions

**Strategy:** Implement missing functions in `userland/libc-xinim/`

**Directory structure:**
```
userland/libc-xinim/
├── extensions/
│   ├── mqueue.c         # POSIX message queues
│   ├── semaphore.c      # POSIX semaphores
│   ├── locale.c         # Basic locale support
│   └── wchar.c          # Basic wide char support
├── xinim/
│   ├── capability.c     # XINIM-specific: Capabilities
│   ├── lattice.c        # XINIM-specific: Lattice IPC
│   └── resurrection.c   # XINIM-specific: Service manager
└── Makefile
```

**Example: Message Queue Implementation**

`extensions/mqueue.c`:
```c
#include <mqueue.h>
#include <xinim/syscalls.h>

// XINIM implements message queues in kernel via lattice IPC

mqd_t mq_open(const char* name, int oflag, ...) {
    // Translate to XINIM lattice_connect syscall
    return syscall(__NR_lattice_connect, (long)name, oflag, 0);
}

int mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned msg_prio) {
    return syscall(__NR_lattice_send, mqdes, (long)msg_ptr, msg_len, msg_prio);
}

ssize_t mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned* msg_prio) {
    return syscall(__NR_lattice_recv, mqdes, (long)msg_ptr, msg_len, (long)msg_prio);
}
```

**Build extensions:**
```bash
cd userland/libc-xinim
make

# Output: libxinim.a (XINIM extensions)

# Link with programs:
x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    -Os -static \
    myapp.c -o myapp \
    -L. -lxinim
```

---

## 6. Performance Tuning

### 6.1 Size Optimization

**Ultra-small binaries:**
```bash
CFLAGS="-Os -ffunction-sections -fdata-sections"
LDFLAGS="-Wl,--gc-sections -s"

x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    $CFLAGS $LDFLAGS \
    hello.c -o hello

# Result: ~6 KB (vs ~8 KB default)
```

**Explanation:**
- `-ffunction-sections -fdata-sections`: Put each function/data in separate section
- `-Wl,--gc-sections`: Linker removes unused sections
- `-s`: Strip all symbols

### 6.2 Speed Optimization

**For performance-critical code:**
```bash
CFLAGS="-O3 -march=x86-64-v3 -mtune=generic \
        -fomit-frame-pointer -fno-plt"

x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    $CFLAGS -static \
    server.c -o server

# Result: Faster, but larger binary
```

**Explanation:**
- `-O3`: Maximum optimization
- `-march=x86-64-v3`: Use AVX, AVX2 instructions (requires newer CPUs)
- `-fno-plt`: Avoid PLT overhead (for static linking)

### 6.3 Profiling

**Build with profiling:**
```bash
x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    -Os -static -pg \
    app.c -o app

# Run in QEMU
./scripts/qemu_x86_64.sh

# Extract gmon.out and analyze
gprof app gmon.out > profile.txt
```

---

## 7. Debugging with dietlibc

### 7.1 GDB Setup

**Build with debug symbols:**
```bash
x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    -g -static \
    app.c -o app

# Debug with QEMU + GDB
./scripts/qemu_x86_64.sh &
x86_64-xinim-elf-gdb app
(gdb) target remote localhost:1234
(gdb) break main
(gdb) continue
```

### 7.2 Common Debugging Scenarios

**1. Segfault on syscall:**
```gdb
(gdb) break syscall_entry
(gdb) continue
(gdb) info registers
# Check RAX (syscall number), RDI (fd), RSI (buffer), RDX (count)

# Example:
RAX = 0    (read)
RDI = 5    (fd)
RSI = 0x0  (NULL buffer - BUG!)
RDX = 100  (count)

# Fix: Check buffer validity before syscall
```

**2. Missing symbols:**
```bash
# Rebuild dietlibc with debug info
cd ~/xinim-sources/dietlibc-0.34
make clean
make CFLAGS="-g -Os" all
make install prefix=/opt/xinim-sysroot/usr

# Rebuild app
x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
    -g -static app.c -o app

# Now GDB can show dietlibc internals
(gdb) break printf
(gdb) step
# Shows dietlibc source
```

### 7.3 Valgrind (Future)

**Note:** Valgrind requires porting to XINIM. Track in issue #XXX.

---

## 8. Common Issues

### 8.1 "undefined reference to `__stack_chk_fail`"

**Problem:** GCC defaults to stack protector, but dietlibc doesn't provide it.

**Solution:** Disable stack protector:
```bash
CFLAGS="-fno-stack-protector"
```

### 8.2 "fatal error: features.h: No such file or directory"

**Problem:** dietlibc doesn't have `features.h` (glibc-specific).

**Solution:** Remove `#include <features.h>` from source, or add stub:
```c
// features.h (stub)
#ifndef _FEATURES_H
#define _FEATURES_H
#define __GLIBC__ 0
#endif
```

### 8.3 "locale not supported"

**Problem:** dietlibc has minimal locale support.

**Solution:**
```c
// Don't use setlocale(), or accept failure:
if (setlocale(LC_ALL, "") == NULL) {
    // dietlibc doesn't support locale - continue anyway
    fprintf(stderr, "Warning: locale not supported\n");
}
```

### 8.4 "mq_open not found"

**Problem:** dietlibc doesn't have POSIX message queues.

**Solution:** Use XINIM extensions (see Section 5.3):
```bash
gcc ... -lxinim
```

### 8.5 Binary too large (>1 MB)

**Problem:** Including too many libraries (e.g., libstdc++ with templates).

**Solution:** Profile binary size:
```bash
x86_64-xinim-elf-nm -S --size-sort app | tail -20
# Shows largest symbols

# Example output:
# 00000000 00050000 T std::__cxx11::basic_string<...>  (320 KB!)

# Fix: Avoid heavy templates, use C-style strings
```

---

## 9. Integration Checklist

Before deploying userland with dietlibc:

- [ ] Toolchain built and verified (`build_dietlibc.sh` completes)
- [ ] Test compilation of simple C program (hello.c)
- [ ] Test compilation of simple C++ program (hello.cpp)
- [ ] Verify syscall adapter (read/write work in QEMU)
- [ ] Implement missing POSIX functions in `libc-xinim`
- [ ] Build all 60+ coreutils with dietlibc
- [ ] Run POSIX test suite (`third_party/gpl/posixtestsuite-main`)
- [ ] Verify binary sizes (should be 60-80% smaller than musl)
- [ ] Performance benchmark (IPC throughput, syscall latency)
- [ ] Memory footprint test (RSS should be ~150 KB per process)
- [ ] Update documentation (BUILDING.md, USERLAND_IMPLEMENTATION_PLAN.md)

---

## 10. Next Steps

**Week 2 (Current):**
- Complete toolchain build (binutils, GCC Stage 1, dietlibc, GCC Stage 2)
- Build mksh shell with dietlibc
- Port 10 coreutils to dietlibc (cat, ls, cp, mv, rm, mkdir, rmdir, touch, echo, pwd)

**Week 3-4:**
- Implement `libc-xinim` extensions (message queues, semaphores, locale)
- Build all 60+ coreutils
- Run POSIX compliance tests

**Week 5-8:**
- Implement 240 missing syscalls in kernel
- Verify dietlibc programs work on real XINIM kernel
- Optimize syscall performance

**Week 9-12:**
- Port additional utilities (find, grep, sed, awk, tar, gzip)
- Build network utilities (ping, wget, ssh)
- Build development tools (make, gdb)

---

## References

- [dietlibc homepage](https://www.fefe.de/dietlibc/)
- [dietlibc FAQ](https://www.fefe.de/dietlibc/FAQ.txt)
- [XINIM Toolchain Specification](/home/user/XINIM/docs/specs/TOOLCHAIN_SPECIFICATION.md)
- [XINIM dietlibc vs musl Analysis](/home/user/XINIM/docs/specs/DIETLIBC_VS_MUSL.md)
- [XINIM Syscall Implementation Guide](/home/user/XINIM/docs/guides/SYSCALL_IMPLEMENTATION_GUIDE.md)
- [SUSv4 POSIX Compliance Audit](/home/user/XINIM/docs/SUSV4_POSIX_2017_COMPLIANCE_AUDIT.md)

---

**Document Status:** ✅ APPROVED
**Last Updated:** 2025-11-17
**Maintainer:** XINIM Toolchain Team
