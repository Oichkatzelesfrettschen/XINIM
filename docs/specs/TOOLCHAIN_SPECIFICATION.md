# XINIM Cross-Compiler Toolchain Technical Specification
**Version:** 1.0
**Date:** 2025-11-17
**Status:** Implementation Ready

---

## 1. Overview

This document specifies the technical requirements for the XINIM cross-compiler toolchain, a complete development environment for building x86_64-xinim-elf binaries targeting the XINIM operating system.

### 1.1 Purpose

The XINIM toolchain enables:
- Compilation of freestanding code (kernel, bootloader)
- Compilation of userland programs (with musl libc)
- Linking with XINIM system libraries
- Debugging XINIM programs
- Self-hosting (building XINIM from within XINIM)

### 1.2 Scope

**In Scope:**
- binutils 2.41 (assembler, linker, binary utilities)
- GCC 13.2 (C/C++ compiler)
- musl 1.2.4 (C standard library)
- libstdc++ (C++ standard library)
- LLVM/Clang 18.1 (alternative toolchain, optional)

**Out of Scope:**
- Fortran, Ada, Go compilers
- Cross-debugging tools (gdbserver on XINIM)
- IDE integration

---

## 2. Architecture

### 2.1 Target Architecture

**Target Triplet:** `x86_64-xinim-elf`

| Component | Value |
|-----------|-------|
| Architecture | x86_64 (64-bit Intel/AMD) |
| Vendor | xinim (custom) |
| OS | elf (ELF binary format, freestanding) |
| ABI | System V AMD64 ABI |

**Microarchitecture Baseline:** x86-64-v1 (2003 AMD64/EM64T)

**Required Instructions:**
- SSE, SSE2 (mandatory for x86_64)
- CMOV, CMPXCHG8B, FPU, FXSR, MMX
- OSFXSR, SCE (syscall/sysret)

**Excluded (for maximum compatibility):**
- SSSE3, SSE4.1, SSE4.2 (x86-64-v2)
- AVX, AVX2, FMA (x86-64-v3)
- AVX512 (x86-64-v4)

**Note:** Runtime SIMD detection in XINIM kernel can use AVX2/AVX512 when available.

### 2.2 Toolchain Components

```
x86_64-xinim-elf-toolchain
├── binutils-2.41
│   ├── as (assembler)
│   ├── ld (linker, gold linker)
│   ├── ar (archiver)
│   ├── nm (symbol table)
│   ├── objcopy (object file converter)
│   ├── objdump (object file disassembler)
│   ├── ranlib (archive index)
│   ├── readelf (ELF file inspector)
│   ├── size (section sizes)
│   ├── strings (extract strings)
│   └── strip (symbol stripper)
├── gcc-13.2.0
│   ├── gcc (C compiler)
│   ├── g++ (C++ compiler)
│   ├── cpp (C preprocessor)
│   ├── libgcc (compiler support library)
│   └── libstdc++ (C++ standard library)
├── musl-1.2.4
│   ├── libc.a (C standard library)
│   ├── libm.a (math library)
│   ├── libpthread.a (threading library)
│   ├── crt1.o, crti.o, crtn.o (startup files)
│   └── headers (stdio.h, stdlib.h, unistd.h, etc.)
└── llvm-18.1.0 (optional)
    ├── clang (C/C++ compiler)
    ├── clang++ (C++ compiler)
    ├── lld (linker)
    ├── lldb (debugger)
    └── compiler-rt (runtime library)
```

---

## 3. Build Configuration

### 3.1 Directory Layout

