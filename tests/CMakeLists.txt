# CMake configuration for XINIM Unit Tests
#
# This file defines the build and test configuration for the XINIM unit tests,
# harmonizing legacy and modern test setups for the XINIM kernel. It enforces
# modern C++23 standards, optimizes build flags, and organizes tests for core
# libraries, fastpath, scheduler, Lattice IPC, network drivers, and sorting
# utilities. The configuration resolves inconsistencies, enhances modularity, and
# provides clear documentation for developers.

cmake_minimum_required(VERSION 3.16)
project(XINIMUnitTests LANGUAGES C CXX)

# Root of the repository (parent directory of this tests/ folder)
set(PROJECT_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# Enforce C++23 for all targets
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Recommended build flags for different configurations
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto -march=x86-64-v1 -mtune=generic")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Enable testing
enable_testing()
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)
include(FetchContent)

# -----------------------------------------------------------------------------
# Property-based testing framework: RapidCheck
# -----------------------------------------------------------------------------
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
)
FetchContent_MakeAvailable(rapidcheck)

# Common include directories
set(XINIM_INCLUDES
    ${PROJECT_ROOT}/include
    ${PROJECT_ROOT}/h
    ${PROJECT_ROOT}/kernel
    ${PROJECT_ROOT}/crypto
    ${PROJECT_ROOT}/commands
)

# Common sources for Lattice IPC tests
set(LATTICE_COMMON_SOURCES
    ${PROJECT_ROOT}/kernel/lattice_ipc.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
    ${PROJECT_ROOT}/kernel/pqcrypto.cpp
    ${PROJECT_ROOT}/kernel/schedule.cpp
    ${PROJECT_ROOT}/kernel/service.cpp
    ${PROJECT_ROOT}/kernel/wait_graph.cpp
    ${PROJECT_ROOT}/kernel/table.cpp
    task_stubs.cpp
)

# Common sources for fastpath and scheduler tests
set(FASTPATH_SOURCES
    ${PROJECT_ROOT}/kernel/wormhole.cpp
    ${PROJECT_ROOT}/kernel/schedule.cpp
    ${PROJECT_ROOT}/kernel/service.cpp
    ${PROJECT_ROOT}/kernel/wait_graph.cpp
)

