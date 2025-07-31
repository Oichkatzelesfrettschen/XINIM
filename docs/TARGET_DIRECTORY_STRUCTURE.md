# XINIM OS Target Directory Structure

This document outlines the proposed target directory structure for the XINIM Operating System, designed to enhance modularity, clarity, and build system integration.

```
xinim/
├── KERNEL/
│   ├── ARCH/                   # Architecture-specific code (e.g., x86_64, arm64)
│   │   └── X86_64/
│   │       ├── boot/           # Limine bootloader integration, early boot assembly
│   │       ├── cpu/            # CPU features, APIC, SMP
│   │       ├── fpu_simd/       # FPU/SIMD context switching, alignment helpers
│   │       └── mm/             # MMU setup, paging, HAT-like layer
│   ├── CORE/                   # Core kernel functionality (mostly arch-independent)
│   │   ├── include/          # Internal kernel core headers
│   │   ├── config.hpp        # Kernel compile-time configuration
│   │   ├── main.cpp          # Kernel entry point (kmain)
│   │   ├── pmm.cpp           # Physical Memory Manager
│   │   ├── vmm.cpp           # Virtual Memory Manager (arch-independent parts)
│   │   ├── scheduler.cpp     # Process/thread scheduler (DAG-aware)
│   │   ├── process.cpp       # Process/thread management
│   │   ├── syscall.cpp       # System call dispatcher
│   │   └── timer.cpp         # System timer, scheduling clock
│   ├── IPC/
│   │   ├── include/
│   │   │   └── public/       # Public IPC API headers (e.g., lattice_api.hpp)
│   │   ├── lattice_ipc.cpp   # Core Lattice-DAG channel management
│   │   ├── wormhole.cpp      # Fast-path IPC (shared memory, direct calls)
│   │   ├── ALGEBRAIC/        # Algebraic libraries for IPC & Security
│   │   │   ├── include/      # Public headers for algebraic types
│   │   │   ├── quaternion/   # Quaternion library & spinlocks
│   │   │   ├── octonion/     # Octonion library (capabilities, channel IDs)
│   │   │   └── sedenion/     # Sedenion library (quantum-safe crypto constructs)
│   │   └── pqcrypto.cpp      # Post-quantum KEM/Signatures for channel security
│   ├── DRIVERS/                # Device drivers (can be kernel or user-space loadable)
│   │   ├── include/
│   │   ├── BUS/                # Bus drivers (PCI, USB)
│   │   ├── NET/                # Network interface card drivers
│   │   ├── STORAGE/            # Disk controller drivers
│   │   ├── INPUT/              # Keyboard, mouse
│   │   └── DISPLAY/            # Framebuffer, console
│   └── INCLUDE/                # Public kernel headers (for LibOSes and servers)
│       └── XINIM/
│           ├── kernel_api.hpp  # Main interface for kernel services
│           └── types.hpp       # Core XINIM types (pid_t, capability_t, etc.)
├── SERVERS/                    # System server processes
│   ├── FILESYSTEM/
│   │   ├── include/
│   │   └── src/
│   ├── MEMORY/                 # Unified Memory Manager server OR in-kernel UMM
│   │   ├── include/
│   │   └── src/
│   ├── NETWORK/
│   │   ├── include/
│   │   └── src/                # Network stack (TCP/IP)
│   └── SERVICE_MANAGER/        # SMF-like service manager
│       ├── include/
│       └── src/
├── LIB/                        # User-space libraries
│   ├── LIBC/                   # Standard C library
│   │   ├── include/
│   │   ├── src/
│   │   └── sysdeps/xinim/    # System call wrappers for XINIM
│   ├── LIBM/                   # Math library
│   ├── LIBXINIM/               # XINIM-specific APIs for applications
│   └── LIBPTHREAD/             # POSIX threads library
├── USERLAND/                   # User-space programs and utilities
│   ├── BIN/                    # Core utilities (sh, ls, cat, etc.)
│   ├── SBIN/                   # System administration utilities
│   ├── ETC/                    # System configuration files
│   └── APPS/                   # Example applications, ports
├── TESTS/                      # Test suites
│   ├── KERNEL/                 # Kernel unit and integration tests
│   ├── LIBC/                   # Libc tests
│   ├── POSIX_COMPLIANCE/       # POSIX compliance test suites (e.g., LTP)
│   └── BENCHMARKS/
├── DOCS/                       # Documentation
│   ├── ARCHITECTURE/           # Detailed design documents (this will be one)
│   ├── SPHINX/                 # Sphinx source for HTML/PDF docs
│   │   └── source/
│   └── DOXYGEN/                # Doxygen configuration and output
├── TOOLS/                      # Build, development, and analysis tools
│   ├── build/                  # CMake toolchain files, helper scripts
│   ├── analysis/               # Scripts for cloc, lizard, cppcheck
│   └── setup.sh                # Environment setup script
├── BUILD/                      # Build output directory (CMake)
└── MISC/                       # Miscellaneous files (e.g., .clang-format, .gitignore)
```