```
/opt/xinim-toolchain/          # Toolchain installation prefix
├── bin/                       # Executable programs
│   ├── x86_64-xinim-elf-gcc
│   ├── x86_64-xinim-elf-g++
│   ├── x86_64-xinim-elf-as
│   ├── x86_64-xinim-elf-ld
│   └── ... (other tools)
├── lib/
│   └── gcc/x86_64-xinim-elf/13.2.0/
│       ├── libgcc.a
│       ├── libgcc_eh.a
│       └── include/
├── libexec/gcc/               # GCC internal programs
├── share/                     # Documentation, man pages
└── xinim-env.sh               # Environment setup script

/opt/xinim-sysroot/            # System root for target
├── lib/                       # System libraries
│   ├── libc.a
│   ├── libm.a
│   ├── libpthread.a
│   ├── libstdc++.a
│   ├── crt1.o, crti.o, crtn.o
│   └── ld-musl-x86_64.so.1 (dynamic linker, optional)
├── lib64 -> lib               # Symlink for compatibility
├── usr/
│   ├── include/               # System headers
│   │   ├── stdio.h
│   │   ├── stdlib.h
│   │   ├── unistd.h
│   │   ├── sys/
│   │   └── ... (all POSIX headers)
│   ├── lib/ -> ../lib         # Symlink
│   └── lib64 -> ../lib        # Symlink
├── bin/                       # Target binaries (userland)
├── sbin/                      # System binaries
├── etc/                       # Configuration files
├── var/                       # Variable data
├── tmp/                       # Temporary files
├── dev/                       # Device files
├── proc/                      # Process filesystem (mount)
└── sys/                       # Sysfs (mount)

$HOME/xinim-build/             # Build directories
├── binutils-build/
├── gcc-stage1-build/
├── musl-build/
└── gcc-stage2-build/

$HOME/xinim-sources/           # Source code
├── binutils-2.41/
├── gcc-13.2.0/
├── musl-1.2.4/
└── linux-6.6/ (headers)
```

### 3.2 binutils Configuration

**Version:** 2.41

**Configure Options:**
```bash
./configure \
    --target=x86_64-xinim-elf \
    --prefix=/opt/xinim-toolchain \
    --with-sysroot=/opt/xinim-sysroot \
    --disable-nls \
    --disable-werror \
    --enable-gold \
    --enable-ld=default \
    --enable-plugins \
    --enable-lto \
    --enable-deterministic-archives \
    --enable-relro \
    --enable-threads \
    --with-pic
```

**Key Features:**
- **Gold Linker:** Fast parallel linker (alternative to GNU ld)
- **LTO Support:** Link-Time Optimization plugin
- **RELRO:** Read-only relocation for security
- **Deterministic Archives:** Reproducible builds

**Build Time:** ~5-10 minutes (with `-j$(nproc)`)

**Installed Size:** ~150 MB

### 3.3 GCC Stage 1 Configuration

**Version:** 13.2.0 (bootstrap compiler, no libc)

**Configure Options:**
```bash
./configure \
    --target=x86_64-xinim-elf \
    --prefix=/opt/xinim-toolchain \
    --with-sysroot=/opt/xinim-sysroot \
    --enable-languages=c,c++ \
    --without-headers \
    --with-newlib \
    --disable-nls \
    --disable-shared \
    --disable-multilib \
    --disable-decimal-float \
    --disable-threads \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libquadmath \
    --disable-libssp \
    --disable-libvtv \
    --disable-libstdcxx \
    --enable-lto \
    --with-arch=x86-64 \
    --with-tune=generic
```

**Purpose:** Build freestanding code (kernel, bootloader)

**Build Time:** ~20-30 minutes (with `-j$(nproc)`)

**Installed Size:** ~500 MB

**Capabilities:**
- ✅ Compile C/C++ code without libc
- ✅ Inline assembly
- ✅ Freestanding attributes
- ❌ Cannot use standard library functions (printf, malloc, etc.)

### 3.4 musl libc Configuration

**Version:** 1.2.4

**Configure Options:**
```bash
CC=x86_64-xinim-elf-gcc \
AR=x86_64-xinim-elf-ar \
RANLIB=x86_64-xinim-elf-ranlib \
./configure \
    --target=x86_64 \
    --prefix=/opt/xinim-sysroot/usr \
    --syslibdir=/opt/xinim-sysroot/lib \
    --disable-shared \
    --enable-static \
    --enable-optimize \
    --disable-debug \
    --enable-warnings \
    CFLAGS="-Os -pipe -fomit-frame-pointer \
            -fno-unwind-tables -fno-asynchronous-unwind-tables \
            -march=x86-64 -mtune=generic"
```

