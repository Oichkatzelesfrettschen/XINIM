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
/** Queue of outbound packets keyed by destination node. */
static std::unordered_map<int, std::deque<std::vector<std::byte>>> g_queues;
} // namespace

/** \brief Return the local node identifier used for loopback tests. */
int local_node() noexcept { return 0; }

/**
 * @brief Queue bytes for delivery to @p node.
 *
 * The data is copied into an internal per-node queue so that tests may
 * retrieve it later via ::net::receive.
 */
void send(int node, std::span<const std::byte> data) {
    std::vector<std::byte> copy(data.begin(), data.end());
    g_queues[node].push_back(std::move(copy));
}

/**
 * @brief Pop the earliest packet destined for @p node.
 *
 * @return The bytes originally passed to ::net::send or an empty vector when
 *         no packet is pending.
 */
std::vector<std::byte> receive(int node) {
    auto it = g_queues.find(node);
    if (it == g_queues.end() || it->second.empty()) {
        return {};
    }
    auto data = std::move(it->second.front());
    it->second.pop_front();
    return data;
}

/**
 * @brief Clear all pending packets across every node.
 */
void reset() noexcept { g_queues.clear(); }

} // namespace net
