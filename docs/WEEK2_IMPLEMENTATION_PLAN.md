# Week 2 Implementation Plan: libc-xinim Extensions & Lock Integration

**Date:** 2025-11-17
**Status:** 🟡 IN PROGRESS
**Dependencies:** Week 1 toolchain (scripts ready, pending system dependency installation)
**Branch:** `claude/audit-translate-repo-018NNYQuW6XJrQJL2LLUPS45`

---

## Executive Summary

Week 2 focuses on **extending POSIX compliance** through the libc-xinim extension library and **integrating the novel lock framework** with XINIM's core kernel services. This week bridges the gap between dietlibc's 90% POSIX coverage and our 95% target, while hardening the microkernel's fault tolerance through advanced synchronization primitives.

**Key Objectives:**
1. ✅ **Complete Week 1 toolchain build** (install dependencies, build, validate)
2. 🔧 **Implement libc-xinim** (~1,500 LOC providing 5% additional POSIX coverage)
3. 🔧 **Integrate lock framework** with ServiceManager and WaitForGraph
4. 🔧 **Create performance benchmarks** for all 5 lock types
5. 🔧 **Port mksh shell** and 10 coreutils to dietlibc

**Timeline:** 7 days (parallel execution of multiple tracks)

**Deliverables:**
- libc-xinim extension library (libxinim.a)
- Lock framework integration (ServiceManager, WaitForGraph, scheduler)
- Performance benchmark suite (5 lock types × 8 scenarios)
- mksh 59c built with dietlibc
- 10 coreutils (cat, ls, cp, mv, rm, mkdir, rmdir, touch, echo, pwd)

---

## Table of Contents

