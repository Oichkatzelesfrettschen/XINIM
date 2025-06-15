Libsodium Testing Setup
=======================

The unit tests for the network driver rely on the libsodium cryptography
library. Install the development headers and Python bindings before
building the tests.

.. code-block:: bash

   sudo apt-get update
   sudo apt-get install libsodium-dev
   pip install pynacl
   npm install libsodium-wrappers

After installing these packages ``cmake`` will detect libsodium and
build the network tests automatically.