**Features:**
- Full POSIX.1-2017 compliance
- Thread-safe by design
- Small footprint (~650 KB source, ~1 MB static library)
- No legacy cruft
- Clean, readable code

**Build Time:** ~5 minutes (with `-j$(nproc)`)

**Installed Size:** ~2 MB

**Provided Libraries:**
- `libc.a` - C standard library
- `libm.a` - Math library (linked into libc.a)
- `libpthread.a` - Threading library (linked into libc.a)
- `libcrypt.a` - Cryptography library
- `libdl.a` - Dynamic linking (stub for static builds)
- `librt.a` - Realtime extensions (linked into libc.a)

**Startup Files:**
- `crt1.o` - Main program startup
- `crti.o` - Constructor prologue
- `crtn.o` - Constructor epilogue
- `Scrt1.o` - Position-independent startup (PIE)
- `rcrt1.o` - Fully relocatable startup

### 3.5 GCC Stage 2 Configuration

**Version:** 13.2.0 (full compiler with libc and libstdc++)

**Configure Options:**
```bash
./configure \
    --target=x86_64-xinim-elf \
    --prefix=/opt/xinim-toolchain \
    --with-sysroot=/opt/xinim-sysroot \
    --enable-languages=c,c++ \
    --disable-nls \
    --enable-shared \
    --enable-static \
    --disable-multilib \
    --enable-threads=posix \
    --enable-tls \
    --enable-lto \
    --enable-plugins \
    --enable-gold \
    --enable-ld=default \
    --enable-libstdcxx \
    --enable-libstdcxx-time=yes \
    --enable-libstdcxx-filesystem-ts=yes \
    --with-arch=x86-64 \
    --with-tune=generic \
    --with-fpmath=sse \
    --enable-clocale=generic \
    --disable-werror
```

**Build Time:** ~30-60 minutes (with `-j$(nproc)`)

**Installed Size:** ~2 GB

**Capabilities:**
- ✅ Full C17 support
- ✅ Full C++23 support
- ✅ POSIX threads (pthread)
- ✅ Thread-local storage (TLS)
- ✅ Link-Time Optimization (LTO)
- ✅ Standard library (libstdc++)
- ✅ Filesystem support (std::filesystem)

---

## 4. Compiler Flags

### 4.1 Target-Specific Flags

**Baseline (required for all builds):**
```makefile
CFLAGS   = -march=x86-64 -mtune=generic -mno-red-zone
CXXFLAGS = -march=x86-64 -mtune=generic -mno-red-zone
LDFLAGS  = -z max-page-size=4096
```

**Explanation:**
- `-march=x86-64`: Target x86-64-v1 baseline (maximum compatibility)
- `-mtune=generic`: Optimize for average x86-64 CPU
- `-mno-red-zone`: Disable red zone (required for kernel code)
- `-z max-page-size=4096`: Use 4 KB pages (default on x86-64)

### 4.2 Optimization Levels

**Debug Build:**
```makefile
CFLAGS += -g3 -O0 -fno-omit-frame-pointer
```

**Development Build:**
```makefile
CFLAGS += -g -O2 -fno-strict-aliasing
```

**Release Build:**
```makefile
CFLAGS += -O2 -DNDEBUG -flto
LDFLAGS += -flto -fuse-linker-plugin
```

**Performance Build:**
```makefile
CFLAGS += -O3 -march=native -flto -ffast-math
LDFLAGS += -flto -fuse-linker-plugin
```

**Size-Optimized Build:**
```makefile
CFLAGS += -Os -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections
```

### 4.3 Security Hardening

**Stack Protection:**
```makefile
CFLAGS += -fstack-protector-strong
```

**Position-Independent Code:**
```makefile
CFLAGS += -fPIC
LDFLAGS += -pie
```

**Full RELRO:**
```makefile
LDFLAGS += -Wl,-z,relro,-z,now
```

**No Execute Stack:**
```makefile
LDFLAGS += -Wl,-z,noexecstack
```

