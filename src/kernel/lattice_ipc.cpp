/*
 * @file lattice_ipc.cpp
 * @brief Capability‐based IPC with bare-metal fixed-size structures.
 */

#include "lattice_ipc.hpp"
#include "sys/const.hpp"
#include "../include/xinim/core_types.hpp"
#include "glo.hpp"
#include <xinim/net/network.hpp>
#include "schedule.hpp"
#include "sys/error.hpp"
#include <cstring>

namespace lattice {

Graph g_graph;

int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    if (node_id == 0) node_id = net::local_node();
    g_graph.connect(src, dst, node_id);
    g_graph.connect(dst, src, node_id);
    return xinim::OK;
}

void lattice_listen(xinim::pid_t pid) { g_graph.set_listening(pid, true); }

int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg, [[maybe_unused]] IpcFlags flags) {
    Channel *ch = g_graph.find(src, dst, ANY_NODE);
    if (!ch) ch = &g_graph.connect(src, dst, net::local_node());

    if (ch->node_id != net::local_node()) {
        // Remote IPC not supported: no network driver available.
        // Phase 6 (P6-T08) will implement E1000 for multi-node IPC.
        return static_cast<int>(ErrorCode::EIO);
    }

    if (g_graph.is_listening(dst)) {
        if (dst >= 0 && dst < Graph::MAX_PROCS) {
            g_graph.inbox_[dst] = msg;
            g_graph.inbox_full_[dst] = true;
            g_graph.set_listening(dst, false);
            sched::scheduler.unblock(dst);
            sched::scheduler.yield_to(dst);
            return xinim::OK;
        }
    }

    if (ch->push(msg)) return xinim::OK;
    return static_cast<int>(ErrorCode::E_TRY_AGAIN);
}

int lattice_recv(xinim::pid_t pid, message *out, [[maybe_unused]] IpcFlags flags) {
    if (pid >= 0 && pid < Graph::MAX_PROCS && g_graph.inbox_full_[pid]) {
        *out = g_graph.inbox_[pid];
        g_graph.inbox_full_[pid] = false;
        return xinim::OK;
    }

    for (int i = 0; i < Graph::MAX_PROCS; ++i) {
        Channel *ch = g_graph.find(i, pid, net::local_node());
        if (ch && ch->pop(*out)) {
            return xinim::OK;
        }
    }
    
    return static_cast<int>(ErrorCode::E_NO_MESSAGE);
}

void poll_network() {
    // Network polling placeholder
}

Channel &Graph::connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
    Channel *ch = find(src, dst, node_id);
    if (ch) return *ch;

    if (channel_count_ >= MAX_CHANNELS) {
        // Channel table full -- recycle the oldest channel.
        // This is a last resort; callers should avoid exhausting the table.
        channel_count_ = MAX_CHANNELS - 1;
    }

    ch = &channels_[channel_count_++];
    ch->src = src;
    ch->dst = dst;
    ch->node_id = node_id;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    return *ch;
}

Channel *Graph::find(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) noexcept {
    for (int i = 0; i < channel_count_; ++i) {
        if (channels_[i].src == src && channels_[i].dst == dst && 
            (node_id == ANY_NODE || channels_[i].node_id == node_id))
            return &channels_[i];
    }
    return nullptr;
}

bool Graph::is_listening(xinim::pid_t pid) const noexcept { 
    if (pid >= 0 && pid < MAX_PROCS) return listening_[pid];
    return false;
}

void Graph::set_listening(xinim::pid_t pid, bool flag) noexcept { 
    if (pid >= 0 && pid < MAX_PROCS) listening_[pid] = flag;
}

} // namespace lattice