# Helper macro for Lattice IPC tests
function(add_lattice_test target source)
    add_executable(${target}
        ${source}
        ${LATTICE_COMMON_SOURCES}
        ${ARGN}
    )
    target_include_directories(${target} PRIVATE
        ${XINIM_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_definitions(${target} PRIVATE EXTERN=extern)
    target_link_libraries(${target} PRIVATE pqcrypto Threads::Threads nlohmann_json::nlohmann_json xinim_common)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# Helper macro for fastpath and scheduler tests
function(add_fastpath_test target source)
    add_executable(${target}
        ${source}
        ${FASTPATH_SOURCES}
    )
    target_include_directories(${target} PRIVATE
        ${PROJECT_ROOT}/kernel
        ${PROJECT_ROOT}/include
    )
    target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json xinim_common)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# -----------------------------------------------------------------------------
# Core Library Tests
# -----------------------------------------------------------------------------
add_executable(minix_test_lib
    test_lib.cpp
    ${PROJECT_ROOT}/lib/strlen.cpp
    ${PROJECT_ROOT}/lib/strcmp.cpp
    ${PROJECT_ROOT}/lib/rand.cpp
)
target_include_directories(minix_test_lib PRIVATE ${XINIM_INCLUDES})
target_compile_options(minix_test_lib PRIVATE -fno-builtin)
target_link_libraries(minix_test_lib PRIVATE xinim_common)
add_test(NAME minix_test_lib COMMAND minix_test_lib)

add_executable(minix_test_syscall
    test_syscall.cpp
)
target_include_directories(minix_test_syscall PRIVATE ${XINIM_INCLUDES})
target_link_libraries(minix_test_syscall PRIVATE xinim_common)
add_test(NAME minix_test_syscall COMMAND minix_test_syscall)

add_executable(minix_test_memory_stream
    test_memory_stream.cpp
    ${PROJECT_ROOT}/lib/io/src/memory_stream.cpp
)
target_include_directories(minix_test_memory_stream PRIVATE ${XINIM_INCLUDES})
target_link_libraries(minix_test_memory_stream PRIVATE xinim_common)
add_test(NAME minix_test_memory_stream COMMAND minix_test_memory_stream)

# -----------------------------------------------------------------------------
# File System Tests
# -----------------------------------------------------------------------------
add_executable(minix_test_fs_compat
    ${PROJECT_ROOT}/fs/tests/compat_tests.cpp
    ${PROJECT_ROOT}/fs/compat.cpp
    ${PROJECT_ROOT}/lib/safe_alloc.cpp
)
target_include_directories(minix_test_fs_compat PRIVATE ${XINIM_INCLUDES} ${PROJECT_ROOT}/fs)
target_link_libraries(minix_test_fs_compat PRIVATE xinim_common)
add_test(NAME minix_test_fs_compat COMMAND minix_test_fs_compat)

# Optional stream tests (disabled by default)
add_executable(minix_test_streams
    test_streams.cpp
)
target_link_libraries(minix_test_streams PRIVATE minix_libc xinim_common)
target_include_directories(minix_test_streams PRIVATE ${XINIM_INCLUDES})
set_target_properties(minix_test_streams PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_test(NAME minix_test_streams COMMAND minix_test_streams)
set_tests_properties(minix_test_streams PROPERTIES DISABLED TRUE)

add_executable(minix_test_stream_foundation
    test_stream_foundation.cpp
    ${PROJECT_ROOT}/lib/io/src/file_stream.cpp
    ${PROJECT_ROOT}/lib/io/src/file_operations.cpp
    ${PROJECT_ROOT}/lib/io/src/standard_streams.cpp
)
target_include_directories(minix_test_stream_foundation PRIVATE ${PROJECT_ROOT}/include)
set_target_properties(minix_test_stream_foundation PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_test(NAME minix_test_stream_foundation COMMAND minix_test_stream_foundation)
set_tests_properties(minix_test_stream_foundation PROPERTIES DISABLED TRUE)
target_link_libraries(minix_test_stream_foundation PRIVATE xinim_common)

# -----------------------------------------------------------------------------
# Fastpath and Scheduler Tests
# -----------------------------------------------------------------------------
add_fastpath_test(minix_test_fastpath test_fastpath.cpp)
add_fastpath_test(minix_test_fastpath_fallback test_fastpath_fallback.cpp)
add_fastpath_test(minix_test_fastpath_preconditions test_fastpath_preconditions.cpp)
add_fastpath_test(minix_test_fastpath_cache_performance test_fastpath_cache_performance.cpp)
add_fastpath_test(minix_test_scheduler test_scheduler.cpp)
add_fastpath_test(minix_test_scheduler_deadlock test_scheduler_deadlock.cpp)
add_fastpath_test(minix_test_scheduler_edge test_scheduler_edge.cpp)

add_executable(minix_test_wait_graph
    test_wait_graph.cpp
    ${PROJECT_ROOT}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_wait_graph PRIVATE ${PROJECT_ROOT}/kernel ${PROJECT_ROOT}/include)
target_link_libraries(minix_test_wait_graph PRIVATE xinim_common)
add_test(NAME minix_test_wait_graph COMMAND minix_test_wait_graph)

add_fastpath_test(minix_test_service_contract test_service_contract.cpp)
add_fastpath_test(minix_test_service_manager_dag test_service_manager_dag.cpp)
add_fastpath_test(minix_test_service_serialization test_service_serialization.cpp)
add_fastpath_test(minix_test_service_manager_updates test_service_manager_updates.cpp)

# -----------------------------------------------------------------------------
# svcctl Command Tests
# -----------------------------------------------------------------------------
add_executable(minix_test_svcctl
    test_svcctl.cpp
    ${LATTICE_COMMON_SOURCES}
    ${PROJECT_ROOT}/commands/svcctl.cpp
)
target_include_directories(minix_test_svcctl PRIVATE ${XINIM_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(minix_test_svcctl PRIVATE EXTERN=extern SVCCTL_NO_MAIN SVCCTL_NO_WAIT)
target_link_libraries(minix_test_svcctl PRIVATE pqcrypto Threads::Threads nlohmann_json::nlohmann_json xinim_common)
add_test(NAME minix_test_svcctl COMMAND minix_test_svcctl)

# -----------------------------------------------------------------------------
# Lattice IPC Tests
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_lattice test_lattice.cpp)
add_lattice_test(minix_test_lattice_send_recv test_lattice_send_recv.cpp)
add_lattice_test(minix_test_lattice_blocking test_lattice_blocking.cpp)
add_lattice_test(minix_test_lattice_ipc test_lattice_ipc.cpp ${PROJECT_ROOT}/crypto)
add_lattice_test(minix_test_lattice_network test_lattice_network.cpp ${PROJECT_ROOT}/crypto)
add_lattice_test(minix_test_lattice_network_encrypted test_lattice_network_encrypted.cpp ${PROJECT_ROOT}/crypto)
add_lattice_test(minix_test_lattice_ipv6 test_lattice_ipv6.cpp ${PROJECT_ROOT}/crypto)
add_lattice_test(minix_test_net_two_node test_net_two_node.cpp ${PROJECT_ROOT}/crypto)
add_lattice_test(minix_test_poll_network test_poll_network.cpp ${PROJECT_ROOT}/crypto)

# -----------------------------------------------------------------------------
# Network Driver Tests
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver
    test_net_driver.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver COMMAND minix_test_net_driver)

add_executable(minix_test_net_driver_overflow
    test_net_driver_overflow.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_overflow PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_overflow PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_overflow COMMAND minix_test_net_driver_overflow)

add_executable(minix_test_net_driver_drop
    test_net_driver_drop.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_drop PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_drop PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_drop COMMAND minix_test_net_driver_drop)

add_executable(minix_test_net_driver_drop_newest
    test_net_driver_drop_newest.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_drop_newest PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_drop_newest PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_drop_newest COMMAND minix_test_net_driver_drop_newest)

add_executable(minix_test_net_driver_tcp
    test_net_driver_tcp.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_tcp PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_tcp PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_tcp COMMAND minix_test_net_driver_tcp)

add_executable(minix_test_net_driver_reconnect
    test_net_driver_reconnect.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_reconnect PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_reconnect PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_reconnect COMMAND minix_test_net_driver_reconnect)

add_executable(minix_test_net_driver_loopback
    test_net_driver_loopback.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_loopback PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_loopback PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_loopback COMMAND minix_test_net_driver_loopback)

add_executable(minix_test_net_driver_ipv6
    test_net_driver_ipv6.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_ipv6 PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_ipv6 PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_ipv6 COMMAND minix_test_net_driver_ipv6)

add_executable(minix_test_net_driver_concurrency
    test_net_driver_concurrency.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_concurrency PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_concurrency PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_concurrency COMMAND minix_test_net_driver_concurrency)

add_executable(minix_test_net_driver_persistent_id
    test_net_driver_persistent_id.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_persistent_id PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_persistent_id PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_persistent_id COMMAND minix_test_net_driver_persistent_id)

add_executable(minix_test_net_driver_socket_failure
    test_net_driver_socket_failure.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_socket_failure PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_socket_failure PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_socket_failure COMMAND minix_test_net_driver_socket_failure)

add_executable(minix_test_net_driver_unpriv_id
    test_net_driver_unpriv_id.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_unpriv_id PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_unpriv_id PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_unpriv_id COMMAND minix_test_net_driver_unpriv_id)

add_executable(minix_test_net_driver_autodetect
    test_net_driver_autodetect.cpp
    ${PROJECT_ROOT}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_autodetect PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_net_driver_autodetect PRIVATE Threads::Threads xinim_common)
add_test(NAME minix_test_net_driver_autodetect COMMAND minix_test_net_driver_autodetect)

# -----------------------------------------------------------------------------
# Sort Merge Tests
# -----------------------------------------------------------------------------
add_executable(minix_test_sort_merge
    test_sort_merge.cpp
    ${PROJECT_ROOT}/commands/sort.cpp
)
target_include_directories(minix_test_sort_merge PRIVATE ${PROJECT_ROOT}/commands)
target_compile_definitions(minix_test_sort_merge PRIVATE SORT_UTILITY_NO_MAIN)
target_link_libraries(minix_test_sort_merge PRIVATE xinim_common)
add_test(NAME minix_test_sort_merge COMMAND minix_test_sort_merge)

# -----------------------------------------------------------------------------
# Miscellaneous Tests
# -----------------------------------------------------------------------------
add_executable(minix_test_hypercomplex
    test_hypercomplex.cpp
)
target_include_directories(minix_test_hypercomplex PRIVATE ${PROJECT_ROOT}/kernel)
target_link_libraries(minix_test_hypercomplex PRIVATE xinim_common)
add_test(NAME minix_test_hypercomplex COMMAND minix_test_hypercomplex)

add_executable(minix_test_fat_time
    test_fat_time.cpp
)
target_include_directories(minix_test_fat_time PRIVATE ${PROJECT_ROOT}/include)
target_link_libraries(minix_test_fat_time PRIVATE xinim_common)
add_test(NAME minix_test_fat_time COMMAND minix_test_fat_time)

add_executable(minix_test_semantic_region
    test_semantic_region.cpp
)
target_include_directories(minix_test_semantic_region PRIVATE ${PROJECT_ROOT}/include)
target_link_libraries(minix_test_semantic_region PRIVATE xinim_common)
add_test(NAME minix_test_semantic_region COMMAND minix_test_semantic_region)

# -----------------------------------------------------------------------------
# Property-based Tests
# -----------------------------------------------------------------------------
add_executable(minix_test_strlen_property
    test_strlen_property.cpp
    ${PROJECT_ROOT}/lib/strlen.cpp
)
target_include_directories(minix_test_strlen_property PRIVATE ${XINIM_INCLUDES})
target_link_libraries(minix_test_strlen_property PRIVATE rapidcheck xinim_common)
target_compile_options(minix_test_strlen_property PRIVATE -Wno-error=deprecated-declarations)
add_test(NAME minix_test_strlen_property COMMAND minix_test_strlen_property)

# Crypto library is built by main CMakeLists.txt
# No need to add_subdirectory here as it creates conflicts

# -----------------------------------------------------------------------------
# Recommendations for @copilot TODOs and AI Agent FIXMEs: 
# -----------------------------------------------------------------------------
# - Ensure consistent include paths across all tests by centralizing XINIM_INCLUDES.
# - Consider enabling disabled tests (minix_test_streams, minix_test_stream_foundation)
#   after resolving dependencies or updating test cases.
# - Add version checks for required libraries (e.g., pqcrypto, Threads) to ensure
#   compatibility.
# - Integrate tests into a CI pipeline with artifacts for test reports.
# - Document test purposes and expected outcomes in a separate README or test guide.
# - Validate cross-platform compatibility by testing on different compilers (e.g., GCC, Clang).
