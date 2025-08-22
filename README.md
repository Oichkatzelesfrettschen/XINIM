# XINIM: Post-Quantum Microkernel Operating System

XINIM is an advanced C++23 reimplementation of MINIX that extends the classic microkernel architecture with post-quantum cryptography, lattice-based IPC, and sophisticated mathematical foundations. This research operating system demonstrates modern systems programming while maintaining educational clarity.

**🔬 Research Focus**: Post-quantum security, capability-based access control, and advanced scheduling in microkernel architectures

**🎓 Educational**: Clean, well-documented code suitable for operating systems coursework

**🛡️ Security**: ML-KEM (Kyber) encryption, octonion-based capability algebra, and information flow control

## 🚀 Key Features

### Post-Quantum Security
- **ML-KEM (Kyber)**: NIST-standardized lattice-based key encapsulation for quantum-resistant IPC
- **XChaCha20-Poly1305**: Authenticated encryption for secure message channels
- **Constant-time operations**: Side-channel resistant cryptographic implementations

### Advanced Architecture
- **Microkernel Design**: Minimal kernel with user-mode servers (PM, MM, FS, RS, DS)
- **Lattice IPC**: Capability-based inter-process communication with security labels
- **DAG Scheduling**: Dependency-aware scheduling with deadlock detection
- **Service Resurrection**: Automatic fault detection and coordinated service restart

### Mathematical Foundations
- **Octonion Algebra**: Non-associative algebra for capability delegation semantics
- **Security Lattice**: Information flow control with join/meet operations
- **Budget Semiring**: Resource accounting and execution cost modeling

### Modern Implementation
- **C++23**: Latest language features with strong type safety and RAII
- **Template Metaprogramming**: Compile-time optimizations and type safety
- **Comprehensive Testing**: Unit tests, integration tests, and property-based testing
- **Documentation**: Doxygen + Sphinx for comprehensive API documentation

---

## 🏗️ Architecture Overview

XINIM extends the classic MINIX microkernel with modern security and scheduling capabilities:

```
┌─────────────────────────────────────────────────────────────────┐
│                        User Mode Servers                        │
├─────────────┬─────────────┬─────────────┬─────────────┬─────────┤
│     PM      │     MM      │     FS      │     RS      │    DS   │
│  Process    │   Memory    │    File     │ Resurrection│  Data   │
│  Manager    │  Manager    │   System    │   Server    │ Store   │
└─────────────┴─────────────┴─────────────┴─────────────┴─────────┘
│                     Lattice IPC (Post-Quantum)                  │
├─────────────────────────────────────────────────────────────────┤
│                       XINIM Microkernel                         │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐│
│  │ Scheduling  │ │   Lattice   │ │ Capability  │ │   Memory    ││
│  │    (DAG)    │ │     IPC     │ │   System    │ │ Management  ││
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘│
└─────────────────────────────────────────────────────────────────┘
│                        Hardware Layer                           │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

- **kernel/**: Microkernel with scheduling, IPC, and memory management
- **crypto/**: Post-quantum cryptography (ML-KEM implementation)
- **mm/**: Memory management with virtual memory and paging
- **fs/**: MINIX filesystem with modern caching and optimization
- **lib/**: Standard library and runtime support
- **commands/**: UNIX-compatible utilities (75+ commands)
- **tests/**: Comprehensive test suite with property-based testing

---

## 🔧 Building and Development

### Prerequisites

XINIM requires modern development tools for C++23 and post-quantum cryptography:

- **Compiler**: Clang 18+ (preferred) or GCC 13+ with full C++23 support
- **Build System**: CMake 3.10+ and Make
- **Dependencies**: OpenSSL (for system crypto), optional libsodium
- **Documentation**: Doxygen + Sphinx with Breathe extension

**Quick Setup** (Ubuntu 24.04 LTS):
```bash
sudo apt-get update
sudo apt-get install -y clang-18 libc++-18-dev libc++abi-18-dev
sudo apt-get install -y cmake ninja-build doxygen python3-sphinx python3-breathe
sudo apt-get install -y libssl-dev pkg-config
```

For detailed platform-specific instructions, see [`docs/TOOL_INSTALL.md`](docs/TOOL_INSTALL.md).

### Build Process

**CMake (Recommended)**:
```bash
mkdir build && cd build
cmake -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)  # Builds crypto libraries and core components
```

**Cross-compilation** for freestanding x86-64:
```bash
cmake -DCROSS_COMPILE_X86_64=ON -DCROSS_PREFIX=x86_64-elf- ..
```

**Make (Component-specific)**:
```bash
make all          # Build all targets
make clean        # Remove build artifacts
make docs         # Generate documentation
```

### Testing and Validation

Run the comprehensive test suite covering IPC, scheduling, and cryptography:
```bash
# Build and run all tests
cd build && make test