**Buffer Overflow Detection:**
```makefile
CFLAGS += -D_FORTIFY_SOURCE=2
```

### 4.4 C++23 Specific Flags

```makefile
CXXFLAGS += -std=c++23 \
            -fno-exceptions \
            -fno-rtti \
            -fno-threadsafe-statics
```

**Explanation:**
- `-std=c++23`: Enable C++23 standard
- `-fno-exceptions`: Disable exceptions (kernel code)
- `-fno-rtti`: Disable runtime type information (smaller binaries)
- `-fno-threadsafe-statics`: Disable thread-safe static initialization (kernel)

**Note:** For userland code, enable exceptions and RTTI:
```makefile
CXXFLAGS += -std=c++23 -fexceptions -frtti
```

---

## 5. ELF Binary Format

### 5.1 ELF Header

**Class:** ELFCLASS64 (64-bit)
**Data:** ELFDATA2LSB (little-endian)
**Machine:** EM_X86_64 (x86-64)
**Type:** ET_EXEC (executable) or ET_DYN (shared object)
**ABI:** ELFOSABI_SYSV (System V)
**ABI Version:** 0

### 5.2 Program Headers

**Required Segments:**

| Type | VirtAddr | Permissions | Contents |
|------|----------|-------------|----------|
| LOAD | 0x400000 | R-X | .text, .rodata |
| LOAD | 0x600000 | RW- | .data, .bss |
| DYNAMIC | - | R-- | Dynamic linking info |
| GNU_STACK | - | RW- | Stack (non-executable) |
| GNU_RELRO | - | R-- | Read-only after relocation |

**Page Alignment:** 4096 bytes (4 KB)

### 5.3 Section Headers

**Required Sections:**

| Section | Type | Flags | Purpose |
|---------|------|-------|---------|
| .text | PROGBITS | AX | Executable code |
| .rodata | PROGBITS | A | Read-only data |
| .data | PROGBITS | WA | Initialized data |
| .bss | NOBITS | WA | Uninitialized data |
| .symtab | SYMTAB | - | Symbol table |
| .strtab | STRTAB | - | String table |
| .shstrtab | STRTAB | - | Section header string table |

**Optional Sections:**
- `.init`, `.fini` - Initialization/finalization
- `.ctors`, `.dtors` - Global constructors/destructors
- `.eh_frame` - Exception handling frames
- `.debug_*` - DWARF debugging information

### 5.4 Dynamic Linking

**Dynamic Linker:** `/lib/ld-musl-x86_64.so.1`

**Dynamic Sections:**
- `.dynamic` - Dynamic linking information
- `.dynsym` - Dynamic symbol table
- `.dynstr` - Dynamic string table
- `.rela.dyn` - Dynamic relocations
- `.rela.plt` - PLT relocations
- `.plt`, `.plt.got` - Procedure Linkage Table
- `.got`, `.got.plt` - Global Offset Table

**RPATH/RUNPATH:** `/opt/xinim-sysroot/lib`

---

## 6. System V AMD64 ABI

### 6.1 Calling Convention

**Function Arguments (integer/pointer):**
1. RDI
2. RSI
3. RDX
4. RCX
5. R8
6. R9
7+ Stack (right-to-left)

**Function Arguments (floating-point):**
1-8. XMM0-XMM7
9+ Stack

**Return Values:**
- Integer/pointer: RAX (64-bit), RDX:RAX (128-bit)
- Floating-point: XMM0 (double), XMM0:XMM1 (long double)

**Caller-Saved Registers:**
- RAX, RCX, RDX, RSI, RDI, R8-R11
- XMM0-XMM15

**Callee-Saved Registers:**
- RBX, RBP, R12-R15
- (No SSE registers are callee-saved)

**Stack Alignment:** 16 bytes before `call` instruction

### 6.2 Syscall Convention

**Instruction:** `syscall` (SYSCALL/SYSRET fast system calls)

**Arguments:**
- Syscall number: RAX
- Arguments 1-6: RDI, RSI, RDX, R10, R8, R9
- Return value: RAX (negative on error, -errno)