## Initial File Mapping (Illustrative - requires detailed scan)

This is a *preliminary* mapping based on common XINIM files and typical OS structures. A detailed mapping will require analyzing `docs/TREE_FULL.txt` and the contents of each file.

*   **`boot.S`**: `KERNEL/ARCH/X86_64/boot/boot.S`
*   **`kernel.cpp`** (main entry): `KERNEL/CORE/main.cpp`
*   **`linker.ld`**: `KERNEL/ARCH/X86_64/boot/linker.ld`
*   **`grub.cfg`**: `KERNEL/ARCH/X86_64/boot/grub.cfg` (or `BOOT/grub/grub.cfg` at root if using GRUB directly)
*   **`multiboot.h`**: `KERNEL/ARCH/X86_64/boot/include/multiboot.h` (or a more general `KERNEL/INCLUDE/XINIM/boot/`)
*   **`console.cpp`, `console.h`**: Likely `KERNEL/DRIVERS/DISPLAY/console.cpp` and `KERNEL/DRIVERS/INCLUDE/xinim/drivers/console.hpp` (or `KERNEL/CORE/` if very basic)
*   **`pmm.cpp`, `pmm.h`**: `KERNEL/CORE/pmm.cpp`, `KERNEL/CORE/include/pmm.hpp`
*   **`vmm.cpp`, `vmm.h`**: `KERNEL/CORE/vmm.cpp`, `KERNEL/CORE/include/vmm.hpp` (arch-independent parts), with arch-specifics in `KERNEL/ARCH/X86_64/mm/`
*   **`common/math/quaternion.*`**: `KERNEL/IPC/ALGEBRAIC/quaternion/`
*   **`common/math/octonion.*`**: `KERNEL/IPC/ALGEBRAIC/octonion/`
*   **`common/math/sedenion.*`**: `KERNEL/IPC/ALGEBRAIC/sedenion/`
*   **`tools/setup.sh`**: `TOOLS/setup.sh` (already correct)
*   **`docs/*`**: Reorganize within `DOCS/` (e.g., `DOCS/ARCHITECTURE/`, `DOCS/CONCEPTS/`)
*   **Existing `Makefile`**: Will be superseded by CMake files. Store for reference in `MISC/legacy_build/`.

### Files from Memory Unification Guide (minix/*)

*   `minix/kernel/kalloc.c`, `minix/kernel/malloc.c`, `minix/kernel/palloc.c`, `minix/kernel/alloc.c`: These are existing allocators. Their logic will be analyzed and potentially refactored into the Unified Memory Manager (UMM). If UMM is in-kernel, parts go to `KERNEL/CORE/umm_*.cpp`. If UMM is a server, `SERVERS/MEMORY/src/`.
*   `minix/kernel/bootstrap/kalloc.c`: `KERNEL/ARCH/X86_64/boot/bootstrap_alloc.cpp` (or generic `KERNEL/CORE/bootstrap_alloc.cpp` if possible).
*   `minix/include/xk_malloc.h`, `minix/kernel/2Sys/malloc.h`, `minix/kernel/2Sys/zalloc.h`, `minix/kernel/head/malloc.h`: These headers would be consolidated or their relevant parts moved into new UMM headers, likely under `KERNEL/INCLUDE/XINIM/mm/` or `SERVERS/MEMORY/include/`.
*   `minix/lib/libmalloc/*`, `minix/lib/libmtmalloc/*`, etc.: User-space library allocators. Their functionality will be provided by a unified `LIB/LIBC/src/malloc/` which uses the UMM via syscalls (if UMM is in kernel) or IPC (if UMM is a server).
*   `servers/memory/*` (from guide's proposed structure): This maps directly to `SERVERS/MEMORY/`. The `memory_manager.hpp`, `allocator.hpp` would go into `SERVERS/MEMORY/include/`.

A more detailed file-by-file mapping will be generated after thorough analysis of the existing codebase using the reports from Step 1.
