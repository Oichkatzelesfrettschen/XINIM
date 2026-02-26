# XINIM Userland Implementation Plan

## Overview

Complete POSIX userland with mksh as the default shell, full coreutils suite, compilers, and development tools for a production-ready OS with development environment.

## Architecture

### Component Categories

1. **Shell** - mksh (MirBSD Korn Shell)
2. **Core Utilities** - POSIX coreutils (extended from existing)
3. **Compilers & Toolchain** - GCC/Clang cross-compiler, binutils
4. **Development Tools** - make, git, debugger, profiler
5. **System Libraries** - libc, libm, libpthread
6. **Network Tools** - ping, telnet, ssh, wget/curl
7. **System Administration** - init, service manager, package manager

## Phase 1: Shell Environment (mksh)

### mksh Integration

**Source**: https://github.com/MirBSD/mksh
**License**: BSD-like (compatible with XINIM)
**Size**: ~150KB source code

**Features**:
- Full POSIX sh compliance
- Bash-compatible extensions
- Command-line editing (emacs/vi modes)
- Job control
- Signal handling
- Command history
- Tab completion
- Aliases and functions

**Implementation**:
```
userland/shell/mksh/
├── mksh.c              # Main mksh source (single-file)
├── sh.h                # Shell header
├── Makefile            # Build configuration
└── integration/
    ├── xinim_syscalls.c    # XINIM-specific syscalls
    ├── xinim_terminal.c    # Terminal I/O integration
    └── xinim_job_control.c # Job control integration
```

**Build Integration**:
```lua
-- xmake.lua
target("mksh")
    set_kind("binary")
    add_files("userland/shell/mksh/mksh.c")
    add_files("userland/shell/mksh/integration/*.c")
    add_includedirs("include")
    add_links("xinim_libc")
```

## Phase 2: Core Utilities Extension

### Existing Commands (Already Implemented)

✅ File Operations: cat, cp, mv, rm, ln, chmod, chown, touch, mkdir, rmdir
✅ Text Processing: grep, sed (awk), cut, tr, wc, head, tail, sort, uniq
✅ System: ps, kill, sleep, time, sync, mount, umount
✅ Development: make, ar, cc (compiler driver)
✅ Editors: mined (editor)

### Missing POSIX Utilities to Implement

**File/Directory**:
- [ ] find - Search for files
- [ ] xargs - Build and execute command lines
- [ ] file - Determine file type
- [ ] du - Disk usage
- [ ] tree - Directory tree display

**Text Processing**:
- [ ] diff - Compare files line by line
- [ ] patch - Apply diff to files
- [ ] sed - Stream editor (modernize existing)
- [ ] expand/unexpand - Convert tabs/spaces
- [ ] fold - Wrap text to width
- [ ] join - Join lines of two files
- [ ] paste - Merge lines of files

**Archive/Compression**:
- [ ] gzip/gunzip - Compress files
- [ ] bzip2/bunzip2 - Block-sorting compression
- [ ] tar - Archive utility (modernize existing)
- [ ] zip/unzip - ZIP archive
- [ ] compress/uncompress - LZW compression

**Network**:
- [ ] ping - ICMP echo
- [ ] netstat - Network statistics
- [ ] ifconfig - Network interface configuration
- [ ] route - Routing table management
- [ ] telnet - Remote terminal
- [ ] ftp - File transfer
- [ ] wget/curl - HTTP client

**System Admin**:
- [ ] init - System initialization
- [ ] shutdown/reboot - System power management
- [ ] useradd/userdel - User management
- [ ] groupadd/groupdel - Group management
- [ ] cron - Scheduled tasks
- [ ] logger - System logging

**Development**:
- [ ] ld - Linker
- [ ] as - Assembler  
- [ ] nm - Symbol table viewer
- [ ] objdump - Object file dumper
- [ ] strip - Remove symbols
- [ ] gdb - Debugger
- [ ] strace - System call tracer
- [ ] ltrace - Library call tracer

## Phase 3: Compiler Toolchain

