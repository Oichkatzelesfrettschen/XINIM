# XINIM Architecture

XINIM is a modern C++23 reimplementation of MINIX that extends the classic microkernel architecture with post-quantum cryptography, advanced mathematical foundations, and sophisticated scheduling. This document describes the layered architecture and key innovations.

## Overview

XINIM preserves MINIX's educational clarity while incorporating cutting-edge research in operating systems, cryptography, and mathematical computing. The system maintains the clean separation between a minimal kernel and user-mode servers while adding modern security and scheduling capabilities.

## Core Principles

1. **Microkernel Architecture**: Minimal kernel with user-mode servers (PM, MM, FS, RS, DS)
2. **Post-Quantum Security**: ML-KEM (Kyber) for secure channel establishment
3. **Mathematical Foundations**: Capability algebra grounded in octonion mathematics
4. **Modern C++23**: Type-safe, RAII-based system programming
5. **Educational Clarity**: Maintainable, well-documented code for learning

## Layered Architecture

### L0 - Mathematical Model

**Security Lattice (𝒮)**
- Complete lattice for information flow control
- Security labels with join (⊔) and meet (⊓) operations
- Monotonic flow: information can only flow to higher security levels

**Capability Algebra (𝒞)**
- Non-associative algebra for delegation semantics
- Rights, labels, and provenance tracking
- Octonion-inspired composition with order sensitivity

**Budget Semiring (𝒯)**
- Resource accounting and scheduling tokens
- Additive composition for budget pooling
- Multiplicative sequencing for execution costs

### L1 - Kernel Contract

**Core Abstractions**
```cpp
namespace xinim {
    using pid_t = std::int32_t;        // Process identifiers
    using phys_addr_t = std::uint64_t; // Physical memory addresses
    using virt_addr_t = std::uint64_t; // Virtual memory addresses
}
```

**IPC State Machines**
- Channel establishment with capability exchange
- Message passing with security label verification
- Zero-copy grants for bulk data transfer

**Scheduling Invariants**
- Priority-based with CFS-style time sharing
- DAG-based dependency tracking for deadlock avoidance
- Budget enforcement with time slice accounting

### L2 - Algorithmic Implementation

**Lattice IPC System** (`lattice::`)
```cpp
struct Channel {
    xinim::pid_t src, dst;
    net::node_t node_id;
    std::deque<message> queue;
    std::array<std::uint8_t, 32> aead_key; // XChaCha20-Poly1305
};

class Graph {
    std::map<std::tuple<pid_t, pid_t, node_t>, Channel> edges_;
    std::unordered_map<pid_t, bool> listening_;
};
```

**Scheduler** (`sched::`)
- Multi-level feedback queues with priority inheritance
- Wait-for graph maintenance for deadlock detection
- Service dependency DAG for resurrection ordering

**Service Management** (`svc::`)
- Resurrection server for fault tolerance
- Dependency tracking between services
- State persistence and recovery coordination

### L3 - C++23 Implementation

**Core Types** (`xinim::core_types.hpp`)
- Modern type definitions with strong typing
- Platform-abstracted address types
- RAII resource management throughout

**Mathematical Primitives**
- `lattice::Octonion` for capability tokens
- `lattice::Sedenion` for extended algebras
- Constant-time operations for security

**Post-Quantum Cryptography** (`pqcrypto::`)
```cpp
struct KeyPair {
    std::array<uint8_t, KYBER512_PUBLICKEY_BYTES> public_key;
    std::array<uint8_t, KYBER512_SECRETKEY_BYTES> private_key;
};

KeyPair generate_keypair() noexcept;
std::array<uint8_t, 32> compute_shared_secret(const KeyPair& local, 
                                             const KeyPair& peer) noexcept;
```

### L4 - System Integration

**Build System**
- CMake with Clang 18+ requirement
- Cross-compilation support for x86-64
- Doxygen + Sphinx documentation pipeline

**User Mode Servers**
- **PM (Process Manager)**: Process lifecycle and signal handling
- **MM (Memory Manager)**: Virtual memory and paging policy  
- **FS (File System)**: VFS layer with MINIX filesystem support
- **RS (Resurrection Server)**: Service monitoring and restart
- **DS (Data Store)**: Configuration and state persistence
- **NET**: Network stack with BSD sockets compatibility

## Key Innovations

### Post-Quantum IPC

Traditional IPC is enhanced with ML-KEM (Kyber) key establishment:

1. **Channel Bootstrap**: First connection performs KEM handshake
2. **Session Keys**: Derived shared secret enables XChaCha20-Poly1305 AEAD
3. **Zero-Copy Security**: Grant maps with encrypted metadata
4. **Forward Secrecy**: Per-session keys with ephemeral properties

### Capability Algebra

Non-associative algebra where delegation order matters:

```cpp
// x ⋄ y ≠ y ⋄ x in general (order-sensitive delegation)
std::optional<Cap> delegate(const Cap& grantor, const Cap& grantee);
```

Rights are restricted based on provenance, preventing confused deputy attacks
while allowing flexible policy composition.

### DAG-Based Scheduling

The scheduler maintains a dependency DAG to:
- Detect deadlocks before they occur
- Order service restarts during recovery
- Optimize scheduling decisions based on blocking relationships

### Resurrection Architecture

Service failures are handled through:
1. **Heartbeat Monitoring**: Regular liveness checks
2. **Dependency Analysis**: Identify affected services
3. **Coordinated Restart**: Topological ordering of recovery
4. **State Restoration**: Capability and connection reestablishment

## Security Model

### Information Flow

Security labels form a lattice with clear flow rules:
- **Confidentiality**: Information flows only to higher levels
- **Integrity**: Operations require sufficient clearance
- **Availability**: Resources protected by capability requirements

### Capability System

Capabilities consist of:
- **Rights**: Bit-vector of permitted operations
- **Label**: Security classification requirement
- **Provenance**: Delegation history and epoch tracking

### Post-Quantum Properties

- **Key Encapsulation**: ML-KEM provides quantum-resistant key exchange
- **Perfect Forward Secrecy**: Session keys are ephemeral
- **Authenticated Encryption**: XChaCha20-Poly1305 for confidentiality and integrity
- **Side-Channel Resistance**: Constant-time implementations

## Implementation Status

**Completed**
- ✅ Core mathematical foundations (octonion algebra)
- ✅ Post-quantum cryptography integration (Kyber)
- ✅ Lattice-based IPC with encryption
- ✅ DAG-based scheduling infrastructure
- ✅ Service management and resurrection
- ✅ Modern C++23 throughout codebase

**In Progress**
- 🔄 Complete user-mode server implementations
- 🔄 STREAMS integration for modular I/O
- 🔄 Rump kernel integration for drivers
- 🔄 MIPS32 port alongside x86-64

**Planned**
- 📋 Hardware abstraction layer completion
- 📋 WebAssembly userland support
- 📋 Advanced scheduling algorithms
- 📋 Distributed systems extensions

## Educational Use

XINIM serves as a modern teaching platform that demonstrates:
- **Classical OS Concepts**: Through MINIX-compatible interfaces
- **Modern Security**: Post-quantum cryptography in practice
- **Advanced Mathematics**: Applied algebra in system design
- **Contemporary C++**: Modern language features in systems programming

The codebase maintains pedagogical clarity while showcasing state-of-the-art
techniques in operating system research and implementation.

---

For detailed API documentation, see the generated Sphinx documentation at `docs/sphinx/html/index.html`.