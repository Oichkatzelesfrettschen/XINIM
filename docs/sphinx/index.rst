XINIM Documentation
===================

Welcome to the XINIM documentation. This guide outlines the components of the
system and how to navigate the reference material.

The documentation is divided into several sections:

* **Architecture Overview** describes the layered microkernel design.
* **Compiler Driver** documents the bespoke ``minix_cmd_cc`` front‑end.
* **Lattice IPC Subsystem** details the fast capability‑based message passing layer.
* **Libsodium Testing** outlines the cryptography test setup.
* **Pre‑commit Hook** describes how to automatically format code before committing.
* **API Reference** contains the complete Doxygen‑generated API documentation.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   api
   architecture
   compiler_driver
   lattice_ipc
   libsodium_tests
   precommit