### GCC Cross-Compiler

**Target**: x86_64-xinim-elf
**Components**:
- binutils (as, ld, ar, nm, objdump, strip)
- gcc (C/C++ compiler)
- libgcc (compiler support library)
- libstdc++ (C++ standard library)

**Build Process**:
```bash
# Build binutils
../binutils-2.41/configure \
    --target=x86_64-xinim-elf \
    --prefix=/opt/xinim-toolchain \
    --with-sysroot=/opt/xinim-sysroot \
    --disable-nls --disable-werror

# Build GCC (stage 1 - bootstrap)
../gcc-13.2.0/configure \
    --target=x86_64-xinim-elf \
    --prefix=/opt/xinim-toolchain \
    --with-sysroot=/opt/xinim-sysroot \
    --enable-languages=c,c++ \
    --without-headers \
    --disable-hosted-libstdcxx

# Build newlib (C library alternative)
../newlib-4.3.0/configure \
    --target=x86_64-xinim-elf \
    --prefix=/opt/xinim-toolchain

# Build GCC (stage 2 - full)
# Rebuild with newlib
```

### LLVM/Clang Alternative

**Advantages**:
- Modular architecture
- Better diagnostics
- Faster compilation
- Integrated sanitizers

```bash
cmake -G Ninja ../llvm-project/llvm \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/xinim-toolchain \
    -DLLVM_ENABLE_PROJECTS="clang;lld;lldb" \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-xinim-elf
```

## Phase 4: System Libraries

### libc Implementation

**Based on**: musl libc (small, fast, standards-compliant)
**Size**: ~600KB

**Core Components**:
```
userland/libc/
├── include/           # POSIX headers
│   ├── stdio.h
│   ├── stdlib.h
│   ├── string.h
│   ├── unistd.h
│   ├── sys/types.h
│   └── ...
├── src/
│   ├── stdio/         # I/O functions
│   ├── stdlib/        # Memory, environment
│   ├── string/        # String operations
│   ├── unistd/        # POSIX syscalls
│   ├── math/          # Math functions
│   ├── pthread/       # Threading
│   ├── network/       # Socket APIs
│   └── signal/        # Signal handling
└── arch/x86_64/       # Architecture-specific
    ├── syscall.s      # System call interface
    ├── setjmp.s       # Context switching
    └── atomic.h       # Atomic operations
```

**Key Features**:
- Full POSIX.1-2024 compliance
- Thread-safe by design
- Small memory footprint
- No bloat or legacy cruft
- C++23 backend with C ABI

### libm (Math Library)

**Functions**:
- Trigonometric (sin, cos, tan, asin, acos, atan)
- Exponential (exp, log, pow, sqrt)
- Special functions (erf, gamma, bessel)
- Complex numbers

**Optimization**:
- SIMD implementations (AVX2/AVX512)
- Lookup tables for common values
- Taylor/Chebyshev approximations

### libpthread (Threading)

**Components**:
- pthread_create/join/detach
- Mutexes (pthread_mutex_*)
- Condition variables (pthread_cond_*)
- Read-write locks (pthread_rwlock_*)
- Thread-local storage (TLS)
- Barriers, semaphores

## Phase 5: Development Environment

### Build Tools

**make** (already implemented):
- GNU Make compatibility
- Parallel builds (-j)
- Pattern rules
- Automatic dependencies

**cmake** (optional):
- Cross-platform build generator
- Out-of-source builds
- Find modules for libraries

**ninja** (optional):
- Fast parallel builds
- Simple dependency graph

### Version Control

**git**:
- Full Git implementation
- libgit2 backend
- All porcelain commands
- Remote operations (https, ssh)

**Implementation**:
```
userland/git/
├── libgit2/          # Git library
├── git-core/         # Core Git commands
└── integration/
    └── xinim_fs.c    # Filesystem integration
```

### Debugger

**gdb** (GNU Debugger):
- Source-level debugging
- Breakpoints, watchpoints
- Stack traces
- Variable inspection
- Remote debugging (gdbserver)

