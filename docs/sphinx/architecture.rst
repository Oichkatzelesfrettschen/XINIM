XINIM Architecture Reference
============================

This document provides the comprehensive architectural reference for XINIM, a
modern C++23 microkernel operating system with post-quantum security and
mathematical foundations.

Overview
--------

XINIM extends the classic MINIX microkernel architecture with:

* **Post-quantum cryptography** (ML-KEM/Kyber) for secure IPC
* **Mathematical foundations** including octonion-based capability algebra  
* **DAG-based scheduling** with deadlock detection and prevention
* **Service resurrection** for fault-tolerant operation
* **Modern C++23** implementation with type safety and RAII

System Architecture
-------------------

The system follows a strict layered design that separates mathematical
abstractions from concrete implementations:

.. code-block:: text

    ┌─────────────────────────────────────────────────────────────────┐
    │                         User Mode Servers                         │
    ├─────────────┬─────────────┬─────────────┬─────────────┬─────────┤
    │      PM     │      MM     │      FS     │      RS     │   DS    │
    │  Process    │   Memory    │   File      │ Resurrection│  Data   │
    │  Manager    │  Manager    │   System    │   Server    │ Store   │
    └─────────────┴─────────────┴─────────────┴─────────────┴─────────┘
    │                   Lattice IPC (Post-Quantum)                      │
    ├─────────────────────────────────────────────────────────────────┤
    │                         XINIM Microkernel                         │
    │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐│
    │  │ Scheduling  │ │   Lattice   │ │ Capability  │ │   Memory    ││
    │  │    (DAG)    │ │      IPC    │ │   System    │ │ Management  ││
    │  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘│
    └─────────────────────────────────────────────────────────────────┘
    │                         Hardware Layer                            │
    └─────────────────────────────────────────────────────────────────┘

Layered Design
--------------

L0 - Mathematical Model
~~~~~~~~~~~~~~~~~~~~~~~

**Security Lattice (𝒮)**
    Complete lattice for information flow control with join (⊔) and meet (⊓)
    operations. Security labels ensure monotonic information flow.

**Capability Algebra (𝒞)** Non-associative algebra for delegation semantics based on octonion
    mathematics. Tracks rights, security labels, and provenance.

**Budget Semiring (𝒯)**
    Resource accounting with additive budget pooling and multiplicative
    execution sequencing.

L1 - Kernel Contract  
~~~~~~~~~~~~~~~~~~~~

**IPC State Machines**
    Channel establishment, message passing with capability exchange,
    and zero-copy grants for bulk data transfer.

**Scheduling Invariants**
    Priority-based scheduling with CFS-style time sharing, DAG-based
    dependency tracking, and budget enforcement.

**Memory Management**
    Virtual memory primitives, page fault handling, and capability-mediated
    memory mapping.

L2 - Algorithmic Implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Lattice IPC System** (``lattice`` namespace)
    Implements the :cpp:struct:`lattice::Channel` abstraction with
    :cpp:class:`lattice::Graph` for managing IPC endpoints.

**Scheduler** (``sched`` namespace)
    Multi-level feedback queues with :cpp:class:`sched::Scheduler`
    implementing priority inheritance and dependency tracking.

**Service Management** (``svc`` namespace)
    :cpp:class:`svc::ServiceManager` provides resurrection capabilities
    with dependency-aware restart ordering.

L3 - C++23 Implementation
~~~~~~~~~~~~~~~~~~~~~~~~~

**Core Types** (``xinim/core_types.hpp``)
    Modern type definitions with platform abstraction and strong typing.

**Mathematical Primitives**
    :cpp:struct:`lattice::Octonion` for capability tokens and
    constant-time operations for security.

**Post-Quantum Cryptography** (``pqcrypto`` namespace)
    ML-KEM implementation with :cpp:func:`pqcrypto::generate_keypair` and
    :cpp:func:`pqcrypto::compute_shared_secret`.

