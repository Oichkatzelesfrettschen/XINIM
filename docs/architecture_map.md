# Architecture Map

```text
```
XINIM Operating System - Repository Architecture Map
=====================================================

XINIM/                              🏗️  Modern C++23 MINIX Implementation
├── 📋 LICENSE                      📄  BSD-MODERNMOST (Retroactive to 1987)
├── ⚙️  CMakeLists.txt              🔧  Modern C++23 Build Configuration  
├── 🔧 .clang-format                📐  LLVM Code Style (4-space, 100-col)
├── 🔍 .clang-tidy                  🛡️  Static Analysis (bugprone+portability)
├── 🚀 boot.S                       ⚡  Multiboot2 x86_64 Assembly Bootloader
├── 🔗 linker.ld                    📍  1MB Physical Load/Virtual Mapping
├── ⚙️  grub.cfg                    🥾  GRUB2 "OctoPOS" Boot Configuration
├── 🖥️  console.cpp/.h              💻  VGA Text Mode Display Driver (→.hpp)
├── 🧠 kernel.cpp                   🎯  Main Kernel Entry Point & Multiboot Parser
├── 💾 pmm.cpp/.h                   🗂️  Physical Memory Manager (→.hpp)
├── 🗄️  vmm.cpp/.h                   🔄  Virtual Memory Manager (→.hpp)
├── 📊 multiboot.h                  📋  Multiboot2 Structure Definitions (→.hpp)
├── 📁 kr_cpp_summary.json          📝  126 Files Needing K&R→C++23 Migration
│
├── 🧰 commands/                    🛠️  75+ UNIX Utilities (Mixed C++23/K&R Legacy)
│   ├── 📦 ar.cpp (C++23)           📚  Archive Utility - CCN 14
│   ├── 📅 cal.cpp (C++23)          🗓️  Calendar Generator - CCN 21  
│   ├── 🐱 cat.cpp (Modern)         📄  File Concatenation
│   ├── ⚙️  cc.cpp (Legacy)          🔨  Compiler Driver - CCN 41 ⚠️
│   ├── 📁 ls.cpp (Legacy)          📋  Directory Listing
│   ├── 🔧 make.cpp (Legacy)        🏗️  Build System (1,779 lines)
│   ├── ✏️  mined1.cpp/mined2.cpp   📝  Text Editor (2,445 lines total)
│   ├── 🗂️  mkfs.cpp (Legacy)       💾  Filesystem Creation
│   ├── 🐚 sh[1-5].cpp (Legacy)     💬  Shell Implementation (5 modules)
│   ├── 🔍 sort.cpp (Modern)        📊  Sorting Utility (847 lines)
│   ├── 📜 roff.cpp (Legacy)        📖  Text Formatting (1,173 lines)
│   └── 🎛️  svcctl.cpp/.hpp         🔧  Service Control (Modern C++23)
│
├── 🧠 kernel/                      ⚡  Core Operating System Kernel
│   ├── 🎯 main.cpp                 🚀  Kernel Bootstrap & Initialization
│   ├── ⏰ clock.cpp                🕐  System Timer & Scheduling
│   ├── 🎭 proc.cpp/.hpp            🔄  Process Management & Context Switching
│   ├── 📡 syscall.cpp              🔌  System Call Interface (ASM Integration)
│   ├── 🖥️  tty.cpp (796 lines)     💻  Terminal/Console Driver
│   ├── 💽 at_wini.cpp/xt_wini.cpp  💾  IDE Hard Disk Drivers
│   ├── 💿 floppy.cpp               📀  Floppy Disk Driver
│   ├── 🌐 net_driver.cpp/.hpp     🔗  Network Interface Abstraction
│   ├── 🔒 pqcrypto.cpp/.hpp       🛡️  Post-Quantum Cryptography Integration
│   ├── 🌀 wormhole.cpp/.hpp       🌌  Novel Network Abstraction Layer
│   ├── 📊 lattice_ipc.cpp/.hpp    🔐  Lattice-Based Secure IPC
│   ├── 🎲 quaternion_spinlock.hpp 🔄  4D Rotation-Based Locking
│   ├── 🔢 octonion.hpp             🧮  8-Dimensional Number System
│   ├── 🌟 sedenion.hpp             ✨  16-Dimensional Algebra
│   ├── 🎯 wait_graph.cpp/.hpp     🕸️  Deadlock Detection Algorithms
│   ├── ⚙️  mpx64.cpp               🔧  64-bit Multiprocessing (9 ASM blocks)
│   └── 📋 schedule.cpp/.hpp        ⏳  Process Scheduler Implementation
│
├── 💾 mm/                          🧠  Memory Management Subsystem  
│   ├── 🎯 main.cpp                 🚀  Memory Manager Bootstrap
│   ├── 🗂️  alloc.cpp/.hpp          📦  Dynamic Memory Allocation
│   ├── 📄 paging.cpp               📋  Page Table Management
│   ├── 🔄 vm.cpp                   🗄️  Virtual Memory Implementation
│   ├── 🍴 forkexit.cpp             👶  Process Creation/Termination
│   ├── 🏃 exec.cpp                 🎬  Program Execution
│   ├── 🔗 break.cpp                📈  Heap Boundary Management
│   ├── 📊 mproc.hpp                📋  Process Control Block Definitions
│   └── 🎛️  signal.cpp              📡  Signal Handling Implementation
│
├── 🗄️  fs/                         📂  MINIX Filesystem Implementation
│   ├── 🎯 main.cpp                 🚀  Filesystem Server Bootstrap  
│   ├── 📖 read.cpp (258 lines)     📄  File Reading Operations - CCN 42 ⚠️
│   ├── ✏️  write.cpp                📝  File Writing Operations
│   ├── 📁 inode.cpp/.hpp           🏷️  File Metadata Management
│   ├── 💾 super.cpp/.hpp           🎯  Filesystem Superblock Operations
│   ├── 🔗 link.cpp                 🌐  Hard/Symbolic Link Management
│   ├── 📂 open.cpp                 🚪  File Opening/Creation
│   ├── 🚪 mount.cpp                🔌  Filesystem Mounting
│   ├── 🔄 cache.cpp                💨  Buffer Cache Management
│   ├── 🚰 pipe.cpp                 🔗  Named/Anonymous Pipe Implementation
│   ├── 📋 device.cpp               🖥️  Device File Interface
│   ├── 🔒 protect.cpp              🛡️  Permission/Access Control
│   └── 🛠️  utility.cpp             🔧  Filesystem Utility Functions
│
├── 📚 lib/                         🏗️  Standard Library & Runtime Support
│   ├── 🧮 regexp.cpp (801 lines)   🔍  Regular Expression Engine
│   ├── 🧠 malloc.cpp               💾  Dynamic Memory Allocator
│   ├── 🔌 syscall_x86_64.cpp      ⚡  64-bit System Call Interface (ASM)
│   ├── 🚀 crt0.cpp/head.cpp       🎬  C Runtime Startup Code
│   ├── 📞 call.cpp                 📡  Inter-process Communication
│   ├── 🍴 fork.cpp                 👶  Process Creation
│   ├── 🎯 getuid.cpp/getgid.cpp   🔑  User/Group ID Management
│   └── 🎭 minix/                   🐧  MINIX-specific Library Extensions
│
├── 🔐 crypto/                      🛡️  Post-Quantum Cryptography Suite
│   ├── 🎯 kyber.cpp/.hpp          🔒  NIST Kyber Lattice-Based Encryption
│   ├── 🔑 pqcrypto_shared.cpp     🤝  Shared Cryptographic Primitives
│   └── 🧬 kyber_impl/             🔬  External NIST Reference Implementation
│       ├── 🔢 fips202.c/.h         🧮  SHA-3/SHAKE Cryptographic Hashing
│       ├── 🎲 ntt.c/.h             ⚡  Number Theoretic Transform  
│       ├── 🔐 kem.c/.h             🗝️  Key Encapsulation Mechanism
│       └── 🎯 indcpa.c/.h          🛡️  Indistinguishable CPA Security
│
├── 🧮 common/math/                 📐  Advanced Mathematical Libraries
│   ├── 🌀 quaternion.cpp/.hpp     🔄  4D Rotation Mathematics
│   ├── 🔢 octonion.cpp/.hpp       🎯  8-Dimensional Number System  
│   └── ✨ sedenion.cpp/.hpp        🌟  16-Dimensional Algebraic System
│
├── 🛠️  tools/                      🔧  Build Tools & Utilities
│   ├── 🏗️  build.cpp (441 lines)   🔨  Boot Image Builder
│   ├── 🗄️  fsck.cpp (1,093 lines)  🔍  Filesystem Checker/Repair
│   ├── 💾 mkfs.cpp (758 lines)     🏗️  Filesystem Creation Utility
│   ├── 💿 diskio.cpp               📀  Low-level Disk I/O Operations
│   └── 🎯 c86/                     🔧  Legacy 8086 Build Tools
│
├── 🧪 tests/                       🔬  Test Suite & Validation
│   ├── 📋 CMakeLists.txt (269 lines) ⚙️  Comprehensive Test Configuration
│   ├── 🧪 test[0-12].cpp          🎯  Core Functionality Tests  
│   ├── 🔧 test_syscall.cpp        📡  System Call Interface Tests
│   └── 🧂 sodium.h                🔐  Cryptographic Testing Framework
│
├── 📖 docs/                        📚  Documentation & Analysis
│   ├── 📋 README_renamed.md        📄  Project Overview & Features
│   ├── 🔨 BUILDING.md              🏗️  Build Instructions & Prerequisites
│   ├── 🗺️  ROADMAP.md               🎯  Strategic Development Plan
│   ├── 🤖 AGENTS.md                🔧  AI Development Guidelines
│   ├── 📊 results.md               📈  Comprehensive Analysis Results
│   ├── ⚙️  Doxyfile                📖  API Documentation Configuration
│   ├── 🌐 sphinx/                  📚  Professional Documentation Pipeline
│   └── 🌳 TREE_CLASSIFIED.txt     📋  Repository Structure Analysis
│
├── 📦 include/                     📂  Modern C++23 Header Organization
│   ├── 🎯 constant_time.hpp       ⏱️  Timing-Attack-Resistant Operations
│   ├── 📏 blocksiz.hpp             📐  Block Size Constants
│   ├── 🗂️  minix/fs/inode.hpp      📁  Filesystem Header Modernization
│   └── 🧮 vm.h                     💾  Virtual Memory Interface (→.hpp)
│
├── 🎯 h/                           📋  Legacy Header Directory (→include/)
│   ├── 📞 callnr.hpp               📱  System Call Numbers
│   ├── 🔧 const.hpp                ⚙️  System Constants
│   ├── 🚨 error.hpp                ⚠️  Error Code Definitions
│   ├── 📡 signal.h                 📢  Signal Definitions (→.hpp)
│   ├── 📊 stat.hpp                 📈  File Status Structures
│   └── 🔤 type.hpp                 📝  Type Definitions
│
└── 📊 Analysis Artifacts           🔍  Code Quality & Metrics
    ├── 📈 analysis_cloc_*.txt      📊  63,715 Lines Across 551 Files
    ├── 🔍 analysis_cppcheck_*.txt  🛡️  Static Analysis Results  
    ├── 🧮 analysis_pmccabe_*.txt   📐  Complexity Analysis (1,671 functions)
    ├── 🏷️  tags (8,636 symbols)     🔍  Code Navigation Database
    ├── 🗂️  cscope.* (448 files)     📋  Cross-Reference Database
    └── 🌳 kr_cpp_summary.json      📝  126 Files Needing Modernization

Legend:
🏗️ = Build/Config    📊 = Analysis     🔧 = Tools        🧠 = Core System
📂 = Filesystem      🔐 = Crypto       🧮 = Math         🛡️ = Security  
⚡ = Performance     🎯 = Entry Point  🚀 = Bootstrap    ⚠️ = High Complexity
📝 = Documentation   🔍 = Analysis     💾 = Storage      🌐 = Network
🎭 = Process Mgmt    📡 = IPC/Signals  🔄 = Concurrency  ✨ = Advanced Math

Repository Statistics:
• Total: 63,715 lines of code across 551 files
• C++23: 337 files (59.6% modern, sophisticated features)  
• Legacy: 126 files requiring K&R→C++23 modernization
• Headers: 104 files (several need .h→.hpp conversion)
• Analysis: 8,636 ctags, 448 indexed files, 10+ quality reports
• Unique Features: Post-quantum crypto, octonion math, lattice IPC
```

```
