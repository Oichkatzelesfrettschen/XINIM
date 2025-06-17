add_executable(minix_test_kyber test_kyber.cpp)

set_target_properties(minix_test_kyber PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON)

target_include_directories(minix_test_kyber PUBLIC
    "${CMAKE_SOURCE_DIR}/crypto")

target_link_libraries(minix_test_kyber PRIVATE pqcrypto)

add_test(NAME minix_test_kyber COMMAND minix_test_kyber)

# -----------------------------------------------------------------------------
# minix_test_shared_secret_failure
# -----------------------------------------------------------------------------
add_executable(minix_test_shared_secret_failure test_shared_secret_failure.cpp)

set_target_properties(minix_test_shared_secret_failure PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON)

target_include_directories(minix_test_shared_secret_failure PUBLIC
    "${CMAKE_SOURCE_DIR}/crypto")

target_link_libraries(minix_test_shared_secret_failure PRIVATE pqcrypto)

add_test(NAME minix_test_shared_secret_failure COMMAND minix_test_shared_secret_failure)

# -----------------------------------------------------------------------------
# minix_test_constant_time_equal
# -----------------------------------------------------------------------------
add_executable(minix_test_constant_time_equal test_constant_time_equal.cpp)

set_target_properties(minix_test_constant_time_equal PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON)

target_include_directories(minix_test_constant_time_equal PUBLIC
    "${CMAKE_SOURCE_DIR}/crypto"
    "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(minix_test_constant_time_equal PRIVATE pqcrypto)

add_test(NAME minix_test_constant_time_equal COMMAND minix_test_constant_time_equal)

