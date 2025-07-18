cmake_minimum_required(VERSION 3.16)
project(XINIMUnitTests LANGUAGES CXX)

# Enforce modern C++
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Recommended build flags
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto -march=x86-64-v1 -mtune=generic")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
add_compile_options(-Wall -Wextra -Wpedantic)

enable_testing()
find_package(Threads REQUIRED)

# Common include directories
set(XINIM_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/h
    ${CMAKE_SOURCE_DIR}/kernel
)

# Common sources for Lattice IPC tests
set(LATTICE_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/kernel/lattice_ipc.cpp
    ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
    ${CMAKE_SOURCE_DIR}/kernel/pqcrypto.cpp
    ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
    ${CMAKE_SOURCE_DIR}/kernel/service.cpp
    ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
    ${CMAKE_SOURCE_DIR}/kernel/table.cpp
    task_stubs.cpp
)

# Helper macro for Lattice IPC tests
macro(add_lattice_test name test_src)
    add_executable(${name}
        ${test_src}
        ${LATTICE_COMMON_SOURCES}
    )
    target_include_directories(${name} PRIVATE
        ${XINIM_INCLUDES}
        ${CMAKE_SOURCE_DIR}/crypto
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_compile_definitions(${name} PRIVATE EXTERN=extern)
    target_link_libraries(${name} PRIVATE pqcrypto Threads::Threads)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

# -----------------------------------------------------------------------------
# Core library tests
# -----------------------------------------------------------------------------
add_executable(minix_test_lib
    test_lib.cpp
    ${CMAKE_SOURCE_DIR}/lib/strlen.cpp
    ${CMAKE_SOURCE_DIR}/lib/strcmp.cpp
    ${CMAKE_SOURCE_DIR}/lib/rand.cpp
)
target_include_directories(minix_test_lib PRIVATE ${XINIM_INCLUDES})
target_compile_options(minix_test_lib PRIVATE -fno-builtin)
add_test(NAME minix_test_lib COMMAND minix_test_lib)

add_executable(minix_test_syscall
    test_syscall.cpp
)
target_include_directories(minix_test_syscall PRIVATE ${XINIM_INCLUDES})
add_test(NAME minix_test_syscall COMMAND minix_test_syscall)

add_executable(minix_test_memory_stream
    test_memory_stream.cpp
    ${CMAKE_SOURCE_DIR}/lib/io/src/memory_stream.cpp
)
target_include_directories(minix_test_memory_stream PRIVATE ${XINIM_INCLUDES})
add_test(NAME minix_test_memory_stream COMMAND minix_test_memory_stream)

# -----------------------------------------------------------------------------
# Fastpath & scheduler tests
# -----------------------------------------------------------------------------
set(FASTPATH_SOURCES
    ${CMAKE_SOURCE_DIR}/kernel/wormhole.cpp
    ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
    ${CMAKE_SOURCE_DIR}/kernel/service.cpp
    ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)

add_executable(minix_test_fastpath
    test_fastpath.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_fastpath PRIVATE ${CMAKE_SOURCE_DIR}/kernel)
add_test(NAME minix_test_fastpath COMMAND minix_test_fastpath)

add_executable(minix_test_fastpath_fallback
    test_fastpath_fallback.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_fastpath_fallback PRIVATE ${CMAKE_SOURCE_DIR}/kernel)
add_test(NAME minix_test_fastpath_fallback COMMAND minix_test_fastpath_fallback)

add_executable(minix_test_scheduler
    test_scheduler.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_scheduler PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_scheduler COMMAND minix_test_scheduler)

add_executable(minix_test_scheduler_deadlock
    test_scheduler_deadlock.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_scheduler_deadlock PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_scheduler_deadlock COMMAND minix_test_scheduler_deadlock)

add_executable(minix_test_wait_graph
    test_wait_graph.cpp
    ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_wait_graph PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_wait_graph COMMAND minix_test_wait_graph)

add_executable(minix_test_service_contract
    test_service_contract.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_service_contract PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_service_contract COMMAND minix_test_service_contract)

add_executable(minix_test_service_manager_dag
    test_service_manager_dag.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_service_manager_dag PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_service_manager_dag COMMAND minix_test_service_manager_dag)

add_executable(minix_test_service_serialization
    test_service_serialization.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_service_serialization PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_service_serialization COMMAND minix_test_service_serialization)

add_executable(minix_test_service_manager_updates
    test_service_manager_updates.cpp
    ${FASTPATH_SOURCES}
)
target_include_directories(minix_test_service_manager_updates PRIVATE
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_service_manager_updates COMMAND minix_test_service_manager_updates)

# -----------------------------------------------------------------------------
# svcctl command tests
# -----------------------------------------------------------------------------
add_executable(minix_test_svcctl
    test_svcctl.cpp
    ${LATTICE_COMMON_SOURCES}
    ${CMAKE_SOURCE_DIR}/commands/svcctl.cpp
)
target_include_directories(minix_test_svcctl PRIVATE
    ${XINIM_INCLUDES}
    ${CMAKE_SOURCE_DIR}/crypto
    ${CMAKE_SOURCE_DIR}/commands
)
target_compile_definitions(minix_test_svcctl PRIVATE EXTERN=extern SVCCTL_NO_MAIN SVCCTL_NO_WAIT)
target_link_libraries(minix_test_svcctl PRIVATE pqcrypto Threads::Threads)
add_test(NAME minix_test_svcctl COMMAND minix_test_svcctl)

# -----------------------------------------------------------------------------
# Lattice IPC tests
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_lattice_send_recv test_lattice_send_recv.cpp)
add_lattice_test(minix_test_lattice_blocking test_lattice_blocking.cpp)

