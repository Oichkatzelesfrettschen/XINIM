#include "net_driver.hpp"

#include <deque>
#include <unordered_map>
#include <vector>

/**
 * @file net_driver.cpp
 * @brief Stub network driver for lattice IPC.
 */

namespace net {

namespace {
std::unordered_map<node_t, std::deque<Packet>> g_queues; ///< Per-node packet queues
}

int local_node() noexcept { return 0; }

void send(int node, std::span<const std::byte> data) {
    std::vector<std::byte> copy(data.begin(), data.end());
    Packet pkt{local_node(), std::move(copy)};
    g_queues[node].push_back(std::move(pkt));
}

bool recv(Packet &out) {
    auto &q = g_queues[local_node()];
    if (q.empty()) {
        return false;
    }
    out = std::move(q.front());
    q.pop_front();
    return true;
}

} // namespace net