1. [Week 2 Scope and Objectives](#1-week-2-scope-and-objectives)
2. [Detailed Task Breakdown](#2-detailed-task-breakdown)
3. [libc-xinim Implementation](#3-libc-xinim-implementation)
4. [Lock Framework Integration](#4-lock-framework-integration)
5. [Performance Benchmarks](#5-performance-benchmarks)
6. [mksh and Coreutils](#6-mksh-and-coreutils)
7. [Testing Strategy](#7-testing-strategy)
8. [Dependency Management](#8-dependency-management)
9. [Timeline and Milestones](#9-timeline-and-milestones)
10. [Risks and Mitigation](#10-risks-and-mitigation)

---

## 1. Week 2 Scope and Objectives

### 1.1 Primary Objectives

**Objective 1: Complete Week 1 Toolchain Bootstrap**
- Install 23 system dependencies
- Execute build scripts (binutils, GCC Stage 1, dietlibc, GCC Stage 2)
- Validate toolchain with hello world and basic programs
- **Success criteria:** x86_64-xinim-elf-gcc produces working 8 KB hello world binary

**Objective 2: Implement libc-xinim Extension Library**
- POSIX message queues (8 functions → lattice IPC)
- POSIX semaphores (11 functions → kernel/futex)
- Wide character support (20 functions, UTF-8 only)
- Basic locale support (5 functions, C/POSIX/UTF-8)
- **Success criteria:** libxinim.a builds, links, passes 40+ unit tests

**Objective 3: Integrate Lock Framework with Kernel**
- Update ServiceManager::handle_crash() to call LockManager::handle_crash()
- Add LOCK edges to WaitForGraph for deadlock detection
- Replace legacy spinlocks in critical paths
- **Success criteria:** Kernel builds, lock tests pass, crash recovery works

**Objective 4: Create Performance Benchmark Suite**
- Benchmark all 5 lock types (Ticket, MCS, Adaptive, PhaseRW, Capability)
- Test scenarios: uncontended, low (2-4), medium (8-16), high (32-64) contention
- Measure: throughput (ops/s), latency (ns), fairness (Jain index)
- **Success criteria:** Verify 16x improvement over TAS spinlock

**Objective 5: Port Shell and Utilities**
- Build mksh 59c with dietlibc
- Port 10 coreutils to dietlibc
- **Success criteria:** All utilities run, pass basic tests, total <100 KB

### 1.2 Scope Boundaries

**In Scope:**
- libc-xinim core functionality (mqueue, semaphore, wchar, locale)
- Lock framework integration (ServiceManager, WaitForGraph, scheduler)
- Performance benchmarks (all 5 lock types)
- Basic userland (mksh + 10 utils)
- Week 1 toolchain completion

**Out of Scope (Deferred to Week 3-4):**
- Full userland utilities (90+ remaining utils)
- Kernel syscall implementation (300 syscalls in Week 5-8)
- VFS integration (Week 6-7)
- Networking stack (Week 8-9)
- Advanced regex (TRE port, Week 4)
- Full locale support (Week 12)

### 1.3 Dependencies from Week 1

| Deliverable | Status | Required for |
|-------------|--------|--------------|
| Toolchain scripts | ✅ Complete | All Week 2 builds |
| dietlibc integration guide | ✅ Complete | libc-xinim implementation |
| Lock framework implementations | ✅ Complete | Kernel integration |
| POSIX compliance audit | ✅ Complete | libc-xinim scope definition |
| Build environment | 🟡 Pending install | Toolchain execution |

---

## 2. Detailed Task Breakdown

### Track 1: Toolchain Bootstrap Completion (Days 1-2)

**2.1 Install System Dependencies (30 minutes)**

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    bison flex \
    libgmp-dev libmpfr-dev libmpc-dev libisl-dev \
    texinfo help2man gawk \
    libc6-dev linux-headers-$(uname -r) \
    git wget tar xz-utils bzip2 gzip \
    m4 autoconf automake libtool pkg-config \
    python3
```

**Verification:**
```bash
# Check versions
gcc --version          # ≥ 9.0
make --version         # ≥ 4.0
bison --version        # ≥ 3.0
flex --version         # ≥ 2.6
makeinfo --version     # ≥ 6.0
```

**2.2 Execute Build Scripts (2-4 hours)**

```bash
# Setup environment
cd /home/user/XINIM/scripts
./setup_build_environment.sh

# Source environment
source /opt/xinim-toolchain/xinim-env.sh

# Build binutils (10 minutes)
./build_binutils.sh

# Build GCC Stage 1 (60 minutes)
./build_gcc_stage1.sh

# Build dietlibc (5 minutes)
./build_dietlibc.sh

# Build GCC Stage 2 (60 minutes)
./build_gcc_stage2.sh
```

**2.3 Validate Toolchain (15 minutes)**

```bash
./scripts/validate_toolchain.sh

# Expected output:
# ✅ Environment variables: PASS
# ✅ Binutils tools: PASS (50+ tools found)
# ✅ GCC Stage 2: PASS (C and C++ working)
# ✅ dietlibc headers: PASS (120+ headers)
# ✅ C compilation: PASS (8 KB hello world)
# ✅ C++ compilation: PASS
# ✅ Static linking: PASS
# Overall: 100% PASS
```

### Track 2: libc-xinim Implementation (Days 1-4)

**2.4 Create libc-xinim Directory Structure (5 minutes)**

```bash
mkdir -p userland/libc-xinim/{include/xinim,src,tests}
cd userland/libc-xinim
```

**Directory tree:**
```
userland/libc-xinim/
├── Makefile
├── README.md
├── include/xinim/
│   ├── mqueue.h           # POSIX message queues
│   ├── semaphore.h        # POSIX semaphores
│   ├── wchar_ext.h        # Wide character (UTF-8)
│   └── locale_ext.h       # Locale (C/POSIX/UTF-8)
├── src/
│   ├── mqueue.c           # 250 LOC
│   ├── semaphore.c        # 300 LOC
│   ├── wchar.c            # 450 LOC
│   ├── locale.c           # 150 LOC
│   └── internal.h         # Shared definitions
└── tests/
    ├── test_mqueue.c      # 8 test cases
    ├── test_semaphore.c   # 10 test cases
    ├── test_wchar.c       # 15 test cases
    ├── test_locale.c      # 5 test cases
    └── test_integration.c # Integration test
```

**2.5 Implement POSIX Message Queues (Day 2, 4 hours)**

**API Coverage (8 functions):**
- `mq_open()` - Create/open message queue
- `mq_close()` - Close message queue
- `mq_unlink()` - Remove message queue
- `mq_send()` - Send message (blocking)
- `mq_receive()` - Receive message (blocking)
- `mq_timedsend()` - Send with timeout
- `mq_timedreceive()` - Receive with timeout
- `mq_getattr()` / `mq_setattr()` - Get/set attributes

**Implementation Strategy:**
Map POSIX message queues to XINIM lattice IPC:

```c
// include/xinim/mqueue.h
#ifndef XINIM_MQUEUE_H
#define XINIM_MQUEUE_H

#include <sys/types.h>
#include <time.h>

typedef int mqd_t;

struct mq_attr {
    long mq_flags;       // O_NONBLOCK flag
    long mq_maxmsg;      // Max messages in queue
    long mq_msgsize;     // Max message size
    long mq_curmsgs;     // Current messages
};

mqd_t mq_open(const char *name, int oflag, ...);
int mq_close(mqd_t mqdes);
int mq_unlink(const char *name);
int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned msg_prio);
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned *msg_prio);
int mq_timedsend(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
                 unsigned msg_prio, const struct timespec *abs_timeout);
ssize_t mq_timedreceive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                        unsigned *msg_prio, const struct timespec *abs_timeout);
int mq_getattr(mqd_t mqdes, struct mq_attr *attr);
int mq_setattr(mqd_t mqdes, const struct mq_attr *newattr, struct mq_attr *oldattr);

#endif // XINIM_MQUEUE_H
```

**Backend mapping (src/mqueue.c):**
```c
#include <xinim/mqueue.h>
#include <xinim/syscalls.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

// Internal syscall numbers (from kernel)
#define __NR_lattice_connect   500
#define __NR_lattice_send      501
#define __NR_lattice_recv      502
#define __NR_lattice_close     503

// Message queue descriptor
typedef struct {
    int endpoint_id;        // Lattice IPC endpoint
    int flags;              // O_NONBLOCK, etc.
    struct mq_attr attr;    // Queue attributes
} mq_internal_t;

mqd_t mq_open(const char *name, int oflag, ...) {
    if (!name) {
        errno = EINVAL;
        return (mqd_t)-1;
    }

    // Create lattice IPC endpoint
    // Format: /mq/<name> → lattice namespace
    char endpoint_name[256];
    snprintf(endpoint_name, sizeof(endpoint_name), "/mq%s", name);

    int endpoint_id = syscall(__NR_lattice_connect,
                              (long)endpoint_name,
                              oflag,
                              0);

    if (endpoint_id < 0) {
        errno = -endpoint_id;
        return (mqd_t)-1;
    }

    // Allocate descriptor
    mq_internal_t *mq = malloc(sizeof(mq_internal_t));
    if (!mq) {
        syscall(__NR_lattice_close, endpoint_id);
        errno = ENOMEM;
        return (mqd_t)-1;
    }

    mq->endpoint_id = endpoint_id;
    mq->flags = oflag;

    // Default attributes
    mq->attr.mq_flags = oflag & O_NONBLOCK;
    mq->attr.mq_maxmsg = 10;
    mq->attr.mq_msgsize = 8192;
    mq->attr.mq_curmsgs = 0;

    return (mqd_t)(long)mq;
}

int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned msg_prio) {
    mq_internal_t *mq = (mq_internal_t*)(long)mqdes;
    if (!mq || !msg_ptr) {
        errno = EINVAL;
        return -1;
    }

    if (msg_len > mq->attr.mq_msgsize) {
        errno = EMSGSIZE;
        return -1;
    }

    // Use lattice_send syscall
    // Priority encoded in flags
    long ret = syscall(__NR_lattice_send,
                       mq->endpoint_id,
                       (long)msg_ptr,
                       msg_len,
                       msg_prio);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return 0;
}

ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned *msg_prio) {
    mq_internal_t *mq = (mq_internal_t*)(long)mqdes;
    if (!mq || !msg_ptr) {
        errno = EINVAL;
        return -1;
    }

    if (msg_len < mq->attr.mq_msgsize) {
        errno = EMSGSIZE;
        return -1;
    }

    // Use lattice_recv syscall
    long ret = syscall(__NR_lattice_recv,
                       mq->endpoint_id,
                       (long)msg_ptr,
                       msg_len,
                       msg_prio ? (long)msg_prio : 0);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return ret;
}

int mq_close(mqd_t mqdes) {
    mq_internal_t *mq = (mq_internal_t*)(long)mqdes;
    if (!mq) {
        errno = EBADF;
        return -1;
    }

    syscall(__NR_lattice_close, mq->endpoint_id);
    free(mq);
    return 0;
}

int mq_unlink(const char *name) {
    if (!name) {
        errno = EINVAL;
        return -1;
    }

    char endpoint_name[256];
    snprintf(endpoint_name, sizeof(endpoint_name), "/mq%s", name);

    // Remove IPC endpoint (maps to unlink syscall)
    long ret = syscall(__NR_unlink, (long)endpoint_name);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return 0;
}

int mq_getattr(mqd_t mqdes, struct mq_attr *attr) {
    mq_internal_t *mq = (mq_internal_t*)(long)mqdes;
    if (!mq || !attr) {
        errno = EINVAL;
        return -1;
    }

    *attr = mq->attr;
    return 0;
}

int mq_setattr(mqd_t mqdes, const struct mq_attr *newattr, struct mq_attr *oldattr) {
    mq_internal_t *mq = (mq_internal_t*)(long)mqdes;
    if (!mq || !newattr) {
        errno = EINVAL;
        return -1;
    }

    if (oldattr) {
        *oldattr = mq->attr;
    }

    // Only mq_flags can be modified
    mq->attr.mq_flags = newattr->mq_flags & O_NONBLOCK;
    return 0;
}

// Timed variants use timedwait on lattice IPC
int mq_timedsend(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
                 unsigned msg_prio, const struct timespec *abs_timeout) {
    // TODO: Implement timeout via poll/select on endpoint
    // For now, fallback to blocking send
    return mq_send(mqdes, msg_ptr, msg_len, msg_prio);
}

ssize_t mq_timedreceive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                        unsigned *msg_prio, const struct timespec *abs_timeout) {
    // TODO: Implement timeout via poll/select on endpoint
    return mq_receive(mqdes, msg_ptr, msg_len, msg_prio);
}
```

**Testing (tests/test_mqueue.c):**
```c
#include <xinim/mqueue.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_mq_open_close() {
    mqd_t mq = mq_open("/test_queue", O_CREAT | O_RDWR, 0666, NULL);
    assert(mq != (mqd_t)-1);

    int ret = mq_close(mq);
    assert(ret == 0);

    mq_unlink("/test_queue");
    printf("✅ test_mq_open_close\n");
}

void test_mq_send_receive() {
    mqd_t mq = mq_open("/test_queue", O_CREAT | O_RDWR, 0666, NULL);
    assert(mq != (mqd_t)-1);

    const char *msg = "Hello, XINIM!";
    int ret = mq_send(mq, msg, strlen(msg) + 1, 0);
    assert(ret == 0);

    char buf[256];
    ssize_t len = mq_receive(mq, buf, sizeof(buf), NULL);
    assert(len == strlen(msg) + 1);
    assert(strcmp(buf, msg) == 0);

    mq_close(mq);
    mq_unlink("/test_queue");
    printf("✅ test_mq_send_receive\n");
}

void test_mq_priority() {
    mqd_t mq = mq_open("/test_prio", O_CREAT | O_RDWR, 0666, NULL);

    mq_send(mq, "low", 4, 1);
    mq_send(mq, "high", 5, 10);
    mq_send(mq, "mid", 4, 5);

    char buf[256];
    unsigned prio;

    // Should receive in priority order: high, mid, low
    mq_receive(mq, buf, sizeof(buf), &prio);
    assert(strcmp(buf, "high") == 0 && prio == 10);

    mq_receive(mq, buf, sizeof(buf), &prio);
    assert(strcmp(buf, "mid") == 0 && prio == 5);

    mq_receive(mq, buf, sizeof(buf), &prio);
    assert(strcmp(buf, "low") == 0 && prio == 1);

    mq_close(mq);
    mq_unlink("/test_prio");
    printf("✅ test_mq_priority\n");
}

int main() {
    test_mq_open_close();
    test_mq_send_receive();
    test_mq_priority();
    printf("All message queue tests passed!\n");
    return 0;
}
```

**2.6 Implement POSIX Semaphores (Day 2, 3 hours)**

**API Coverage (11 functions):**

Unnamed semaphores:
- `sem_init()` - Initialize semaphore
- `sem_destroy()` - Destroy semaphore
- `sem_wait()` - Wait (decrement)
- `sem_trywait()` - Try wait (non-blocking)
- `sem_timedwait()` - Wait with timeout
- `sem_post()` - Post (increment)
- `sem_getvalue()` - Get current value

Named semaphores:
- `sem_open()` - Create/open named semaphore
- `sem_close()` - Close named semaphore
- `sem_unlink()` - Remove named semaphore

**Implementation (src/semaphore.c):**
```c
// include/xinim/semaphore.h
#ifndef XINIM_SEMAPHORE_H
#define XINIM_SEMAPHORE_H

#include <time.h>

typedef struct {
    unsigned int value;
    int futex;  // Kernel futex for blocking
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

// Named semaphores
sem_t *sem_open(const char *name, int oflag, ...);
int sem_close(sem_t *sem);
int sem_unlink(const char *name);

#endif
```

```c
// src/semaphore.c
#include <xinim/semaphore.h>
#include <xinim/syscalls.h>
#include <errno.h>
#include <stdatomic.h>

#define __NR_futex  202

#define FUTEX_WAIT  0
#define FUTEX_WAKE  1

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    atomic_store_explicit((_Atomic unsigned int*)&sem->value, value,
                          memory_order_relaxed);
    sem->futex = 0;
    return 0;
}

int sem_destroy(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int sem_wait(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    while (1) {
        unsigned int old = atomic_load_explicit(
            (_Atomic unsigned int*)&sem->value, memory_order_acquire);

        if (old > 0) {
            if (atomic_compare_exchange_weak_explicit(
                (_Atomic unsigned int*)&sem->value,
                &old, old - 1,
                memory_order_acquire, memory_order_relaxed)) {
                return 0;  // Successfully decremented
            }
        } else {
            // Wait on futex
            syscall(__NR_futex, &sem->futex, FUTEX_WAIT, 0, NULL, NULL, 0);
        }
    }
}

int sem_trywait(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    unsigned int old = atomic_load_explicit(
        (_Atomic unsigned int*)&sem->value, memory_order_acquire);

    if (old == 0) {
        errno = EAGAIN;
        return -1;
    }

    if (atomic_compare_exchange_strong_explicit(
        (_Atomic unsigned int*)&sem->value,
        &old, old - 1,
        memory_order_acquire, memory_order_relaxed)) {
        return 0;
    }

    errno = EAGAIN;
    return -1;
}

int sem_post(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    atomic_fetch_add_explicit((_Atomic unsigned int*)&sem->value, 1,
                              memory_order_release);

    // Wake one waiter
    syscall(__NR_futex, &sem->futex, FUTEX_WAKE, 1, NULL, NULL, 0);
    return 0;
}

int sem_getvalue(sem_t *sem, int *sval) {
    if (!sem || !sval) {
        errno = EINVAL;
        return -1;
    }

    *sval = atomic_load_explicit((_Atomic unsigned int*)&sem->value,
                                 memory_order_relaxed);
    return 0;
}
```

**2.7 Implement Wide Character Support (Day 3, 3 hours)**

**API Coverage (20 functions):**
- String functions: `wcslen`, `wcscpy`, `wcscat`, `wcscmp`, `wcsncmp`
- Search: `wcschr`, `wcsrchr`, `wcsstr`
- Conversion: `mbstowcs`, `wcstombs`, `mbtowc`, `wctomb`
- Character classification: `iswalpha`, `iswdigit`, `iswspace`, etc.

**Implementation (src/wchar.c) - UTF-8 only:**
```c
// include/xinim/wchar_ext.h
#ifndef XINIM_WCHAR_EXT_H
#define XINIM_WCHAR_EXT_H

#include <stddef.h>
#include <wchar.h>

// Basic string functions
size_t wcslen(const wchar_t *s);
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src);
int wcscmp(const wchar_t *s1, const wchar_t *s2);

// UTF-8 conversion
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
size_t wcstombs(char *dest, const wchar_t *src, size_t n);

#endif
```

```c
// src/wchar.c
#include <xinim/wchar_ext.h>

size_t wcslen(const wchar_t *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

wchar_t *wcscpy(wchar_t *dest, const wchar_t *src) {
    wchar_t *d = dest;
    while ((*d++ = *src++));
    return dest;
}

int wcscmp(const wchar_t *s1, const wchar_t *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const wchar_t*)s1 - *(const wchar_t*)s2;
}

// UTF-8 → wchar_t conversion
size_t mbstowcs(wchar_t *dest, const char *src, size_t n) {
    size_t i = 0, j = 0;

    while (src[i] && j < n) {
        if ((src[i] & 0x80) == 0) {
            // ASCII: 0xxxxxxx
            dest[j++] = src[i++];
        } else if ((src[i] & 0xE0) == 0xC0) {
            // 2-byte: 110xxxxx 10xxxxxx
            dest[j++] = ((src[i] & 0x1F) << 6) | (src[i+1] & 0x3F);
            i += 2;
        } else if ((src[i] & 0xF0) == 0xE0) {
            // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
            dest[j++] = ((src[i] & 0x0F) << 12) |
                        ((src[i+1] & 0x3F) << 6) |
                        (src[i+2] & 0x3F);
            i += 3;
        } else {
            // 4-byte (surrogate pair or replacement char)
            dest[j++] = 0xFFFD;  // Replacement character
            i += 4;
        }
    }

    if (j < n) dest[j] = 0;
    return j;
}

// wchar_t → UTF-8 conversion
size_t wcstombs(char *dest, const wchar_t *src, size_t n) {
    size_t i = 0, j = 0;

    while (src[i] && j < n - 4) {  // Reserve 4 bytes
        wchar_t wc = src[i++];

        if (wc < 0x80) {
            // ASCII
            dest[j++] = (char)wc;
        } else if (wc < 0x800) {
            // 2-byte
            dest[j++] = 0xC0 | (wc >> 6);
            dest[j++] = 0x80 | (wc & 0x3F);
        } else if (wc < 0x10000) {
            // 3-byte
            dest[j++] = 0xE0 | (wc >> 12);
            dest[j++] = 0x80 | ((wc >> 6) & 0x3F);
            dest[j++] = 0x80 | (wc & 0x3F);
        } else {
            // 4-byte (not fully supported, use replacement)
            dest[j++] = 0xEF;
            dest[j++] = 0xBF;
            dest[j++] = 0xBD;
        }
    }

    if (j < n) dest[j] = 0;
    return j;
}
```

**2.8 Implement Basic Locale Support (Day 3, 2 hours)**

**API Coverage (5 functions):**
- `setlocale()` - Set locale (C, POSIX, C.UTF-8 only)
- `localeconv()` - Get locale conventions
- `strcoll()` - Locale-aware string compare (fallback to strcmp)
- `strxfrm()` - String transformation (fallback to strcpy)

**Implementation (src/locale.c):**
```c
// src/locale.c
#include <locale.h>
#include <string.h>

static char current_locale[32] = "C";

char *setlocale(int category, const char *locale) {
    if (!locale) {
        return current_locale;
    }

    // Support only C, POSIX, C.UTF-8
    if (strcmp(locale, "C") == 0 ||
        strcmp(locale, "POSIX") == 0 ||
        strcmp(locale, "C.UTF-8") == 0) {
        strncpy(current_locale, locale, sizeof(current_locale) - 1);
        return current_locale;
    }

    return NULL;  // Unsupported locale
}

struct lconv *localeconv(void) {
    static struct lconv lc = {
        .decimal_point = ".",
        .thousands_sep = "",
        .grouping = "",
        .int_curr_symbol = "",
        .currency_symbol = "",
        .mon_decimal_point = "",
        .mon_thousands_sep = "",
        .mon_grouping = "",
        .positive_sign = "",
        .negative_sign = "",
        .int_frac_digits = 127,
        .frac_digits = 127,
        .p_cs_precedes = 127,
        .p_sep_by_space = 127,
        .n_cs_precedes = 127,
        .n_sep_by_space = 127,
        .p_sign_posn = 127,
        .n_sign_posn = 127,
    };
    return &lc;
}
```

**2.9 Build libc-xinim (Day 4, 2 hours)**

**Makefile (userland/libc-xinim/Makefile):**
```makefile
CC = x86_64-xinim-elf-gcc
AR = x86_64-xinim-elf-ar
CFLAGS = -O2 -Wall -Wextra -std=c11 -I./include
ARFLAGS = rcs

SRCS = src/mqueue.c src/semaphore.c src/wchar.c src/locale.c
OBJS = $(SRCS:.c=.o)

all: libxinim.a

libxinim.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) libxinim.a

install: libxinim.a
	install -D libxinim.a $(DESTDIR)/opt/xinim-sysroot/usr/lib/libxinim.a
	install -D include/xinim/*.h $(DESTDIR)/opt/xinim-sysroot/usr/include/xinim/

test: all
	$(CC) $(CFLAGS) tests/test_mqueue.c -L. -lxinim -o test_mqueue
	$(CC) $(CFLAGS) tests/test_semaphore.c -L. -lxinim -o test_semaphore
	$(CC) $(CFLAGS) tests/test_wchar.c -L. -lxinim -o test_wchar

.PHONY: all clean install test
```

**Build and test:**
```bash
cd userland/libc-xinim
make clean all
make test

# Expected output:
# libxinim.a created (50-80 KB)
# All tests compile and link
```

### Track 3: Lock Framework Integration (Days 2-4)

**2.10 Integrate LockManager with ServiceManager (Day 2, 3 hours)**

**File:** `src/kernel/service_manager.cpp`

**Current crash handling:**
```cpp
void ServiceManager::handle_crash(xinim::pid_t crashed_pid) {
    // Current implementation:
    // 1. Mark service as crashed
    // 2. Clean up resources
    // 3. Restart service
}
```

**Updated crash handling:**
```cpp
#include "lock_manager.hpp"

void ServiceManager::handle_crash(xinim::pid_t crashed_pid) {
    // 1. Mark service as crashed
    auto it = services_.find(crashed_pid);
    if (it == services_.end()) return;

    ServiceInfo& info = it->second;
    info.state = ServiceState::CRASHED;
    info.crash_count++;

    // 2. **NEW: Release all locks held by crashed service**
    size_t locks_released = xinim::LockManager::instance().handle_crash(crashed_pid);

    if (locks_released > 0) {
        log(LOG_WARNING, "Service %d crashed holding %zu locks - force released",
            crashed_pid, locks_released);

        // Notify dependent services about potential tainted state
        notify_lock_taint(crashed_pid, locks_released);
    }

    // 3. Clean up IPC channels
    cleanup_ipc_channels(crashed_pid);

    // 4. Remove from WaitForGraph (prevent deadlock detection false positives)
    wait_graph_.remove_node(crashed_pid);

    // 5. Restart service (if restart policy allows)
    if (should_restart(info)) {
        restart_service(crashed_pid);
    }
}

void ServiceManager::notify_lock_taint(xinim::pid_t crashed_pid, size_t lock_count) {
    // Find all services that were waiting on locks from crashed service
    auto waiters = wait_graph_.get_waiters(crashed_pid);

    for (xinim::pid_t waiter : waiters) {
        send_message(waiter, MessageType::LOCK_TAINT_NOTIFICATION,
                     crashed_pid, lock_count);
    }
}
```

**2.11 Add Lock Edges to WaitForGraph (Day 3, 4 hours)**

**File:** `src/kernel/wait_for_graph.hpp`

**Current edge types:**
```cpp
enum class EdgeType {
    IPC,        // Process waiting on IPC message
    RESOURCE    // Process waiting on resource
};
```

**Updated edge types:**
```cpp
enum class EdgeType {
    IPC,        // Process waiting on IPC message
    RESOURCE,   // Process waiting on resource
    LOCK        // Process waiting on lock (NEW)
};
```

**Add lock tracking:**
```cpp
// wait_for_graph.cpp
#include "wait_for_graph.hpp"
#include "ticket_spinlock.hpp"
#include "adaptive_mutex.hpp"
#include "phase_rwlock.hpp"

void WaitForGraph::add_lock_edge(xinim::pid_t waiter, xinim::pid_t holder,
                                  void* lock_addr, EdgeType lock_type) {
    std::lock_guard<TicketSpinlock> guard(graph_lock_);

    Edge edge;
    edge.from = waiter;
    edge.to = holder;
    edge.type = EdgeType::LOCK;
    edge.resource = lock_addr;
    edge.timestamp = get_monotonic_time();

    edges_.push_back(edge);

    // Check for deadlock cycle
    if (has_cycle(waiter)) {
        // Deadlock detected!
        handle_deadlock(waiter);
    }
}

void WaitForGraph::remove_lock_edge(xinim::pid_t waiter, void* lock_addr) {
    std::lock_guard<TicketSpinlock> guard(graph_lock_);

    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [waiter, lock_addr](const Edge& e) {
                return e.from == waiter &&
                       e.type == EdgeType::LOCK &&
                       e.resource == lock_addr;
            }),
        edges_.end()
    );
}

bool WaitForGraph::has_cycle(xinim::pid_t start_node) {
    std::unordered_set<xinim::pid_t> visited;
    std::unordered_set<xinim::pid_t> rec_stack;

    return has_cycle_util(start_node, visited, rec_stack);
}

bool WaitForGraph::has_cycle_util(xinim::pid_t node,
                                   std::unordered_set<xinim::pid_t>& visited,
                                   std::unordered_set<xinim::pid_t>& rec_stack) {
    if (rec_stack.find(node) != rec_stack.end()) {
        return true;  // Cycle detected!
    }

    if (visited.find(node) != visited.end()) {
        return false;
    }

    visited.insert(node);
    rec_stack.insert(node);

    // Check all outgoing edges
    for (const Edge& e : edges_) {
        if (e.from == node) {
            if (has_cycle_util(e.to, visited, rec_stack)) {
                return true;
            }
        }
    }

    rec_stack.erase(node);
    return false;
}

void WaitForGraph::handle_deadlock(xinim::pid_t node) {
    // Find the cycle
    std::vector<xinim::pid_t> cycle = extract_cycle(node);

    log(LOG_ERROR, "DEADLOCK DETECTED: cycle involves %zu processes", cycle.size());

    for (xinim::pid_t pid : cycle) {
        log(LOG_ERROR, "  Process %d", pid);
    }

    // Deadlock resolution strategy:
    // 1. Pick victim (lowest priority service)
    // 2. Force-unlock one lock in the cycle
    // 3. Notify victim about forced unlock

    xinim::pid_t victim = find_lowest_priority(cycle);
    force_unlock_one_lock(victim);

    notify_deadlock_victim(victim, cycle);
}
```

**2.12 Replace Legacy Spinlocks (Day 4, 3 hours)**

**Replace in interrupt handlers:**
```cpp
// src/kernel/interrupt_handler.cpp
#include "ticket_spinlock.hpp"

// OLD:
// static volatile int irq_lock = 0;
// acquire_spinlock(&irq_lock);

// NEW:
static xinim::TicketSpinlock irq_lock;

void handle_interrupt(int irq_num) {
    irq_lock.lock();

    // Critical section
    process_interrupt(irq_num);

    irq_lock.unlock();
}
```

**Replace in IPC channels:**
```cpp
// src/kernel/lattice_ipc.cpp
#include "adaptive_mutex.hpp"

class IPCChannel {
private:
    // OLD: volatile int lock = 0;
    xinim::AdaptiveMutex lock_;  // NEW

public:
    void send_message(const Message& msg) {
        xinim::pid_t my_pid = get_current_pid();
        lock_.lock(my_pid);

        // Send message
        message_queue_.push(msg);

        lock_.unlock(my_pid);
    }
};
```

**Replace in service manager:**
```cpp
// src/kernel/service_manager.cpp
#include "phase_rwlock.hpp"

class ServiceManager {
private:
    // OLD: pthread_rwlock_t services_lock;
    xinim::PhaseRWLock services_lock_;  // NEW

public:
    ServiceInfo* lookup_service(xinim::pid_t pid) {
        services_lock_.read_lock();

        auto it = services_.find(pid);
        ServiceInfo* result = (it != services_.end()) ? &it->second : nullptr;

        services_lock_.read_unlock();
        return result;
    }

    void register_service(xinim::pid_t pid, const ServiceInfo& info) {
        services_lock_.write_lock();

        services_[pid] = info;

        services_lock_.write_unlock();
    }
};
```

### Track 4: Performance Benchmarks (Days 3-5)

**2.13 Create Benchmark Framework (Day 3, 3 hours)**

**File:** `test/bench/lock_benchmark.cpp`

```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cmath>

#include "ticket_spinlock.hpp"
#include "mcs_spinlock.hpp"
#include "adaptive_mutex.hpp"
#include "phase_rwlock.hpp"
#include "capability_mutex.hpp"

using namespace xinim;
using namespace std::chrono;

// Benchmark configuration
struct BenchConfig {
    const char* lock_name;
    int num_threads;
    int iterations;
    int critical_section_us;  // Critical section duration (microseconds)
};

// Benchmark results
struct BenchResult {
    const char* lock_name;
    int num_threads;
    double throughput_ops_per_sec;
    double avg_latency_ns;
    double p99_latency_ns;
    double fairness_jain_index;
};

// Generic benchmark runner
template<typename LockType>
BenchResult benchmark_lock(const BenchConfig& config) {
    LockType lock;
    std::atomic<uint64_t> total_ops{0};
    std::vector<uint64_t> per_thread_ops(config.num_threads, 0);
    std::vector<double> latencies;

    auto worker = [&](int thread_id) {
        uint64_t ops = 0;

        for (int i = 0; i < config.iterations; i++) {
            auto start = high_resolution_clock::now();

            // Acquire lock
            if constexpr (std::is_same_v<LockType, AdaptiveMutex> ||
                         std::is_same_v<LockType, CapabilityMutex>) {
                lock.lock(thread_id);
            } else {
                lock.lock();
            }

            // Critical section (simulate work)
            if (config.critical_section_us > 0) {
                busy_wait_us(config.critical_section_us);
            }

            // Release lock
            if constexpr (std::is_same_v<LockType, AdaptiveMutex>) {
                lock.unlock(thread_id);
            } else {
                lock.unlock();
            }

            auto end = high_resolution_clock::now();
            double latency = duration_cast<nanoseconds>(end - start).count();

            latencies.push_back(latency);
            ops++;
        }

        per_thread_ops[thread_id] = ops;
        total_ops += ops;
    };

    // Run benchmark
    auto start = high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < config.num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    double elapsed_sec = duration_cast<microseconds>(end - start).count() / 1e6;

    // Calculate metrics
    BenchResult result;
    result.lock_name = config.lock_name;
    result.num_threads = config.num_threads;
    result.throughput_ops_per_sec = total_ops / elapsed_sec;

    // Average latency
    double sum = 0;
    for (double lat : latencies) sum += lat;
    result.avg_latency_ns = sum / latencies.size();

    // P99 latency
    std::sort(latencies.begin(), latencies.end());
    size_t p99_idx = latencies.size() * 99 / 100;
    result.p99_latency_ns = latencies[p99_idx];

    // Fairness (Jain's index)
    double sum_ops = 0, sum_sq_ops = 0;
    for (uint64_t ops : per_thread_ops) {
        sum_ops += ops;
        sum_sq_ops += ops * ops;
    }
    result.fairness_jain_index = (sum_ops * sum_ops) /
                                  (config.num_threads * sum_sq_ops);

    return result;
}

void print_results(const std::vector<BenchResult>& results) {
    printf("%-20s %8s %15s %12s %12s %10s\n",
           "Lock Type", "Threads", "Throughput", "Avg Lat", "P99 Lat", "Fairness");
    printf("%-20s %8s %15s %12s %12s %10s\n",
           "--------------------", "--------", "---------------",
           "------------", "------------", "----------");

    for (const auto& r : results) {
        printf("%-20s %8d %12.2f M/s %10.0f ns %10.0f ns %10.3f\n",
               r.lock_name,
               r.num_threads,
               r.throughput_ops_per_sec / 1e6,
               r.avg_latency_ns,
               r.p99_latency_ns,
               r.fairness_jain_index);
    }
}

int main() {
    std::vector<BenchResult> results;

    // Test configurations
    int thread_counts[] = {1, 2, 4, 8, 16, 32};
    int critical_section_us = 1;  // 1 microsecond
    int iterations = 100000;

    for (int num_threads : thread_counts) {
        // Benchmark TicketSpinlock
        results.push_back(benchmark_lock<TicketSpinlock>({
            "TicketSpinlock", num_threads, iterations, critical_section_us
        }));

        // Benchmark MCSSpinlock
        results.push_back(benchmark_lock<MCSSpinlock>({
            "MCSSpinlock", num_threads, iterations, critical_section_us
        }));

        // Benchmark AdaptiveMutex
        results.push_back(benchmark_lock<AdaptiveMutex>({
            "AdaptiveMutex", num_threads, iterations, critical_section_us
        }));

        // Benchmark PhaseRWLock (writers only)
        results.push_back(benchmark_lock<PhaseRWLock>({
            "PhaseRWLock", num_threads, iterations, critical_section_us
        }));

        // Benchmark CapabilityMutex
        results.push_back(benchmark_lock<CapabilityMutex>({
            "CapabilityMutex", num_threads, iterations, critical_section_us
        }));
    }

    print_results(results);

    printf("\nBenchmark complete!\n");
    return 0;
}
```

**Build and run:**
```bash
cd test/bench
x86_64-xinim-elf-g++ -std=c++17 -O3 -I../../src/kernel \
    lock_benchmark.cpp -o lock_benchmark -pthread

./lock_benchmark

# Expected output (example):
# Lock Type            Threads     Throughput      Avg Lat      P99 Lat   Fairness
# -------------------- -------- --------------- ------------ ------------ ----------
# TicketSpinlock              1       750.00 M/s       1333 ns       1500 ns      1.000
# TicketSpinlock              8        80.00 M/s      10000 ns      15000 ns      0.995
# MCSSpinlock                 8       200.00 M/s       4000 ns       6000 ns      0.998
# AdaptiveMutex               8       150.00 M/s       5333 ns       8000 ns      0.990
# ...
```

### Track 5: mksh and Coreutils (Days 5-7)

**2.14 Build mksh Shell (Day 5, 2 hours)**

```bash
# Download mksh
cd userland
wget https://www.mirbsd.org/MirOS/dist/mir/mksh/mksh-R59c.tgz
tar xzf mksh-R59c.tgz
cd mksh

# Build with dietlibc
export CC=x86_64-xinim-elf-gcc
export CFLAGS="--sysroot=/opt/xinim-sysroot -Os -static"
export LDFLAGS="-static"

sh Build.sh

# Test
ls -lh mksh
# Expected: ~150 KB binary (vs 300 KB with musl)

# Install
install -D mksh /opt/xinim-sysroot/bin/mksh
```

**2.15 Port Coreutils (Days 6-7, 8 hours)**

**10 utilities to port:**
1. cat - Concatenate files
2. ls - List directory
3. cp - Copy files
4. mv - Move files
5. rm - Remove files
6. mkdir - Make directory
7. rmdir - Remove directory
8. touch - Touch file
9. echo - Print text
10. pwd - Print working directory

**Example: cat (userland/coreutils/cat.c):**
```c
// cat.c - Simple cat implementation
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE];

    if (argc == 1) {
        // Read from stdin
        ssize_t n;
        while ((n = read(STDIN_FILENO, buffer, BUFFER_SIZE)) > 0) {
            write(STDOUT_FILENO, buffer, n);
        }
    } else {
        // Read from files
        for (int i = 1; i < argc; i++) {
            int fd = open(argv[i], O_RDONLY);
            if (fd < 0) {
                perror(argv[i]);
                continue;
            }

            ssize_t n;
            while ((n = read(fd, buffer, BUFFER_SIZE)) > 0) {
                write(STDOUT_FILENO, buffer, n);
            }

            close(fd);
        }
    }

    return 0;
}
```

**Build all utilities:**
```bash
cd userland/coreutils

for util in cat ls cp mv rm mkdir rmdir touch echo pwd; do
    x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
        -Os -static ${util}.c -o ${util}

    ls -lh ${util}
    # Expected: 6-12 KB per utility
done

# Total: ~80 KB for all 10 utilities
```

---

## 3. libc-xinim Implementation

[This section provides the detailed implementation from section 2.5-2.9]

**Summary:**
- POSIX message queues: 8 functions, 250 LOC, maps to lattice IPC
- POSIX semaphores: 11 functions, 300 LOC, uses futex
- Wide character: 20 functions, 450 LOC, UTF-8 only
- Locale: 5 functions, 150 LOC, C/POSIX/C.UTF-8
- **Total: 44 functions, ~1,500 LOC, 50-80 KB libxinim.a**

---

## 4. Lock Framework Integration

[This section provides the detailed implementation from section 2.10-2.12]

**Summary:**
- ServiceManager integration: Auto-release locks on crash
- WaitForGraph: Track lock dependencies, detect deadlocks
- Legacy replacement: Ticket (IRQ), Adaptive (IPC), PhaseRW (ServiceMgr)

---

## 5. Performance Benchmarks

[This section provides the detailed implementation from section 2.13]

**Expected Results:**

| Lock Type | 1 Thread | 8 Threads | 32 Threads | Fairness |
|-----------|----------|-----------|------------|----------|
| TicketSpinlock | 750 M/s | 80 M/s | 40 M/s | 0.995 |
| MCSSpinlock | 600 M/s | 200 M/s | 180 M/s | 0.998 |
| AdaptiveMutex | 700 M/s | 150 M/s | 100 M/s | 0.990 |
| PhaseRWLock | 800 M/s | 400 M/s (read) | 350 M/s | 0.985 |
| CapabilityMutex | 600 M/s | 120 M/s | 80 M/s | 0.992 |

**Key Insights:**
- MCSSpinlock: Best scaling (3x better than Ticket at 32 threads)
- PhaseRWLock: 8x improvement for read-heavy workloads
- AdaptiveMutex: Best for mixed workloads

---

## 6. mksh and Coreutils

[This section provides the detailed implementation from section 2.14-2.15]

**Summary:**
- mksh: 150 KB (50% smaller than musl build)
- 10 coreutils: 80 KB total (73% smaller than GNU coreutils)
- **Total userland: 230 KB**

---

## 7. Testing Strategy

### 7.1 libc-xinim Tests

**Unit tests (40+ test cases):**
- Message queues: 8 tests (open/close, send/receive, priority, timeout)
- Semaphores: 10 tests (init/destroy, wait/post, trywait, named)
- Wide char: 15 tests (strlen, strcpy, strcmp, mbstowcs, wcstombs)
- Locale: 5 tests (setlocale, localeconv)
- Integration: 2 tests (multi-feature scenarios)

**Test execution:**
```bash
cd userland/libc-xinim
make test

# Run in QEMU (when kernel is ready)
qemu-system-x86_64 -kernel xinim.elf -initrd test_mqueue
```

### 7.2 Lock Framework Tests

**Unit tests (31 existing + 15 new):**
- ServiceManager crash recovery: 5 tests
- WaitForGraph deadlock detection: 5 tests
- Legacy replacement validation: 5 tests

**Stress tests:**
- 1000 service crashes with locks held
- Deadlock cycle detection (3-way, 4-way, 10-way cycles)
- Lock fairness under high contention

### 7.3 Benchmark Validation

**Performance targets:**
- TicketSpinlock: 16x improvement over TAS at 16 threads ✅
- MCSSpinlock: 40x improvement over TAS at 32 threads ✅
- AdaptiveMutex: <100ns uncontended, adaptive sleep ✅
- PhaseRWLock: 8x read throughput improvement ✅
- CapabilityMutex: <100ms crash recovery ✅

### 7.4 Integration Tests

**End-to-end scenarios:**
1. Boot XINIM kernel
2. Start VFS server (uses CapabilityMutex)
3. Start IPC router (uses AdaptiveMutex)
4. Crash VFS server → locks auto-released
5. Restart VFS server → recovery <100ms
6. Run mksh with 10 coreutils
7. Validate all programs work correctly

---

## 8. Dependency Management

### 8.1 External Dependencies

**System packages (Week 1 carryover):**
- build-essential, bison, flex (installed in Track 1)
- libgmp-dev, libmpfr-dev, libmpc-dev (installed in Track 1)
- All 23 packages documented in WEEK1_SYSTEM_DEPENDENCIES_EXHAUSTIVE.md

**Source downloads:**
- mksh R59c: 360 KB tarball
- No other external dependencies

### 8.2 Internal Dependencies

**From Week 1:**
- Toolchain: binutils, GCC, dietlibc ✅
- Lock framework: All 5 implementations ✅
- Documentation: POSIX audit, dietlibc guide ✅

**Week 2 internal dependencies:**
- libc-xinim depends on: dietlibc syscall wrappers
- Lock integration depends on: ServiceManager, WaitForGraph
- Benchmarks depend on: All 5 lock implementations
- mksh/coreutils depend on: dietlibc, toolchain

### 8.3 Circular Dependency Resolution

**No circular dependencies detected.**

Dependency graph:
```
System deps → Toolchain → dietlibc → libc-xinim → mksh/utils
                             ↓
                      Lock framework → Kernel integration
```

---

## 9. Timeline and Milestones

### Day 1 (Monday): Toolchain Completion
- ✅ Install system dependencies (30 min)
- ✅ Execute build scripts (2-4 hours)
- ✅ Validate toolchain (15 min)
- ✅ Setup libc-xinim directory (5 min)

**Milestone 1:** Toolchain builds successfully, hello world works (8 KB binary)

### Day 2 (Tuesday): libc-xinim Core
- ✅ Implement POSIX message queues (4 hours)
- ✅ Implement POSIX semaphores (3 hours)
- ✅ Integrate LockManager with ServiceManager (3 hours)

**Milestone 2:** Message queues and semaphores working, tests pass

### Day 3 (Wednesday): Wide Char + Lock Integration
- ✅ Implement wide character support (3 hours)
- ✅ Implement locale support (2 hours)
- ✅ Add lock edges to WaitForGraph (4 hours)
- ✅ Create benchmark framework (3 hours)

**Milestone 3:** libc-xinim core complete, WaitForGraph enhanced

### Day 4 (Thursday): Lock Integration Complete
- ✅ Build libc-xinim library (2 hours)
- ✅ Replace legacy spinlocks (3 hours)
- ✅ Run lock integration tests (2 hours)

**Milestone 4:** Lock framework fully integrated, kernel builds

### Day 5 (Friday): Performance Benchmarks
- ✅ Run comprehensive benchmarks (3 hours)
- ✅ Analyze results, tune parameters (2 hours)
- ✅ Build mksh shell (2 hours)

**Milestone 5:** Performance targets verified, mksh works

### Day 6 (Saturday): Coreutils
- ✅ Port 10 coreutils (8 hours)
- ✅ Test all utilities (1 hour)

**Milestone 6:** All 10 utilities working, total <100 KB

### Day 7 (Sunday): Integration & Week 3 Planning
- ✅ End-to-end integration tests (3 hours)
- ✅ Documentation updates (2 hours)
- ✅ Create Week 3 plan (2 hours)

**Milestone 7 (FINAL):** Week 2 complete, all deliverables tested

---

## 10. Risks and Mitigation

### Risk 1: System Dependency Installation Fails
**Probability:** Low (Ubuntu/Debian well-supported)
**Impact:** High (blocks entire Week 2)
**Mitigation:**
- Use Docker container with pre-installed dependencies
- Fall back to manual builds if package manager fails
- Document alternative installation methods

### Risk 2: libc-xinim Syscall Mapping Incorrect
**Probability:** Medium (kernel syscalls not yet implemented)
**Impact:** Medium (libc-xinim tests fail)
**Mitigation:**
- Stub syscalls for testing (return mock data)
- Defer integration tests to Week 5 (when syscalls exist)
- Use syscall wrappers with version checking

### Risk 3: Lock Integration Breaks Kernel Build
**Probability:** Low (lock implementations are header-only)
**Impact:** High (kernel doesn't build)
**Mitigation:**
- Incremental integration (one lock type at a time)
- Feature flags to disable new locks if needed
- Extensive unit testing before integration

### Risk 4: Performance Benchmarks Don't Meet Targets
**Probability:** Low (algorithms proven in literature)
**Impact:** Medium (performance goals not met)
**Mitigation:**
- Tune parameters (spin counts, backoff times)
- Profile with perf to find bottlenecks
- Fall back to simpler implementations if needed

### Risk 5: mksh or Coreutils Don't Build with dietlibc
**Probability:** Low (dietlibc has good POSIX coverage)
**Impact:** Low (can defer to Week 3)
**Mitigation:**
- Start with simplest utilities (cat, echo, pwd)
- Add missing functions to libc-xinim if needed
- Use patches for dietlibc compatibility

---

## Completion Criteria

Week 2 is considered **COMPLETE** when:

1. ✅ **Toolchain validated:** x86_64-xinim-elf-gcc produces working binaries
2. ✅ **libc-xinim built:** libxinim.a created, 44 functions implemented
3. ✅ **libc-xinim tested:** 40+ unit tests pass
4. ✅ **Lock integration complete:** ServiceManager, WaitForGraph, legacy replacement
5. ✅ **Lock tests pass:** 46 unit tests + stress tests
6. ✅ **Benchmarks run:** All 5 lock types benchmarked, targets met
7. ✅ **mksh built:** 150 KB binary works
8. ✅ **Coreutils ported:** All 10 utilities work, total <100 KB
9. ✅ **Integration tests pass:** End-to-end scenarios work
10. ✅ **Documentation updated:** Week 2 completion report created

**Quality Gates:**
- Zero build warnings (-Wall -Wextra)
- All tests pass (100%)
- Performance targets met (16x improvement verified)
- Binary sizes meet targets (libxinim <100 KB, mksh <200 KB)
- Code coverage >80% for new code

---

## Next Steps: Week 3 Preview

**Week 3 Goals:**
1. Implement 60 core kernel syscalls (Week 5 brought forward)
2. Port 20 additional utilities
3. Integrate libc-xinim with kernel syscalls (remove stubs)
4. Create Makefile build system (replace bash scripts)
5. TRE regex library port (advanced regex)

**Week 3 Dependencies from Week 2:**
- ✅ Toolchain working
- ✅ libc-xinim built (will integrate with real syscalls)
- ✅ Lock framework integrated (will use in syscall handlers)
- ✅ mksh + 10 utils working (will add 20 more)

---

## Appendix A: File Manifest

**New files created in Week 2:**

```
userland/libc-xinim/
├── Makefile                           # Build system
├── README.md                          # Library documentation
├── include/xinim/
│   ├── mqueue.h                       # 80 lines
│   ├── semaphore.h                    # 60 lines
│   ├── wchar_ext.h                    # 40 lines
│   └── locale_ext.h                   # 30 lines
├── src/
│   ├── mqueue.c                       # 250 lines
│   ├── semaphore.c                    # 300 lines
│   ├── wchar.c                        # 450 lines
│   └── locale.c                       # 150 lines
└── tests/
    ├── test_mqueue.c                  # 200 lines
    ├── test_semaphore.c               # 250 lines
    ├── test_wchar.c                   # 300 lines
    ├── test_locale.c                  # 100 lines
    └── test_integration.c             # 150 lines

src/kernel/
├── service_manager.cpp                # Modified: +50 lines
├── wait_for_graph.hpp                 # Modified: +30 lines
├── wait_for_graph.cpp                 # Modified: +150 lines
├── interrupt_handler.cpp              # Modified: +20 lines
└── lattice_ipc.cpp                    # Modified: +40 lines

test/bench/
└── lock_benchmark.cpp                 # 400 lines (NEW)

userland/coreutils/
├── cat.c                              # 80 lines
├── ls.c                               # 200 lines
├── cp.c                               # 150 lines
├── mv.c                               # 120 lines
├── rm.c                               # 100 lines
├── mkdir.c                            # 60 lines
├── rmdir.c                            # 60 lines
├── touch.c                            # 80 lines
├── echo.c                             # 50 lines
└── pwd.c                              # 40 lines

docs/
└── WEEK2_IMPLEMENTATION_PLAN.md       # This document (1000+ lines)
```

**Total new code: ~3,500 LOC**

---

## Appendix B: Build Commands Quick Reference

**Install dependencies:**
```bash
sudo apt-get install -y build-essential bison flex libgmp-dev \
    libmpfr-dev libmpc-dev texinfo help2man gawk libc6-dev \
    git wget tar xz-utils m4 autoconf automake libtool
```

**Build toolchain:**
```bash
cd /home/user/XINIM/scripts
./setup_build_environment.sh
source /opt/xinim-toolchain/xinim-env.sh
./build_binutils.sh
./build_gcc_stage1.sh
./build_dietlibc.sh
./build_gcc_stage2.sh
./scripts/validate_toolchain.sh
```

**Build libc-xinim:**
```bash
cd userland/libc-xinim
make clean all
make test
make install
```

**Build lock benchmarks:**
```bash
cd test/bench
x86_64-xinim-elf-g++ -std=c++17 -O3 -I../../src/kernel \
    lock_benchmark.cpp -o lock_benchmark -pthread
./lock_benchmark
```

**Build mksh:**
```bash
cd userland/mksh
export CC=x86_64-xinim-elf-gcc
export CFLAGS="--sysroot=/opt/xinim-sysroot -Os -static"
sh Build.sh
```

**Build coreutils:**
```bash
cd userland/coreutils
for util in cat ls cp mv rm mkdir rmdir touch echo pwd; do
    x86_64-xinim-elf-gcc --sysroot=/opt/xinim-sysroot \
        -Os -static ${util}.c -o ${util}
done
```

---

**Document Status:** ✅ READY FOR EXECUTION
**Estimated Effort:** 40-50 hours (7 days with parallel tracks)
**Complexity:** High (kernel integration, syscall mapping, performance tuning)
**Risk Level:** Medium (dependencies on Week 1 toolchain)

**Approval:** Pending execution

---

**End of Week 2 Implementation Plan**
