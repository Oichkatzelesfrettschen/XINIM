# XINIM - Modern C++23 Post-Quantum Microkernel Operating System
# CMake Build Configuration (Refined for Bare-Metal Baseline)

cmake_minimum_required(VERSION 3.28)

project(XINIM VERSION 1.0.0 LANGUAGES C CXX ASM)

# =============================================================================
# Project Options and Compiler Warnings (from cmake/ modules)
# =============================================================================

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(ProjectOptions)
include(CompilerWarnings)

# Architecture-specific flags applied globally (bare-metal target)
add_compile_options(-march=x86-64 -mtune=generic)
add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

# Limine boot protocol
add_compile_definitions(XINIM_BOOT_LIMINE)

# =============================================================================
# Testing Infrastructure
# =============================================================================

enable_testing()

# =============================================================================
# Include Directories
# =============================================================================

# Note: global include_directories() removed. Includes are set per-target via
# xinim_apply_target_defaults() in cmake/ProjectOptions.cmake and explicit
# target_include_directories() calls in each test target.

# =============================================================================
# Main XINIM Kernel Target (Bare-Metal Baseline)
# =============================================================================

add_executable(xinim
    src/kernel/main.cpp
    src/kernel/kernel.cpp
    src/kernel/system.cpp
    src/kernel/syscall.cpp
    src/kernel/proc.cpp
    src/kernel/schedule.cpp
    src/kernel/service.cpp
    src/kernel/tty.cpp
    src/kernel/panic.cpp
    src/boot/limine/shim.cpp
    src/kernel/klib64.cpp
    src/kernel/heap.cpp
    src/kernel/mpx64_stubs.S
    src/kernel/exception_stubs.cpp
    src/kernel/signal_stubs.cpp
    src/kernel/time_stubs.cpp
    src/kernel/wait_graph.cpp
    src/kernel/lock_manager.cpp
    src/kernel/capability_mutex.cpp
    src/kernel/table.cpp
    src/kernel/clock.cpp
    src/kernel/dmp.cpp
    # idt64.cpp removed: superseded by unified path in interrupts.cpp
    src/kernel/interrupts.cpp
    src/kernel/irq.cpp
    src/kernel/memory.cpp
    src/kernel/paging.cpp
    src/mm/alloc.cpp
    src/kernel/scheduler.cpp
    src/kernel/acpi/acpi.cpp
    src/kernel/time/calibrate.cpp
    src/kernel/timer.cpp
    src/kernel/server_spawn.cpp
    src/hal/x86_64/hal/apic.cpp
    src/hal/x86_64/hal/hpet.cpp
    src/hal/x86_64/hal/ioapic.cpp
    src/kernel/printer.cpp
    src/kernel/wini.cpp
    src/kernel/floppy.cpp
    src/kernel/sys/dispatch.cpp
    src/kernel/arch/x86_64/gdt.cpp
    src/kernel/arch/x86_64/idt.cpp
    src/kernel/arch/x86_64/tss.cpp
    src/kernel/arch/x86_64/stub.cpp
    src/kernel/arch/x86_64/syscall_init.cpp
    src/kernel/arch/x86_64/isr.S
    src/arch/x86_64/gdt_load.S
    src/arch/x86_64/tss_load.S
    src/arch/x86_64/syscall_handler.S
    src/arch/x86_64/context_switch.S
    src/arch/x86_64/interrupts.S
    src/kernel/unified_scheduler.cpp
    src/kernel/process_lifecycle.cpp
    src/kernel/lattice_ipc.cpp
    src/kernel/early/serial_16550.cpp
    src/kernel/bare_metal_stubs.cpp
    src/hal/hal.cpp
)

# Apply project-wide target defaults (C++23, defines, includes)
xinim_apply_target_defaults(xinim)
xinim_set_warnings(xinim)

# Bare-metal: disable hosted C++ runtime assumptions
target_compile_options(xinim PRIVATE -ffreestanding)