**Clobbered Registers:** RCX, R11 (return address, RFLAGS)

**Note:** This matches Linux x86_64 syscall ABI for compatibility.

### 6.3 Thread-Local Storage

**Model:** `initial-exec` or `local-exec`

**FS Segment:** Points to thread control block (TCB)

**TLS Layout:**
```
FS:0x00 - TCB pointer (self)
FS:0x08 - Dynamic thread vector (dtv)
FS:0x10 - Thread ID (tid)
FS:0x18 - Thread-specific errno
FS:0x20+ - Thread-local variables
```

---

## 7. Toolchain Usage

### 7.1 Environment Setup

```bash
# Source environment script
source /opt/xinim-toolchain/xinim-env.sh

# Verify toolchain
which x86_64-xinim-elf-gcc
x86_64-xinim-elf-gcc --version
```

### 7.2 Compiling C Programs

**Simple Program:**
```bash
x86_64-xinim-elf-gcc \
    --sysroot=/opt/xinim-sysroot \
    -o hello hello.c
```

**Static Linking:**
```bash
x86_64-xinim-elf-gcc \
    --sysroot=/opt/xinim-sysroot \
    -static \
    -o hello hello.c
```

**With Optimization:**
```bash
x86_64-xinim-elf-gcc \
    --sysroot=/opt/xinim-sysroot \
    -O2 -flto \
    -o hello hello.c
```

### 7.3 Compiling C++ Programs

**C++23 Program:**
```bash
x86_64-xinim-elf-g++ \
    --sysroot=/opt/xinim-sysroot \
    -std=c++23 \
    -o hello hello.cpp
```

**With Exceptions Disabled (kernel):**
```bash
x86_64-xinim-elf-g++ \
    --sysroot=/opt/xinim-sysroot \
    -std=c++23 \
    -fno-exceptions -fno-rtti \
    -o kernel kernel.cpp
```

### 7.4 Linking

**Manual Linking:**
```bash
# Compile to object file
x86_64-xinim-elf-gcc -c program.c -o program.o

# Link with libc
x86_64-xinim-elf-ld \
    --sysroot=/opt/xinim-sysroot \
    -o program \
    /opt/xinim-sysroot/lib/crt1.o \
    /opt/xinim-sysroot/lib/crti.o \
    program.o \
    -lc \
    -lgcc \
    /opt/xinim-sysroot/lib/crtn.o
```

**Note:** Easier to use `gcc` driver which handles startup files automatically.

### 7.5 Creating Static Libraries

```bash
# Compile object files
x86_64-xinim-elf-gcc -c file1.c -o file1.o
x86_64-xinim-elf-gcc -c file2.c -o file2.o

# Create archive
x86_64-xinim-elf-ar rcs libmylib.a file1.o file2.o

# Create index
x86_64-xinim-elf-ranlib libmylib.a

# Use library
x86_64-xinim-elf-gcc program.c -L. -lmylib -o program
```

### 7.6 Inspecting Binaries

```bash
# View ELF header
x86_64-xinim-elf-readelf -h program

# View program headers
x86_64-xinim-elf-readelf -l program

# View section headers
x86_64-xinim-elf-readelf -S program

# View symbols
x86_64-xinim-elf-nm program

# Disassemble
x86_64-xinim-elf-objdump -d program

# View strings
x86_64-xinim-elf-strings program

# Check binary size
x86_64-xinim-elf-size program
```

---

## 8. Verification & Testing

### 8.1 Toolchain Verification

**Compiler Version:**
```bash
x86_64-xinim-elf-gcc --version
# Expected: gcc (GCC) 13.2.0
```

**Target Triplet:**
```bash
x86_64-xinim-elf-gcc -dumpmachine
# Expected: x86_64-xinim-elf
```

**Search Paths:**
```bash
x86_64-xinim-elf-gcc -print-search-dirs
```

**Sysroot:**
```bash
x86_64-xinim-elf-gcc -print-sysroot
# Expected: /opt/xinim-sysroot
```

