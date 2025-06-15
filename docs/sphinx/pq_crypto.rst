Post-Quantum Cryptography
=========================

XINIM includes an experimental post-quantum cryptography layer. It demonstrates
lattice-based key exchange using a minimal API defined in
:file:`include/pqcrypto.hpp`.

.. doxygenfile:: pqcrypto.hpp
   :project: XINIM

.. doxygenstruct:: pqcrypto::KeyPair
   :project: XINIM

.. doxygenfunction:: pqcrypto::generate_keypair
   :project: XINIM

.. doxygenfunction:: pqcrypto::compute_shared_secret
   :project: XINIM

``pqcrypto::compute_shared_secret`` derives a 32-byte session secret by XORing
the remote party's public key with the caller's secret key.  The helper is
implemented in ``crypto/pqcrypto_shared.cpp`` and mirrors the kernel side
exchange primitive used for lattice IPC setup.
