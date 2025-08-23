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

# Determine repository root to allow standalone inclusion
get_filename_component(REPO_ROOT ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM libsodium)

if(SODIUM_FOUND)
  # libsodium detected via pkg-config; use provided variables
else()
  message(WARNING "libsodium not found; building with local stubs")
  add_library(sodium_stub STATIC ${REPO_ROOT}/tests/sodium_stub.cpp)
  list(REMOVE_ITEM KYBER_IMPL_SOURCES kyber_impl/randombytes.c)
  list(APPEND KYBER_IMPL_SOURCES ${REPO_ROOT}/tests/randombytes_stub.c)
  set(SODIUM_LIBRARIES sodium_stub)
  set(SODIUM_INCLUDE_DIRS ${REPO_ROOT}/tests)
endif()

add_library(pqcrypto STATIC kyber.cpp pqcrypto_shared.cpp ${KYBER_IMPL_SOURCES}
    ${REPO_ROOT}/lib/abort.cpp)

set_target_properties(pqcrypto PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON)

target_compile_definitions(pqcrypto PRIVATE KYBER_K=2)

target_compile_features(pqcrypto PRIVATE c_std_11)

# expose header directory
target_include_directories(pqcrypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/kyber_impl
    ${REPO_ROOT}/include)

target_link_libraries(pqcrypto
    PUBLIC OpenSSL::Crypto ${SODIUM_LIBRARIES}
    PRIVATE xinim_common)
target_include_directories(pqcrypto PRIVATE ${SODIUM_INCLUDE_DIRS})