**lldb** (LLVM Debugger - alternative):
- Modern C++ codebase
- Python scripting
- Better C++ support

### Profiler

**gprof**:
- Call graph profiling
- Execution time analysis
- Flat profile

**perf** (Linux perf tool port):
- Hardware counter access
- CPU profiling
- Cache analysis

## Phase 6: Network Stack Integration

### Protocol Implementation

**TCP/IP Stack** (lwIP):
- IPv4/IPv6
- TCP, UDP, ICMP
- DHCP client
- DNS resolver
- Raw sockets

**Network Utilities**:
```
userland/net/
├── ping.cpp          # ICMP echo
├── traceroute.cpp    # Route tracing
├── netstat.cpp       # Network statistics
├── ifconfig.cpp      # Interface configuration
├── route.cpp         # Routing table
├── telnet.cpp        # Remote terminal
├── ssh/              # Secure shell
│   ├── openssh/      # OpenSSH port
│   └── dropbear/     # Lightweight alternative
└── wget.cpp          # HTTP downloader
```

### SSH Implementation

**Dropbear** (lightweight):
- SSH2 protocol
- SCP support
- Small footprint (~200KB)
- No external dependencies

**OpenSSH** (full-featured):
- Complete SSH suite
- Agent forwarding
- Port forwarding
- SFTP support

## Phase 7: Package Management

### XINIM Package Manager (xpkg)

**Features**:
- Binary package distribution
- Source package builds
- Dependency resolution
- Atomic updates
- Rollback support

**Structure**:
```
userland/xpkg/
├── xpkg.cpp          # Main package manager
├── repository.cpp    # Repository management
├── package.cpp       # Package operations
├── dependency.cpp    # Dependency resolver
└── database.cpp      # Installed package DB
```

**Package Format**:
```
package.xpkg (tar.gz):
├── MANIFEST          # Package metadata
├── INSTALL           # Installation script
├── files.tar.gz      # Package files
└── signature         # Digital signature
```

## Implementation Timeline

### Week 1: mksh Integration
- [ ] Port mksh source to XINIM
- [ ] Integrate with XINIM terminal I/O
- [ ] Implement job control
- [ ] Test command execution
- [ ] Add to build system

### Week 2: Core Utilities Completion
- [ ] Implement find, xargs
- [ ] Implement diff, patch
- [ ] Implement gzip, tar improvements
- [ ] Implement network tools (ping, netstat)
- [ ] Test suite for all utilities

### Week 3: Compiler Toolchain
- [ ] Build binutils cross-compiler
- [ ] Build GCC stage 1
- [ ] Port newlib or create minimal libc
- [ ] Build GCC stage 2 with libc
- [ ] Test compilation of simple programs

### Week 4: System Libraries
- [ ] Complete libc implementation
- [ ] Implement libm (math library)
- [ ] Implement libpthread (threading)
- [ ] POSIX compliance testing
- [ ] Performance benchmarking

### Week 5: Development Tools
- [ ] Integrate gdb/lldb
- [ ] Add profiling tools
- [ ] Version control (git integration)
- [ ] Documentation tools
- [ ] IDE support (LSP server)

### Week 6: Network Stack & Tools
- [ ] Complete lwIP integration
- [ ] Implement network utilities
- [ ] SSH client/server (Dropbear)
- [ ] HTTP client (wget/curl)
- [ ] Network testing

### Week 7: Package Management
- [ ] Design package format
- [ ] Implement xpkg manager
- [ ] Create base package repository
- [ ] Build automation
- [ ] Distribution infrastructure

## Testing Strategy

### Unit Tests
- Each utility has test suite
- POSIX compliance verification
- Edge case coverage
- Performance regression tests

### Integration Tests
- Shell script execution
- Pipeline operations
- Multi-process coordination
- Network communication

### Compatibility Tests
- Run existing shell scripts
- Compile real-world programs
- Standard benchmark suites
- POSIX test suite

## Success Criteria

