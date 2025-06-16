XINIM Documentation
===================

Welcome to the XINIM documentation. This guide outlines the components of the system and how to navigate the reference material.

The documentation is divided into several sections:

* **Lattice IPC Subsystem** provides details about the fast capability-based message passing layer.
* **Remote Channel Setup** demonstrates establishing cross-node links using the network driver.
* **Post-Quantum Cryptography** explains the experimental lattice-based key exchange API.
* **Pre-commit Hook** describes how to automatically format code before committing.
* **API Reference** contains the complete Doxygen-generated API documentation.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   lattice_ipc
   lattice_ipc#remote-channel-setup
   pq_crypto
   scheduler
   service
   service_manager
   precommit
   networking
   libsodium_tests
   api
