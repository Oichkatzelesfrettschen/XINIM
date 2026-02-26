# XINIM: Modern C++23 Post-Quantum Microkernel Operating System

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![POSIX](https://img.shields.io/badge/POSIX-2024-green.svg)](https://pubs.opengroup.org/onlinepubs/9799919799/)
[![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](LICENSE)

XINIM is an advanced C++23 reimplementation of MINIX that extends the classic microkernel architecture with post-quantum cryptography, hardware abstraction layers, and sophisticated mathematical foundations. This research operating system demonstrates modern systems programming while maintaining educational clarity.

## 🚀 Key Innovations

### Pure C++23 Implementation
- **C++23 Core**: Migration to pure C++23 is in progress (legacy C remains in libc/third_party)
- **Modern Language Features**: Concepts, ranges, coroutines, modules
- **Compile-time Optimization**: Extensive constexpr and template metaprogramming
- **RAII Throughout**: Automatic resource management and exception safety

### Hardware Abstraction Layer (HAL)
- **x86_64 Architecture**: Focused on modern 64-bit Intel/AMD processors
- **SIMD Optimization**: AVX2/AVX512 vectorization for high-performance operations
- **Runtime CPU Detection**: Automatic selection of optimal code paths
- **Platform Primitives**: Unified interface for memory barriers, prefetch, atomics
- **QEMU Support**: Full support for x86_64 QEMU virtualization

### Post-Quantum Security
- **ML-KEM (Kyber)**: NIST-standardized lattice-based key encapsulation
- **SIMD-Accelerated Crypto**: 4-16x speedup with vectorized NTT operations
- **Constant-time Operations**: Side-channel resistant implementations
- **XChaCha20-Poly1305**: Authenticated encryption for secure channels

### Advanced Architecture
- **Microkernel Design**: Minimal kernel with user-mode servers
- **Lattice IPC**: Capability-based inter-process communication
- **DAG Scheduling**: Dependency-aware scheduling with deadlock detection
- **Service Resurrection**: Automatic fault recovery with dependency ordering

## 📁 POSIX-Compliant Project Structure

```
XINIM/
├── bin/                    # Executables and scripts
├── include/               # Public headers (.hpp only)
│   ├── sys/              # System headers
│   ├── c++/              # C++ standard library extensions
│   ├── posix/            # POSIX API headers
│   └── xinim/            # XINIM-specific headers
├── lib/                   # Static/Shared libraries
├── src/                   # Source code
│   ├── kernel/           # Microkernel core
│   ├── hal/              # Hardware Abstraction Layer
│   ├── crypto/           # Cryptography implementations
│   ├── fs/               # Filesystem implementations
│   ├── mm/               # Memory management
│   ├── net/              # Networking stack
│   └── tools/            # Build tools and utilities
├── test/                  # Test suites
│   ├── unit/             # Unit tests
│   ├── integration/      # Integration tests
│   └── posix/            # POSIX compliance tests
├── docs/                  # Documentation
│   ├── api/              # API documentation
│   ├── guides/           # User guides
│   └── specs/            # Specifications
├── scripts/               # Build and development scripts
├── third_party/           # External dependencies
└── CMakeLists.txt        # Primary build system
```

## 🏗️ Build System

XINIM uses **CMake + Conan** as the primary build system, providing:

- **C++23 Standardization**: Consistent feature detection and enforcement
- **Cross-platform**: Linux, macOS, Windows, BSD
- **Multi-target**: Debug and Release presets via CMake
- **Dependency Management**: Conan packages and generated toolchains
- **Toolchain Flexibility**: GCC, Clang, MSVC support

### Quick Start

```bash
# Clone and build
git clone https://github.com/Oichkatzelesfrettschen/XINIM.git
cd XINIM

# Install dependencies and configure toolchain
scripts/conan_install.sh build Debug

# Build with CMake presets
cmake --preset debug
cmake --build --preset debug
```

### Development Setup

```bash
# Full development environment
scripts/conan_install.sh build Debug
cmake --preset debug
cmake --build --preset debug
ctest --output-on-failure --test-dir build
```

## 🧪 Testing & Quality Assurance

### Comprehensive Test Coverage
- **Unit Tests**: Individual component testing
- **Integration Tests**: System-level verification
- **POSIX Compliance**: Official test suite validation
- **Performance Benchmarks**: Continuous optimization

### Code Quality Tools
- **Static Analysis**: Clang-Tidy, cppcheck
- **Dynamic Analysis**: Valgrind, Sanitizers
- **Coverage**: gcov, lcov integration
- **Documentation**: Doxygen + Sphinx

## 🔧 Architecture Deep Dive

### Layered Design Philosophy

XINIM follows a mathematically rigorous layered architecture:

1. **L0 - Mathematical Foundations**
   - Formal security models
   - Capability algebra (octonion-based)
   - Compositional verification

2. **L1 - Abstract Contracts**
   - State machines for capabilities
   - IPC channel specifications
   - Scheduling invariants

3. **L2 - Algorithmic Realization**
   - Concrete data structures
   - Dependency DAG scheduling
   - Lattice-based IPC

4. **L3 - C++23 Implementation**
   - Concepts and constraints
   - Template metaprogramming
   - RAII resource management

5. **L4 - Tool Chain Integration**
   - CMake + Conan build system
   - Cross-platform toolchains
   - Automated testing

### Hardware Abstraction Layer (HAL)

The HAL provides unified interfaces across architectures:

```cpp
// Architecture-agnostic SIMD operations
#include <xinim/hal/simd.hpp>

void process_data(std::span<float> data) {
    if (hal::simd::has_avx2()) {
        hal::simd::avx2::vectorized_sum(data);
    } else if (hal::simd::has_neon()) {
        hal::simd::neon::vectorized_sum(data);
    } else {
        hal::simd::scalar::vectorized_sum(data);
    }
}
```

## 🔐 Post-Quantum Cryptography

XINIM integrates NIST-standardized post-quantum algorithms:

### ML-KEM (Kyber) Implementation
- **Lattice-based**: Resistant to quantum attacks
- **SIMD Acceleration**: 4-16x performance improvement
- **Constant-time**: Side-channel attack protection
- **FIPS 203**: Compliant with NIST standards

```cpp
#include <xinim/crypto/kyber.hpp>

void secure_communication() {
    // Generate keypair
    auto [public_key, private_key] = kyber::generate_keypair();

    // Encapsulate shared secret
    auto [ciphertext, shared_secret] = kyber::encapsulate(public_key);

    // Decapsulate on receiver side
    auto received_secret = kyber::decapsulate(ciphertext, private_key);

    // Use shared secret for symmetric encryption
    assert(shared_secret == received_secret);
}
```

## 📚 Documentation

Comprehensive documentation is available in multiple formats:

- **API Reference**: Doxygen-generated HTML
- **Architecture Guide**: Sphinx-based documentation
- **POSIX Compliance**: Detailed implementation notes
- **Performance Analysis**: Benchmark results and optimization guides

```bash
# Generate full documentation
doxygen docs/Doxyfile
sphinx-build -b html docs/sphinx docs/sphinx/html
xdg-open docs/sphinx/html/index.html
```

## 🤝 Contributing

We welcome contributions! Please see our [contributing guide](CONTRIBUTING.md) for details.

### Development Workflow
1. Fork the repository
2. Create a feature branch
3. Make your changes with comprehensive tests
4. Ensure all tests pass: `ctest --output-on-failure --test-dir build`
5. Submit a pull request

### Code Standards
- **C++23**: Use modern language features
- **POSIX Compliance**: Follow UNIX conventions
- **Documentation**: Doxygen comments required
- **Testing**: 100% test coverage expected

## 📄 License

XINIM is licensed under the BSD 3-Clause License. See [LICENSE](LICENSE) for details.

## 🙏 Acknowledgments

- **Original MINIX**: Foundation for microkernel design
- **NIST**: Post-quantum cryptography standards
- **LLVM/Clang**: Excellent C++23 toolchain
- **CMake + Conan**: Modern build and dependency tooling

---

**XINIM**: Where mathematics meets systems programming in perfect harmony.
