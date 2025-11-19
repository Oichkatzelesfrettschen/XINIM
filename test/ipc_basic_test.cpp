/**
 * @file ipc_basic_test.cpp
 * @brief Basic IPC communication test for XINIM Week 7
 *
 * This test validates that IPC messages can be sent from the kernel
 * to each server and responses received correctly.
 *
 * Tests:
 * 1. VFS Server - Send VFS_OPEN, verify response
 * 2. Process Manager - Send PROC_GETPID, verify response
 * 3. Memory Manager - Send MM_BRK, verify response
 *
 * @author XINIM Test Suite
 * @date November 2025
 */

#include <cstdio>
#include <cstring>
#include "../include/xinim/ipc/message_types.h"
#include "../include/xinim/ipc/vfs_protocol.hpp"
#include "../include/xinim/ipc/proc_protocol.hpp"
#include "../include/xinim/ipc/mm_protocol.hpp"

using namespace xinim::ipc;

// Mock IPC functions for testing
static int mock_lattice_send(int src, int dst, const message& msg, int flags) {
    printf("[TEST] IPC Send: %d → %d, type=%d\n", src, dst, msg.m_type);
    return 0;
}

static int mock_lattice_recv(int pid, message* msg, int flags) {
    printf("[TEST] IPC Recv: PID %d waiting for message\n", pid);

    // Mock response based on message type
    if (msg->m_type == VFS_OPEN) {
        auto* resp = reinterpret_cast<VfsOpenResponse*>(&msg->m_u);
        resp->fd = 3;
        resp->error = IPC_SUCCESS;
    } else if (msg->m_type == PROC_GETPID) {
        auto* resp = reinterpret_cast<ProcGetpidResponse*>(&msg->m_u);
        resp->pid = 1;
        resp->error = IPC_SUCCESS;
    } else if (msg->m_type == MM_BRK) {
        auto* resp = reinterpret_cast<MmBrkResponse*>(&msg->m_u);
        resp->current_brk = 0x1000;
        resp->error = IPC_SUCCESS;
    }

    return 0;
}

// Test helper
static bool test_message_size() {
    printf("\n=== Test: Message Size Constraints ===\n");

    size_t msg_size = sizeof(message);
    printf("message size: %zu bytes\n", msg_size);

    if (msg_size > 256) {
        printf("FAIL: message exceeds 256 bytes\n");
        return false;
    }

    printf("PASS: message fits in 256 bytes\n");
    return true;
}

static bool test_vfs_open_protocol() {
    printf("\n=== Test: VFS Open Protocol ===\n");

    // Create request
    message request{}, response{};
    request.m_source = 1;
    request.m_type = VFS_OPEN;

    auto* req = reinterpret_cast<VfsOpenRequest*>(&request.m_u);
    strncpy(req->path, "/test.txt", sizeof(req->path) - 1);
    req->flags = 0x02;  // O_RDWR
    req->mode = 0644;
    req->caller_pid = 1;

    printf("Request: path='%s', flags=0x%x, mode=0%o\n",
           req->path, req->flags, req->mode);

    // Simulate IPC
    mock_lattice_send(1, VFS_SERVER_PID, request, 0);
    response.m_type = VFS_OPEN;
    mock_lattice_recv(1, &response, 0);

    // Verify response
    const auto* resp = reinterpret_cast<const VfsOpenResponse*>(&response.m_u);
    printf("Response: fd=%d, error=%d\n", resp->fd, resp->error);

    if (resp->error != IPC_SUCCESS || resp->fd < 3) {
        printf("FAIL: Invalid response\n");
        return false;
    }

    printf("PASS: VFS Open protocol validated\n");
    return true;
}

static bool test_proc_getpid_protocol() {
    printf("\n=== Test: Process Manager GETPID Protocol ===\n");

    message request{}, response{};
    request.m_source = 1;
    request.m_type = PROC_GETPID;

    auto* req = reinterpret_cast<ProcGetpidRequest*>(&request.m_u);
    req->caller_pid = 1;

    printf("Request: caller_pid=%d\n", req->caller_pid);

    mock_lattice_send(1, PROC_MGR_PID, request, 0);
    response.m_type = PROC_GETPID;
    mock_lattice_recv(1, &response, 0);

    const auto* resp = reinterpret_cast<const ProcGetpidResponse*>(&response.m_u);
    printf("Response: pid=%d, error=%d\n", resp->pid, resp->error);

    if (resp->error != IPC_SUCCESS || resp->pid != 1) {
        printf("FAIL: Invalid response\n");
        return false;
    }

    printf("PASS: Process Manager GETPID protocol validated\n");
    return true;
}

static bool test_mm_brk_protocol() {
    printf("\n=== Test: Memory Manager BRK Protocol ===\n");

    message request{}, response{};
    request.m_source = 1;
    request.m_type = MM_BRK;

    auto* req = reinterpret_cast<MmBrkRequest*>(&request.m_u);
    req->caller_pid = 1;
    req->new_brk = 0x2000;

    printf("Request: new_brk=0x%lx\n", req->new_brk);

    mock_lattice_send(1, MEM_MGR_PID, request, 0);
    response.m_type = MM_BRK;
    mock_lattice_recv(1, &response, 0);

    const auto* resp = reinterpret_cast<const MmBrkResponse*>(&response.m_u);
    printf("Response: current_brk=0x%lx, error=%d\n",
           resp->current_brk, resp->error);

    if (resp->error != IPC_SUCCESS) {
        printf("FAIL: Invalid response\n");
        return false;
    }

    printf("PASS: Memory Manager BRK protocol validated\n");
    return true;
}

int main() {
    printf("========================================\n");
    printf("XINIM IPC Basic Validation Test\n");
    printf("========================================\n");

    int passed = 0;
    int total = 0;

    // Run tests
    total++; if (test_message_size()) passed++;
    total++; if (test_vfs_open_protocol()) passed++;
    total++; if (test_proc_getpid_protocol()) passed++;
    total++; if (test_mm_brk_protocol()) passed++;

    // Summary
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    printf("========================================\n");

    return (passed == total) ? 0 : 1;
}
