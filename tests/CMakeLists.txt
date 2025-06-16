cmake_minimum_required(VERSION 3.16)
project(XINIMUnitTests LANGUAGES CXX)

# Enforce C++23 for all targets
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(Threads REQUIRED)

function(add_lattice_test target source)
  add_executable(${target}
    ${source}
    ${CMAKE_SOURCE_DIR}/kernel/lattice_ipc.cpp
    ${CMAKE_SOURCE_DIR}/kernel/net_driver.cpp
    ${CMAKE_SOURCE_DIR}/kernel/pqcrypto.cpp
    ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
    ${CMAKE_SOURCE_DIR}/kernel/service.cpp
    ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
    ${CMAKE_SOURCE_DIR}/kernel/table.cpp
    task_stubs.cpp)
  target_include_directories(${target} PUBLIC
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/h
    ${ARGN})
  target_compile_definitions(${target} PRIVATE EXTERN=extern)
  target_link_libraries(${target} PRIVATE pqcrypto Threads::Threads)
  add_test(NAME ${target} COMMAND ${target})
endfunction()

# -----------------------------------------------------------------------------
# minix_test_lib
# -----------------------------------------------------------------------------
add_executable(minix_test_lib
  test_lib.cpp
  ${CMAKE_SOURCE_DIR}/lib/strlen.cpp
  ${CMAKE_SOURCE_DIR}/lib/strcmp.cpp
  ${CMAKE_SOURCE_DIR}/lib/rand.cpp
)
target_include_directories(minix_test_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/h
)
target_compile_options(minix_test_lib PRIVATE
  -fno-builtin
)
add_test(NAME minix_test_lib COMMAND minix_test_lib)

# -----------------------------------------------------------------------------
# minix_test_syscall
# -----------------------------------------------------------------------------
add_executable(minix_test_syscall
  test_syscall.cpp
)
target_include_directories(minix_test_syscall PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/h
)
add_test(NAME minix_test_syscall COMMAND minix_test_syscall)

# -----------------------------------------------------------------------------
# minix_test_streams
# -----------------------------------------------------------------------------
add_executable(minix_test_streams
  test_streams.cpp
)
target_link_libraries(minix_test_streams PRIVATE minix_libc)
target_include_directories(minix_test_streams PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/h
)
set_target_properties(minix_test_streams PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_test(NAME minix_test_streams COMMAND minix_test_streams)
set_tests_properties(minix_test_streams PROPERTIES DISABLED TRUE)

# -----------------------------------------------------------------------------
# minix_test_stream_foundation
# -----------------------------------------------------------------------------
add_executable(minix_test_stream_foundation
  test_stream_foundation.cpp
  ${CMAKE_SOURCE_DIR}/lib/io/src/file_stream.cpp
  ${CMAKE_SOURCE_DIR}/lib/io/src/file_operations.cpp
  ${CMAKE_SOURCE_DIR}/lib/io/src/standard_streams.cpp
)
target_include_directories(minix_test_stream_foundation PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(minix_test_stream_foundation PROPERTIES
  EXCLUDE_FROM_ALL TRUE
)
add_test(NAME minix_test_stream_foundation COMMAND minix_test_stream_foundation)
set_tests_properties(minix_test_stream_foundation PROPERTIES DISABLED TRUE)

# -----------------------------------------------------------------------------
# minix_test_memory_stream
# -----------------------------------------------------------------------------
add_executable(minix_test_memory_stream
  test_memory_stream.cpp
  ${CMAKE_SOURCE_DIR}/lib/io/src/memory_stream.cpp
)
target_include_directories(minix_test_memory_stream PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_memory_stream COMMAND minix_test_memory_stream)

# -----------------------------------------------------------------------------
# minix_test_fastpath
# -----------------------------------------------------------------------------
add_executable(minix_test_fastpath
  test_fastpath.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wormhole.cpp
  ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
  ${CMAKE_SOURCE_DIR}/kernel/service.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_fastpath PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
add_test(NAME minix_test_fastpath COMMAND minix_test_fastpath)

# -----------------------------------------------------------------------------
# minix_test_fastpath_fallback
# -----------------------------------------------------------------------------
add_executable(minix_test_fastpath_fallback
  test_fastpath_fallback.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wormhole.cpp
  ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_fastpath_fallback PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
)
add_test(NAME minix_test_fastpath_fallback COMMAND minix_test_fastpath_fallback)

# -----------------------------------------------------------------------------
# minix_test_scheduler
# -----------------------------------------------------------------------------
add_executable(minix_test_scheduler
  test_scheduler.cpp
  ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
  ${CMAKE_SOURCE_DIR}/kernel/service.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_scheduler PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
  ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_scheduler COMMAND minix_test_scheduler)

# -----------------------------------------------------------------------------
# minix_test_scheduler_deadlock
# -----------------------------------------------------------------------------
add_executable(minix_test_scheduler_deadlock
  test_scheduler_deadlock.cpp
  ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
  ${CMAKE_SOURCE_DIR}/kernel/service.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_scheduler_deadlock PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
  ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_scheduler_deadlock COMMAND minix_test_scheduler_deadlock)

# -----------------------------------------------------------------------------
# minix_test_scheduler_edge
# -----------------------------------------------------------------------------
add_executable(minix_test_scheduler_edge
  test_scheduler_edge.cpp
  ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
  ${CMAKE_SOURCE_DIR}/kernel/service.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_scheduler_edge PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
  ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_scheduler_edge COMMAND minix_test_scheduler_edge)

# -----------------------------------------------------------------------------
# minix_test_wait_graph
# -----------------------------------------------------------------------------
add_executable(minix_test_wait_graph
  test_wait_graph.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_wait_graph PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
  ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_wait_graph COMMAND minix_test_wait_graph)

# -----------------------------------------------------------------------------
# minix_test_fastpath_preconditions
# -----------------------------------------------------------------------------
add_executable(minix_test_fastpath_preconditions
  test_fastpath_preconditions.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wormhole.cpp
  ${CMAKE_SOURCE_DIR}/kernel/schedule.cpp
  ${CMAKE_SOURCE_DIR}/kernel/service.cpp
  ${CMAKE_SOURCE_DIR}/kernel/wait_graph.cpp
)
target_include_directories(minix_test_fastpath_preconditions PUBLIC
  ${CMAKE_SOURCE_DIR}/kernel
  ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_fastpath_preconditions COMMAND minix_test_fastpath_preconditions)

# -----------------------------------------------------------------------------
# minix_test_semantic_region
# -----------------------------------------------------------------------------
add_executable(minix_test_semantic_region
  test_semantic_region.cpp
)
target_include_directories(minix_test_semantic_region PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME minix_test_semantic_region COMMAND minix_test_semantic_region)

# -----------------------------------------------------------------------------
# minix_test_lattice
# -----------------------------------------------------------------------------
add_lattice_test(minix_test_lattice test_lattice.cpp)

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
# Crypto subdirectory
# -----------------------------------------------------------------------------
add_subdirectory(crypto)