# Linker script for bare-metal kernel layout (1MB physical load)
target_link_options(xinim PRIVATE -T ${CMAKE_SOURCE_DIR}/linker.ld)
target_link_options(xinim PRIVATE -nostdlib -static -fno-stack-protector)

# No Conan-managed runtime deps: crypto is vendored, kernel is freestanding.
# target_link_libraries(xinim PRIVATE) -- intentionally empty

# =============================================================================
# Kernel Binary Existence Check
# =============================================================================
# Verifies the xinim kernel binary was produced by the build.

add_test(NAME build_check
    COMMAND ${CMAKE_COMMAND} -E true
)
set_tests_properties(build_check PROPERTIES LABELS "build")

# =============================================================================
# Host-Side Unit Tests (compiled as regular hosted C++23 executables)
# =============================================================================
# These tests run on the build host, not inside the kernel.
# They exercise self-contained kernel data structures that don't require
# bare-metal support (schedulers, wait graphs, sync primitives, etc.).

# Common sources shared by multiple host tests
set(XINIM_TEST_COMMON_SOURCES
    src/kernel/wait_graph.cpp
)

set(XINIM_TEST_SCHED_SOURCES
    ${XINIM_TEST_COMMON_SOURCES}
    src/kernel/schedule.cpp
    src/kernel/service.cpp
)

# Capability mutex needs octonion + lock_manager + schedule + service
set(XINIM_TEST_CAPMUTEX_SOURCES
    ${XINIM_TEST_SCHED_SOURCES}
    src/kernel/capability_mutex.cpp
    src/kernel/lock_manager.cpp
)

# Helper function to configure a host-side test target.
# Host tests compile as regular hosted C++23 (exceptions, RTTI, threads).
# They DO NOT inherit -ffreestanding (that is only for the kernel target).
function(xinim_add_host_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${ARGN})
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_23)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/xinim
        ${CMAKE_SOURCE_DIR}/src/kernel
    )
    target_compile_options(${TEST_NAME} PRIVATE -stdlib=libc++ -pthread)
    target_link_options(${TEST_NAME} PRIVATE -stdlib=libc++ -pthread)
    xinim_set_warnings(${TEST_NAME})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30 LABELS "unit")
endfunction()

# --- Wait Graph Tests ---
xinim_add_host_test(test_wait_graph
    test/test_wait_graph.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)

