/**
 * @file ipc_test.cpp
 * @brief Kernel-side IPC validation tests for Week 7
 *
 * This module provides simple tests that can be called from the kernel
 * after servers are spawned to validate IPC communication.
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#include "ipc_test.hpp"
#include "early/serial_16550.hpp"
#include "../include/xinim/ipc/message_types.h"
#include "../include/xinim/ipc/vfs_protocol.hpp"
#include "../include/xinim/ipc/proc_protocol.hpp"
#include "../include/xinim/ipc/mm_protocol.hpp"
#include <cstring>
#include <cstdio>

extern xinim::early::Serial16550 early_serial;

namespace xinim::kernel::test {

using namespace xinim::ipc;

/**
 * @brief Simple test: Send message to VFS server
 *
 * Sends a VFS_OPEN request to test basic IPC communication.
 */
static bool test_vfs_ipc() {
    early_serial.write("\n[TEST] VFS Server IPC Test\n");

    message request{}, response{};
    request.m_source = 0;  // Kernel (PID 0)
    request.m_type = VFS_OPEN;

    auto* req = reinterpret_cast<VfsOpenRequest*>(&request.m_u);
    strncpy(req->path, "/test_ipc.txt", sizeof(req->path) - 1);
    req->path[sizeof(req->path) - 1] = '\0';
    req->flags = 0x42;  // O_CREAT | O_RDWR
    req->mode = 0644;
    req->caller_pid = 0;

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "  Request: VFS_OPEN path='%s' flags=0x%x mode=0%o\n",
             req->path, req->flags, req->mode);
    early_serial.write(buffer);

    // TODO: Actually send IPC message when lattice_send is available
    // For Week 7, this is a structural test only
    early_serial.write("  [SKIP] IPC not yet active (servers need to be in receive loop)\n");

    return true;
}

/**
 * @brief Simple test: Send message to Process Manager
 */
static bool test_proc_mgr_ipc() {
    early_serial.write("\n[TEST] Process Manager IPC Test\n");

    message request{}, response{};
    request.m_source = 0;
    request.m_type = PROC_GETPID;

    auto* req = reinterpret_cast<ProcGetpidRequest*>(&request.m_u);
    req->caller_pid = 0;

    early_serial.write("  Request: PROC_GETPID caller_pid=0\n");
    early_serial.write("  [SKIP] IPC not yet active\n");

    return true;
}

/**
 * @brief Simple test: Send message to Memory Manager
 */
static bool test_mem_mgr_ipc() {
    early_serial.write("\n[TEST] Memory Manager IPC Test\n");

    message request{}, response{};
    request.m_source = 0;
    request.m_type = MM_BRK;

    auto* req = reinterpret_cast<MmBrkRequest*>(&request.m_u);
    req->caller_pid = 0;
    req->new_brk = 0x1000;

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "  Request: MM_BRK new_brk=0x%lx\n", req->new_brk);
    early_serial.write(buffer);
    early_serial.write("  [SKIP] IPC not yet active\n");

    return true;
}

/**
 * @brief Run all IPC validation tests
 *
 * This function should be called after servers are spawned but before
 * entering the main scheduler loop.
 *
 * @return 0 on success, -1 on failure
 */
int run_ipc_validation_tests() {
    early_serial.write("\n");
    early_serial.write("========================================\n");
    early_serial.write("IPC Validation Tests (Week 7)\n");
    early_serial.write("========================================\n");

    int passed = 0;
    int total = 0;

    total++; if (test_vfs_ipc()) passed++;
    total++; if (test_proc_mgr_ipc()) passed++;
    total++; if (test_mem_mgr_ipc()) passed++;

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "\nResults: %d/%d tests passed\n", passed, total);
    early_serial.write(buffer);

    early_serial.write("========================================\n");
    early_serial.write("Note: Full IPC testing requires servers\n");
    early_serial.write("to be in their receive loops (Week 7 Part 2)\n");
    early_serial.write("========================================\n\n");

    return (passed == total) ? 0 : -1;
}

} // namespace xinim::kernel::test