# -----------------------------------------------------------------------------
# minix_test_lattice_ipc
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_lattice_ipc test_lattice_ipc.cpp ${CMAKE_SOURCE_DIR}/crypto)
target_compile_definitions(minix_test_lattice_ipc PRIVATE EXTERN=extern)

# -----------------------------------------------------------------------------
# minix_test_lattice_network
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_lattice_network test_lattice_network.cpp ${CMAKE_SOURCE_DIR}/crypto)
target_compile_definitions(minix_test_lattice_network PRIVATE EXTERN=extern)

# -----------------------------------------------------------------------------
# minix_test_lattice_network_encrypted
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_lattice_network_encrypted test_lattice_network_encrypted.cpp ${CMAKE_SOURCE_DIR}/crypto)
target_compile_definitions(minix_test_lattice_network_encrypted PRIVATE EXTERN=extern)

# -----------------------------------------------------------------------------
# minix_test_lattice_ipv6
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_lattice_ipv6 test_lattice_ipv6.cpp ${CMAKE_SOURCE_DIR}/crypto)
target_compile_definitions(minix_test_lattice_ipv6 PRIVATE EXTERN=extern)

# -----------------------------------------------------------------------------
# minix_test_net_two_node
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_net_two_node test_net_two_node.cpp ${CMAKE_SOURCE_DIR}/crypto)
target_compile_definitions(minix_test_net_two_node PRIVATE EXTERN=extern)

# -----------------------------------------------------------------------------
# minix_test_poll_network
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_poll_network test_poll_network.cpp ${CMAKE_SOURCE_DIR}/crypto)
target_compile_definitions(minix_test_poll_network PRIVATE EXTERN=extern)

# -----------------------------------------------------------------------------
# minix_test_net_driver
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver
  test_net_driver.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver COMMAND minix_test_net_driver)

# -----------------------------------------------------------------------------
# minix_test_net_driver_overflow
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_overflow
  test_net_driver_overflow.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_overflow PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_overflow PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_overflow COMMAND minix_test_net_driver_overflow)

# -----------------------------------------------------------------------------
# minix_test_net_driver_drop_newest
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_drop_newest
  test_net_driver_drop_newest.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_drop_newest PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_drop_newest PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_drop_newest COMMAND minix_test_net_driver_drop_newest)
# minix_test_net_driver_tcp
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_tcp
  test_net_driver_tcp.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_tcp PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_tcp PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_tcp COMMAND minix_test_net_driver_tcp)

# -----------------------------------------------------------------------------
# minix_test_net_driver_reconnect
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_reconnect
  test_net_driver_reconnect.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_reconnect PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_reconnect PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_reconnect COMMAND minix_test_net_driver_reconnect)

# -----------------------------------------------------------------------------
# minix_test_net_driver_loopback
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_loopback
  test_net_driver_loopback.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_loopback PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_loopback PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_loopback COMMAND minix_test_net_driver_loopback)

# -----------------------------------------------------------------------------
# minix_test_net_driver_ipv6
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_ipv6
  test_net_driver_ipv6.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_ipv6 PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_ipv6 PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_ipv6 COMMAND minix_test_net_driver_ipv6)

# -----------------------------------------------------------------------------
# minix_test_net_driver_concurrency
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_concurrency
  test_net_driver_concurrency.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_concurrency PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_concurrency PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_concurrency COMMAND minix_test_net_driver_concurrency)

# -----------------------------------------------------------------------------
# minix_test_net_driver_persistent_id
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_persistent_id
  test_net_driver_persistent_id.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_persistent_id PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_persistent_id PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_persistent_id COMMAND minix_test_net_driver_persistent_id)

# -----------------------------------------------------------------------------

# minix_test_net_driver_socket_failure
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_socket_failure
  test_net_driver_socket_failure.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_socket_failure PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_socket_failure PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_socket_failure COMMAND minix_test_net_driver_socket_failure)

# minix_test_net_driver_unpriv_id
# -----------------------------------------------------------------------------
add_executable(minix_test_net_driver_unpriv_id
  test_net_driver_unpriv_id.cpp
  ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
)
target_include_directories(minix_test_net_driver_unpriv_id PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
target_link_libraries(minix_test_net_driver_unpriv_id PRIVATE Threads::Threads)
add_test(NAME minix_test_net_driver_unpriv_id COMMAND minix_test_net_driver_unpriv_id)

# -----------------------------------------------------------------------------
# minix_test_hypercomplex
# -----------------------------------------------------------------------------
add_executable(minix_test_hypercomplex
    test_hypercomplex.cpp
)
target_include_directories(minix_test_hypercomplex PRIVATE ${CMAKE_SOURCE_DIR}/kernel)
add_test(NAME minix_test_hypercomplex COMMAND minix_test_hypercomplex)

# -----------------------------------------------------------------------------
# Crypto library
# -----------------------------------------------------------------------------
add_subdirectory(crypto)
