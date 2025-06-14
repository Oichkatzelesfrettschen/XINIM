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
