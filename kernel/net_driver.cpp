/**
 * @file net_driver.cpp
 * @brief Stub network driver for Lattice IPC (loopback testing).
 */

#include "net_driver.hpp"

#include <deque>
#include <unordered_map>
#include <vector>
#include <span>

namespace net {

namespace {
    /// Per-node packet queues for loopback simulation.
    static std::unordered_map<node_t, std::deque<Packet>> g_queues;
}

/// Return the local node identifier (always 0 in this stub).
node_t local_node() noexcept {
    return 0;
}

/**
 * @brief Enqueue a packet for delivery to @p node.
 *
 * Copies @p data into a Packet with the current local_node() as the source.
 */
void send(node_t node, std::span<const std::byte> data) {
    Packet pkt;
    pkt.src_node = local_node();
    pkt.payload.assign(data.begin(), data.end());
    g_queues[node].push_back(std::move(pkt));
}

/**
 * @brief Dequeue the next packet destined for local_node().
 *
 * @param out  Receives the next Packet if available.
 * @return     true if a packet was dequeued, false if none pending.
 */
bool recv(Packet &out) {
    auto &q = g_queues[local_node()];
    if (q.empty()) {
        return false;
    }
    out = std::move(q.front());
    q.pop_front();
    return true;
}

/**
 * @brief Clear all pending packets on all nodes.
 */
void reset() noexcept {
    g_queues.clear();
}

} // namespace net