**Predefined Macros:**
```bash
echo | x86_64-xinim-elf-gcc -dM -E -
# Should include: __x86_64__, __ELF__, etc.
```

### 8.2 Test Programs

**Freestanding C (Stage 1):**
```c
void _start(void) {
    while(1) {
        __asm__ volatile("hlt");
    }
}
```

**C with libc (Stage 2):**
```c
#include <stdio.h>
int main(void) {
    printf("Hello, XINIM!\n");
    return 0;
}
```

**C++23 (Stage 2):**
```cpp
#include <iostream>
#include <vector>
#include <ranges>

int main() {
    auto vec = std::vector{1, 2, 3, 4, 5};
    for (auto x : vec | std::views::filter([](int n){ return n % 2; })) {
        std::cout << x << " ";
    }
    std::cout << "\n";
    return 0;
}
```

### 8.3 Build System Integration

**Makefile Example:**
```makefile
TARGET = x86_64-xinim-elf
CC = $(TARGET)-gcc
CXX = $(TARGET)-g++
AR = $(TARGET)-ar
LD = $(TARGET)-ld

SYSROOT = /opt/xinim-sysroot
CFLAGS = --sysroot=$(SYSROOT) -O2 -Wall -Wextra
CXXFLAGS = --sysroot=$(SYSROOT) -std=c++23 -O2 -Wall -Wextra
LDFLAGS = --sysroot=$(SYSROOT)

all: program

program: main.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o program
```

**xmake Integration:**
```lua
target("xinim_program")
    set_kind("binary")
    set_toolchains("x86_64-xinim-elf")
    set_sysroot("/opt/xinim-sysroot")
    add_files("src/*.c")
    set_optimize("fastest")
    set_languages("c17")
```

---

## 9. Troubleshooting

### 9.1 Common Issues

**Issue: "cannot find crt1.o"**
```
Solution: Ensure musl is built and installed to sysroot
Check: ls /opt/xinim-sysroot/lib/crt1.o
```

**Issue: "undefined reference to `__stack_chk_fail'"**
```
Solution: Link with libssp or disable stack protector
Fix: Add -fno-stack-protector to CFLAGS
```

**Issue: "relocation R_X86_64_32 against `.rodata' can not be used"**
```
Solution: Code is position-dependent but linker expects PIC
Fix: Either use -fPIC or remove -pie from LDFLAGS
```

**Issue: "multiple definition of `__dso_handle'"**
```
Solution: Conflict between libgcc and crtbegin
Fix: Use -nostartfiles and manually specify startup files
```

### 9.2 Performance Issues

**Slow Compilation:**
- Use `-j$(nproc)` for parallel builds
- Use `ccache` for caching
- Use precompiled headers for large projects

**Large Binaries:**
- Use `-Os` for size optimization
- Use `-ffunction-sections -fdata-sections` + `-Wl,--gc-sections`
- Strip symbols: `x86_64-xinim-elf-strip program`
- Use LTO: `-flto` during compile and link

---

## 10. References

### 10.1 Documentation

- GCC Manual: https://gcc.gnu.org/onlinedocs/gcc-13.2.0/gcc/
- binutils Manual: https://sourceware.org/binutils/docs/
- musl Documentation: https://musl.libc.org/
- System V ABI: https://refspecs.linuxfoundation.org/elf/x86_64-abi-0.99.pdf
- Intel SDM: https://www.intel.com/sdm
- C++23 Standard: https://en.cppreference.com/w/cpp/23

### 10.2 Related Documents

- [XINIM SUSv4 Compliance Audit](/home/user/XINIM/docs/SUSV4_POSIX_2017_COMPLIANCE_AUDIT.md)
- [XINIM Roadmap](/home/user/XINIM/docs/ROADMAP_SUSV4_COMPLIANCE.md)
- [XINIM Architecture](/home/user/XINIM/docs/Architecture.md)
- [XINIM Building Guide](/home/user/XINIM/docs/BUILDING.md)

---

**Document Status:** ✅ COMPLETE
**Next Revision:** After toolchain build completion
**Maintainer:** XINIM Toolchain Team
