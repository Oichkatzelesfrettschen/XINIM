Post - Quantum Cryptography == == == == == == == == == == == ==
    =

        XINIM includes an experimental post - quantum cryptography layer.It demonstrates lattice -
        based key exchange using the Kyber512 key encapsulation mechanism with a minimal API defined
                in : file :`include /
    pqcrypto.hpp`
        .

        ..doxygenfile::pqcrypto
        .hpp : project : XINIM

        .
        .doxygenstruct::pqcrypto::KeyPair : project : XINIM

        .
        .doxygenfunction::pqcrypto::generate_keypair : project : XINIM

        .
        .doxygenfunction::pqcrypto::compute_shared_secret : project : XINIM

        ..doxygenfunction::pqcrypto::establish_secret : project : XINIM

``pqcrypto::compute_shared_secret`` derives a 32 -
        byte session secret using the Kyber512 encapsulation routine
            .The helper mirrors the kernel side
``pqcrypto::establish_secret`` used during lattice IPC setup.
