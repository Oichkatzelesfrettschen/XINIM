add_executable(minix_test_kyber test_kyber.cpp)

set_target_properties(minix_test_kyber PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON)

target_include_directories(minix_test_kyber PUBLIC
    "${CMAKE_SOURCE_DIR}/crypto")

target_link_libraries(minix_test_kyber PRIVATE pqcrypto)

add_test(NAME minix_test_kyber COMMAND minix_test_kyber)