✅ **Shell**: mksh runs interactive and scripted commands
✅ **Utilities**: All POSIX utilities implemented and tested
✅ **Compiler**: Can compile and link C/C++ programs
✅ **Libraries**: Full POSIX libc with threading
✅ **Network**: TCP/IP stack with SSH connectivity
✅ **Development**: Can build XINIM from within XINIM
✅ **Packages**: Package manager deploys software

## Resources

### External Projects to Port/Integrate

1. **mksh**: https://github.com/MirBSD/mksh
2. **musl libc**: https://musl.libc.org/
3. **busybox**: https://busybox.net/ (for utilities reference)
4. **lwIP**: https://savannah.nongnu.org/projects/lwip/
5. **Dropbear SSH**: https://matt.ucc.asn.au/dropbear/dropbear.html
6. **libgit2**: https://libgit2.org/
7. **GCC/binutils**: https://gcc.gnu.org/, https://www.gnu.org/software/binutils/

### Documentation

- POSIX.1-2024: https://pubs.opengroup.org/onlinepubs/9799919799/
- System V ABI: https://refspecs.linuxfoundation.org/elf/x86_64-abi-0.99.pdf
- Intel SDM: https://www.intel.com/sdm
- mksh manual: https://www.mirbsd.org/htman/i386/man1/mksh.htm

## Directory Structure

```
XINIM/
├── userland/
│   ├── shell/
│   │   └── mksh/              # MirBSD Korn Shell
│   ├── coreutils/             # POSIX utilities
│   │   ├── file/              # File operations
│   │   ├── text/              # Text processing
│   │   ├── system/            # System utilities
│   │   └── network/           # Network tools
│   ├── toolchain/
│   │   ├── binutils/          # Assembler, linker, etc.
│   │   ├── gcc/               # GCC compiler
│   │   └── llvm/              # LLVM/Clang (alternative)
│   ├── libc/                  # C standard library
│   │   ├── include/           # Public headers
│   │   ├── src/               # Implementation
│   │   └── arch/x86_64/       # Architecture-specific
│   ├── libm/                  # Math library
│   ├── libpthread/            # Threading library
│   ├── development/
│   │   ├── gdb/               # Debugger
│   │   ├── git/               # Version control
│   │   └── profiling/         # Performance tools
│   ├── network/
│   │   ├── lwip/              # TCP/IP stack
│   │   ├── ssh/               # Dropbear SSH
│   │   └── tools/             # Network utilities
│   └── package/
│       └── xpkg/              # Package manager
└── sysroot/                   # System root for userland
    ├── bin/                   # User binaries
    ├── sbin/                  # System binaries
    ├── lib/                   # Libraries
    ├── include/               # Headers
    ├── etc/                   # Configuration
    ├── var/                   # Variable data
    └── usr/                   # User programs
        ├── bin/
        ├── lib/
        ├── include/
        └── share/
```

## Code Quality Standards

### All Userland Components

- **Language**: C++23 with C ABI exports
- **Style**: XINIM coding standards
- **Documentation**: Doxygen comments
- **Testing**: Unit tests + integration tests
- **Memory Safety**: RAII, smart pointers, bounds checking
- **Thread Safety**: Mutex protection where needed
- **Error Handling**: errno-based for C ABI, exceptions internally
- **Performance**: SIMD optimization where applicable

### Build Requirements

- **Compiler**: Clang 16+ or GCC 13+
- **C++ Standard**: C++23
- **C Standard**: C17 for ABI
- **Build System**: xmake with automatic dependencies
- **Testing**: Catch2 for C++, criterion for C

## Next Steps

1. **Start with mksh** - Get shell working first
2. **Complete coreutils** - Add missing POSIX utilities
3. **Build toolchain** - Enable native compilation
4. **Implement libc** - Full POSIX library support
5. **Add development tools** - gdb, git, profilers
6. **Network stack** - lwIP + network utilities
7. **Package manager** - xpkg for software distribution

---

**Status**: Ready to begin Phase 1 (mksh integration)
**Target**: Complete userland in 7 weeks
**Confidence**: HIGH - Clear plan with proven components
