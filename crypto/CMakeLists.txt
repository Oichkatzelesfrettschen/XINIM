set(KYBER_IMPL_SOURCES
    kyber_impl/cbd.c
    kyber_impl/fips202.c
    kyber_impl/indcpa.c
    kyber_impl/kem.c
    kyber_impl/ntt.c
    kyber_impl/poly.c
    kyber_impl/polyvec.c
    kyber_impl/randombytes.c
    kyber_impl/reduce.c
    kyber_impl/symmetric-shake.c
    kyber_impl/verify.c)

add_library(pqcrypto STATIC kyber.cpp pqcrypto_shared.cpp ${KYBER_IMPL_SOURCES})

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)

set_target_properties(pqcrypto PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON)

target_compile_definitions(pqcrypto PRIVATE KYBER_K=2)

target_compile_features(pqcrypto PRIVATE c_std_11)

# expose header directory
target_include_directories(pqcrypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/kyber_impl)

target_link_libraries(pqcrypto PUBLIC OpenSSL::Crypto ${LIBSODIUM_LIBRARIES})
target_include_directories(pqcrypto PRIVATE ${LIBSODIUM_INCLUDE_DIRS})
