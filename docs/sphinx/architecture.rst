Architecture Overview
=====================

This document summarises the layered design of XINIM.  The kernel is
organised into clearly separated components ranging from mathematical
models to user facing libraries.

Layers
------

L0 - Governing Mathematical Model
    Formal definitions of security labels and the capability algebra
    constructed from octonions.

L1 - Abstract Kernel Contract
    Specification of state machines describing capability lifecycles,
    message channels and scheduling invariants.

L2 - Algorithmic Realisation
    Concrete data structures such as :cpp:struct:`lattice::Channel` and
    :cpp:class:`sched::Scheduler` implement the contract.  Scheduling
    relies on a DAG of blocked dependencies.

L3 - C++23 Skeleton
    Namespaces, classes and concepts are provided in header files.  Each
    header is documented using Doxygen comments that integrate with
    Breathe for the Sphinx manual.

L4 - Tool Chain
    Meson and CMake files build both kernel and tests with Clang 18.
    Doxygen generates the API documentation which is consumed by Sphinx.
