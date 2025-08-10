# System Architecture

XINIM's design follows a layered approach that separates mathematical foundations from concrete implementations. This overview summarises the major strata and points to the comprehensive Sphinx documentation for further study.

1. **Governing Mathematical Model** – Defines the security label algebra based on octonions.
1. **Abstract Kernel Contract** – Specifies capability lifecycles, message channels and scheduling invariants.
1. **Algorithmic Realisation** – Supplies concrete data structures, including `lattice::Channel` and `sched::Scheduler`.
1. **C++23 Skeleton** – Provides namespaces, classes and concepts with complete Doxygen comments integrated via Breathe.
1. **Tool Chain** – Employs Meson and CMake with Clang 18, and uses Doxygen to feed Sphinx.

For an exhaustive layer‑by‑layer discussion, consult the [Sphinx architecture guide](sphinx/architecture.rst).