# --- Scheduler Tests ---
xinim_add_host_test(test_scheduler
    test/test_scheduler.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

# --- Service Contract Tests ---
xinim_add_host_test(test_service_contract
    test/test_service_contract.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

# --- Sync Primitive Tests ---
xinim_add_host_test(test_ticket_spinlock
    test/test_ticket_spinlock.cpp
)

xinim_add_host_test(test_mcs_spinlock
    test/test_mcs_spinlock.cpp
)

xinim_add_host_test(test_phase_rwlock
    test/test_phase_rwlock.cpp
)

# adaptive_mutex test skipped: header calls sched::scheduler.is_blocked()
# which is not yet implemented in the Scheduler class.

xinim_add_host_test(test_capability_mutex
    test/test_capability_mutex.cpp
    ${XINIM_TEST_CAPMUTEX_SOURCES}
)

# --- Additional Scheduler Tests ---
xinim_add_host_test(test_scheduler_edge
    test/test_scheduler_edge.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

xinim_add_host_test(test_scheduler_deadlock
    test/test_scheduler_deadlock.cpp
    ${XINIM_TEST_SCHED_SOURCES}
)

# --- Data Structure Tests (header-only, no additional sources) ---
xinim_add_host_test(test_octonion
    test/test_octonion.cpp
)

xinim_add_host_test(test_fano_multiply
    test/test_fano_multiply.cpp
)

xinim_add_host_test(test_core_types
    test/test_core_types.cpp
)

# --- Crypto Tests (host-side, no kernel deps) ---
xinim_add_host_test(test_fips202
    test/test_fips202.cpp
    src/crypto/kyber_impl/fips202.cpp
)
target_include_directories(test_fips202 PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

xinim_add_host_test(test_ntt_roundtrip
    test/test_ntt_roundtrip.cpp
    src/crypto/kyber_impl/ntt.cpp
)
target_include_directories(test_ntt_roundtrip PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

xinim_add_host_test(test_montgomery_reduce
    test/test_montgomery_reduce.cpp
)
target_include_directories(test_montgomery_reduce PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

# --- Wait Graph Additional Tests ---
xinim_add_host_test(test_wait_graph_stress
    test/test_wait_graph_stress.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)

# --- Lock Manager Tests ---
xinim_add_host_test(test_lock_manager
    test/test_lock_manager.cpp
    ${XINIM_TEST_CAPMUTEX_SOURCES}
)

# --- Service Manager Tests ---
xinim_add_host_test(test_service_manager
    test/test_service_manager.cpp
    src/kernel/service.cpp
)

# --- Crypto Constants Tests (header-only) ---
xinim_add_host_test(test_kyber_constants
    test/test_kyber_constants.cpp
)
target_include_directories(test_kyber_constants PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)

# Apply subcategory labels for test filtering.
# The macro already applies LABELS "unit"; these extend it.
set_tests_properties(test_wait_graph test_wait_graph_stress PROPERTIES LABELS "unit;kernel")
set_tests_properties(test_scheduler test_scheduler_edge test_scheduler_deadlock test_service_contract PROPERTIES LABELS "unit;scheduler")
set_tests_properties(test_ticket_spinlock test_mcs_spinlock test_phase_rwlock test_capability_mutex test_lock_manager PROPERTIES LABELS "unit;sync")
set_tests_properties(test_fips202 test_ntt_roundtrip test_montgomery_reduce test_kyber_constants PROPERTIES LABELS "unit;crypto")
set_tests_properties(test_octonion test_fano_multiply test_core_types PROPERTIES LABELS "unit;math")
set_tests_properties(test_service_manager PROPERTIES LABELS "unit;kernel")

# --- Kyber512 E2E Compile/Constant Validation ---
xinim_add_host_test(test_kyber_e2e
    test/test_kyber_e2e.cpp
)
target_include_directories(test_kyber_e2e PRIVATE ${CMAKE_SOURCE_DIR}/src/crypto/kyber_impl)
target_compile_definitions(test_kyber_e2e PRIVATE KYBER_K=2)
set_tests_properties(test_kyber_e2e PROPERTIES LABELS "unit;crypto")

# --- Service Manager API Tests ---
xinim_add_host_test(test_service_manager_api
    test/test_service_manager_api.cpp
    src/kernel/service.cpp
)
set_tests_properties(test_service_manager_api PROPERTIES LABELS "unit;kernel")

# --- Net Driver Stub Tests ---
xinim_add_host_test(test_net_driver_stub
    test/test_net_driver_stub.cpp
    src/kernel/net_driver.cpp
)
target_include_directories(test_net_driver_stub PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_tests_properties(test_net_driver_stub PROPERTIES LABELS "unit;kernel")

# --- Syscall Dispatch Table Tests ---
xinim_add_host_test(test_syscall_dispatch
    test/test_syscall_dispatch.cpp
)
target_include_directories(test_syscall_dispatch PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_syscall_dispatch PROPERTIES LABELS "unit;kernel")

# --- IPC Channel Tests (lattice Channel inline methods, header-only) ---
xinim_add_host_test(test_ipc_channel
    test/test_ipc_channel.cpp
)
target_include_directories(test_ipc_channel PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_ipc_channel PROPERTIES LABELS "unit;ipc;kernel")

# --- Unified Scheduler Tests ---
xinim_add_host_test(test_unified_scheduler
    test/test_unified_scheduler.cpp
    src/kernel/unified_scheduler.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)
target_compile_definitions(test_unified_scheduler PRIVATE XINIM_ARCH_X86_64)
set_tests_properties(test_unified_scheduler PROPERTIES LABELS "unit;scheduler")

# --- Heap Allocator Tests ---
xinim_add_host_test(test_heap_allocator
    test/test_heap_allocator.cpp
    src/kernel/heap.cpp
)
set_tests_properties(test_heap_allocator PROPERTIES LABELS "unit;kernel")

# --- Process Lifecycle Tests ---
xinim_add_host_test(test_process_lifecycle
    test/test_process_lifecycle.cpp
    src/kernel/unified_scheduler.cpp
    src/kernel/process_lifecycle.cpp
    src/kernel/heap.cpp
    ${XINIM_TEST_COMMON_SOURCES}
)
target_compile_definitions(test_process_lifecycle PRIVATE XINIM_ARCH_X86_64)
set_tests_properties(test_process_lifecycle PROPERTIES LABELS "unit;kernel")

# --- Lock Timeout Tests ---
xinim_add_host_test(test_lock_timeout
    test/test_lock_timeout.cpp
)
target_include_directories(test_lock_timeout PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_lock_timeout PROPERTIES LABELS "unit;sync")

# --- IPC Blocking Tests ---
xinim_add_host_test(test_ipc_blocking
    test/test_ipc_blocking.cpp
)
target_include_directories(test_ipc_blocking PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_ipc_blocking PROPERTIES LABELS "unit;ipc;kernel")

# --- IPC Exhaustion Tests ---
xinim_add_host_test(test_ipc_exhaustion
    test/test_ipc_exhaustion.cpp
)
target_include_directories(test_ipc_exhaustion PRIVATE ${CMAKE_SOURCE_DIR}/src/kernel)
set_tests_properties(test_ipc_exhaustion PROPERTIES LABELS "unit;ipc;kernel")

# --- ELF Parser Tests ---
# Links elf_loader.cpp which calls early_serial; serial_stub.cpp satisfies that dep.
xinim_add_host_test(test_elf_parser
    test/test_elf_parser.cpp
    src/kernel/elf_loader.cpp
    test/stubs/serial_stub.cpp
)
target_include_directories(test_elf_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src/kernel
)
# serial_stub.cpp provides no-op methods; the class has private members only used
# in the real Serial16550 implementation.  Suppress the unused-field warning for
# this host-only stub target.
target_compile_options(test_elf_parser PRIVATE -Wno-unused-private-field)
set_tests_properties(test_elf_parser PROPERTIES LABELS "unit;kernel")

# --- Boot / Integration Tests (QEMU-based, require kernel image) ---
# These are registered but only meaningful when the kernel is built.

add_test(
    NAME boot_smoke_test
    COMMAND ${CMAKE_SOURCE_DIR}/test/boot/smoke_test.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(boot_smoke_test PROPERTIES
    LABELS "integration;boot"
    TIMEOUT 30
)

add_test(
    NAME kshell_test
    COMMAND python3 ${CMAKE_SOURCE_DIR}/test/boot/kshell_test.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(kshell_test PROPERTIES
    LABELS "integration;kshell"
    TIMEOUT 60
)

# =============================================================================
# Documentation Target (Doxygen)
# =============================================================================

find_program(DOXYGEN_EXECUTABLE doxygen)
if(DOXYGEN_EXECUTABLE)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/docs)
    configure_file(
        ${CMAKE_SOURCE_DIR}/docs/Doxyfile.in
        ${CMAKE_BINARY_DIR}/Doxyfile
        @ONLY
    )
    add_custom_target(xinim_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(WARNING "Doxygen not found; 'cmake --build --preset debug --target xinim_docs' will not work. Install doxygen to generate API docs.")
endif()

# =============================================================================
# Build Targets status
# =============================================================================

message(STATUS "XINIM Baseline Kernel Target Configured.")
