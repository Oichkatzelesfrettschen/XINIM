/*
 * @file lattice_ipc.cpp
 * @brief Capability-based IPC with bare-metal fixed-size structures.
 *
 * v1.2.0: Channel exhaustion returns error, message source tagging,
 * blocked sender/receiver tracking with back-pressure, optimized
 * recv via incoming channel bitset.
 */

#include "lattice_ipc.hpp"
#include "sys/const.hpp"
#include "../include/xinim/core_types.hpp"
#include "glo.hpp"
#include <xinim/net/network.hpp>
#include "schedule.hpp"
#include "unified_scheduler.hpp"
#include <cstring>

// Error codes from sys/error.hpp
#include "../include/sys/error.hpp"

namespace lattice {

Graph g_graph;

int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    if (node_id == 0) node_id = net::local_node();
    Channel* ch1 = g_graph.connect(src, dst, node_id);
    if (!ch1) return static_cast<int>(ErrorCode::E_CHAN_FULL);
    Channel* ch2 = g_graph.connect(dst, src, node_id);
    if (!ch2) return static_cast<int>(ErrorCode::E_CHAN_FULL);
    return xinim::OK;
}

void lattice_listen(xinim::pid_t pid) { g_graph.set_listening(pid, true); }

int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg, [[maybe_unused]] IpcFlags flags) {
    Channel *ch = g_graph.find(src, dst, ANY_NODE);
    if (!ch) {
        ch = g_graph.connect(src, dst, net::local_node());
        if (!ch) return static_cast<int>(ErrorCode::E_CHAN_FULL);
    }

    if (ch->node_id != net::local_node()) {
        return static_cast<int>(ErrorCode::EIO);
    }

    // v1.2.0: Tag message source before delivery
    message tagged = msg;
    tagged.m_source = src;

    // Direct handoff path: if receiver is listening, deliver to inbox
    if (g_graph.is_listening(dst)) {
        if (dst >= 0 && dst < Graph::MAX_PROCS) {
            g_graph.inbox_[dst] = tagged;
            g_graph.inbox_full_[dst] = true;
            g_graph.set_listening(dst, false);
            // Unblock receiver via unified scheduler
            xinim::kernel::g_unified_scheduler.unblock(dst);
            return xinim::OK;
        }
    }

    // Queue path: push to channel queue
    if (ch->push(tagged)) {
        // Mark incoming channel for fast recv scanning
        g_graph.mark_incoming(dst, src);
        return xinim::OK;
    }

    // Queue full
    if (flags == IpcFlags::NONBLOCK) {
        return static_cast<int>(ErrorCode::E_QUEUE_FULL);
    }

    // v1.2.0: Blocking send -- block sender until space opens
    auto* pcb = xinim::kernel::g_unified_scheduler.find_by_pid(src);
    if (pcb) {
        ch->add_blocked_sender(src);
        xinim::kernel::g_unified_scheduler.block(
            pcb, xinim::kernel::BlockReason::IPC_SEND, dst);
    }
    return static_cast<int>(ErrorCode::E_TRY_AGAIN);
}

int lattice_recv(xinim::pid_t pid, message *out, [[maybe_unused]] IpcFlags flags) {
    // Check inbox first (direct handoff from sender)
    if (pid >= 0 && pid < Graph::MAX_PROCS && g_graph.inbox_full_[pid]) {
        *out = g_graph.inbox_[pid];
        g_graph.inbox_full_[pid] = false;
        return xinim::OK;
    }

    // v1.2.0: Optimized scan using incoming channel bitset
    uint64_t incoming = g_graph.get_incoming(pid);
    if (incoming != 0) {
        while (incoming != 0) {
            int sender = __builtin_ctzll(incoming);
            incoming &= incoming - 1; // Clear lowest set bit

            Channel *ch = g_graph.find(sender, pid, net::local_node());
            if (ch && ch->pop(*out)) {
                // If there was a blocked sender, unblock them now
                xinim::pid_t blocked = ch->pop_blocked_sender();
                if (blocked >= 0) {
                    xinim::kernel::g_unified_scheduler.unblock(blocked);
                }
                // Clear bit if channel is now empty
                if (ch->count == 0) {
                    // Bit will be re-set on next send
                }
                return xinim::OK;
            }
        }
    }

    // Fallback: scan all potential senders (for channels not yet in bitset)
    for (int i = 0; i < Graph::MAX_PROCS; ++i) {
        Channel *ch = g_graph.find(i, pid, net::local_node());
        if (ch && ch->pop(*out)) {
            xinim::pid_t blocked = ch->pop_blocked_sender();
            if (blocked >= 0) {
                xinim::kernel::g_unified_scheduler.unblock(blocked);
            }
            return xinim::OK;
        }
    }

    // No message available
    if (flags == IpcFlags::NONBLOCK) {
        return static_cast<int>(ErrorCode::E_NO_MESSAGE);
    }

    // v1.2.0: Blocking receive -- block receiver until message arrives
    auto* pcb = xinim::kernel::g_unified_scheduler.find_by_pid(pid);
    if (pcb) {
        g_graph.set_listening(pid, true);
        xinim::kernel::g_unified_scheduler.block(
            pcb, xinim::kernel::BlockReason::IPC_RECV, -1);
    }
    return static_cast<int>(ErrorCode::E_NO_MESSAGE);
}

void poll_network() {
    // Network polling placeholder
}

// Graph methods are inline in lattice_ipc.hpp

} // namespace lattice