L4 - System Integration
~~~~~~~~~~~~~~~~~~~~~~~

**Build System**
    CMake configuration with Clang 18+ requirement and cross-compilation
    support for x86-64.

**Documentation Pipeline**
    Doxygen generates XML consumed by Sphinx via Breathe extension for
    integrated API documentation.

Key Innovations
---------------

Post-Quantum IPC
~~~~~~~~~~~~~~~~~

Traditional microkernel IPC enhanced with quantum-resistant cryptography:

1.  **Channel Bootstrap**: ML-KEM handshake on first connection
2.  **Session Keys**: XChaCha20-Poly1305 AEAD with derived shared secrets  
3.  **Zero-Copy Security**: Encrypted metadata in grant maps
4.  **Forward Secrecy**: Ephemeral per-session keys

Mathematical Operating System
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

XINIM integrates advanced mathematics directly into the kernel:

* **Octonion Capability Algebra**: Non-associative delegation where order matters
* **Security Lattice**: Information flow control with mathematical rigor
* **DAG Scheduling**: Dependency tracking prevents deadlocks before occurrence

Service Resurrection Architecture
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Fault tolerance through coordinated service management:

1.  **Heartbeat Monitoring**: Regular liveness checks via control channels
2.  **Dependency Analysis**: DAG traversal identifies affected services  
3.  **Coordinated Restart**: Topological ordering ensures correct recovery
4.  **State Restoration**: Capability and connection reestablishment

API Reference
-------------

Core IPC Functions
~~~~~~~~~~~~~~~~~~

.. cpp:function:: int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id = 0)

    Establish a secure channel between processes with ML-KEM key exchange.

.. cpp:function:: int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg, IpcFlags flags = IpcFlags::NONE)

    Send an encrypted message over an established channel.

.. cpp:function:: int lattice_recv(xinim::pid_t pid, message *msg, IpcFlags flags = IpcFlags::NONE)

    Receive and decrypt a message from the process queue.

Capability Management
~~~~~~~~~~~~~~~~~~~~~

.. cpp:struct:: lattice::Octonion

    Eight-component algebraic entity used as capability tokens.

    .. cpp:function:: static constexpr Octonion from_bytes(const std::array<std::uint8_t, 32> &bytes) noexcept

      Convert raw bytes into octonion representation.

Post-Quantum Cryptography
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. cpp:struct:: pqcrypto::KeyPair

    ML-KEM key pair for quantum-resistant key establishment.

.. cpp:function:: KeyPair pqcrypto::generate_keypair() noexcept

    Generate a new Kyber512 key pair.

.. cpp:function:: std::array<std::uint8_t, 32> pqcrypto::compute_shared_secret(const KeyPair &local, const KeyPair &peer) noexcept

    Derive shared secret via KEM encapsulation/decapsulation.

Implementation Status
---------------------

**Completed**
    ✅ Mathematical foundations (octonion algebra)
    ✅ Post-quantum cryptography (ML-KEM/Kyber)  
    ✅ Lattice-based IPC with encryption
    ✅ DAG-based scheduling infrastructure
    ✅ Service management and resurrection
    ✅ Comprehensive test suite

**In Progress** 🔄 Complete user-mode server implementations
    🔄 Hardware abstraction layer
    🔄 MIPS32 port alongside x86-64

**Planned**
    📋 STREAMS integration for modular I/O
    📋 Rump kernel integration for drivers
    📋 WebAssembly userland support

Educational Applications
------------------------

XINIM serves as a modern teaching platform demonstrating:

* **Classical OS Concepts**: Through MINIX-compatible interfaces
* **Modern Security**: Post-quantum cryptography in practice  
* **Advanced Mathematics**: Applied algebra in system design
* **Contemporary C++**: Modern language features in systems programming

The codebase maintains pedagogical clarity while showcasing cutting-edge
techniques in operating system research and implementation.

For complete API documentation, build the project and see the generated
documentation in ``docs/sphinx/html/index.html``.