# Run specific test categories  
./test_lattice_ipc          # Lattice IPC system tests
./test_scheduler            # DAG scheduling tests
./test_hypercomplex         # Octonion/sedenion mathematics
./test_net_driver           # Network and wormhole tests
./test_wait_graph           # Deadlock detection tests
```

### Documentation Generation

Generate complete API documentation with mathematical foundations:
```bash
# Generate Doxygen XML for C++ APIs
doxygen Doxyfile

# Build Sphinx documentation with Breathe integration
cd docs/sphinx
sphinx-build -b html . html/

# Open generated documentation
open html/index.html  # Shows architecture, API reference, and mathematical foundations
```

---

## 🔬 Research Contributions

XINIM advances the state of microkernel operating systems through several key innovations:

### Post-Quantum Microkernel Security
- **First implementation** of ML-KEM (Kyber) in microkernel IPC
- **Zero-copy encrypted messaging** with capability-based access control
- **Side-channel resistant** constant-time cryptographic operations

### Mathematical Operating Systems
- **Octonion-based capability algebra** for non-associative delegation semantics
- **Information flow lattice** integrated into the kernel security model
- **Budget semiring** for resource accounting and fair scheduling

### Advanced Scheduling Architecture
- **DAG-based deadlock prevention** using wait-for graphs
- **Service resurrection** with dependency-aware restart ordering
- **Capability-mediated scheduling** with mathematical priority functions

### Educational Platform
- **Modern C++23 showcase** in systems programming context
- **Clean microkernel pedagogy** with advanced features
- **Research-grade implementation** suitable for academic study

---

## Cleaning the Workspace

```bash
# Remove all build artefacts and test outputs
./tools/clean.sh
```

or, if using CMake directly:

```bash
cmake --build build --target clean
rm -rf build/
```

---

## Documentation

| Document                      | Description                         |
| ----------------------------- | ----------------------------------- |
| `docs/BUILDING.md`            | Full build and flashing guide       |
| `docs/ARCHITECTURE.md`        | Subsystem overview; see [`docs/sphinx/architecture.rst`](docs/sphinx/architecture.rst) |
| `docs/TOOL_INSTALL.md`        | OS-specific dependency list         |
| `docs/sphinx/html/index.html` | Generated developer manual in HTML  |

---

## Sort Utility and Advanced Features

XINIM includes modern implementations of classic UNIX utilities with enhanced capabilities:

**Enhanced Sort Utility**: The `sort` command supports multi-file merge mode with the `-m` flag. Each input file must already be sorted; the utility performs a streaming k-way merge using the same comparison rules as regular sorting. When combined with the `-u` option, duplicate lines encountered across input files are removed during the merge.

**75+ UNIX Commands**: All classic utilities modernized with C++23 for improved safety and performance.

**Advanced Mathematical Computing**: Built-in support for octonion and sedenion algebras for research applications.

---

## License

Licensed under the **BSD-3-Clause** license. See `LICENSE` for details.

---

**XINIM** represents the next generation of educational operating systems - combining the pedagogical clarity of MINIX with cutting-edge research in post-quantum security, mathematical computing, and advanced microkernel architecture.
