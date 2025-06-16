Libsodium Testing Setup
=======================

The unit tests for the network driver rely on the libsodium cryptography
library. Install the development headers and language bindings before
building the tests.

Quick install
-------------

Use the repository's ``setup.sh`` script to install the required
packages on Ubuntu based systems:

.. code-block:: bash

   ./setup.sh

The script installs ``libsodium-dev`` and ``libsodium23`` which package
libsodium **1.0.18** at the time of writing.

Manual steps
------------

Manual installation is straightforward when the script cannot be used.

.. code-block:: bash

   sudo apt-get update
   sudo apt-get install libsodium-dev libsodium23
   pip install "pynacl==1.5.0"
   npm install "libsodium-wrappers@0.7.9"

Optional build flags
--------------------

CMake locates libsodium through ``pkg-config``.  Set ``PKG_CONFIG_PATH``
or pass ``-DLibsodium_DIR=/path/to/lib/cmake`` when configuring if the
library resides in a custom location.  The variables
``LIBSODIUM_LIBRARIES`` and ``LIBSODIUM_INCLUDE_DIRS`` can also be
overridden to point to a specific installation.

After installing the dependencies ``cmake`` detects libsodium and
builds the ``minix_test_lattice_network`` target automatically.
