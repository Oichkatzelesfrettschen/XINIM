XINIM Documentation
===================

Welcome to the XINIM documentation. This guide outlines the components of the system and how to navigate the reference material.

The documentation is divided into several sections:

* **Architecture Overview** describes the layered microkernel design.
* **Compiler Driver** documents the bespoke ``minix_cmd_cc`` front-end.
* **Lattice IPC Subsystem** provides details about the fast capability-based message passing layer.
* **Remote Channel Setup** demonstrates establishing cross-node links using the network driver.
* **Post-Quantum Cryptography** explains the experimental lattice-based key exchange API.
* **Pre-commit Hook** describes how to automatically format code before committing.
* **API Reference** contains the complete Doxygen-generated API documentation.

.. toctree::
    :maxdepth: 2
    :caption: Contents

    api
    architecture
    compiler_driver
    hypercomplex
    lattice_ipc
    libsodium_tests
    networking
    pq_crypto
    scheduler
    service
    service_manager
    wait_graph
    precommit
    process_control
    wini