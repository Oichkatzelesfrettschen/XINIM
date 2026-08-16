#pragma once
/**
 * @file net_driver.hpp
 * @brief Stub network driver interface for freestanding kernel.
 *
 * This replaces the POSIX socket-based implementation with a minimal
 * interface suitable for a bare-metal kernel. No userland headers
 * (sockets, threads, filesystem, mutex) are used.
 */

#include <cstddef>
#include <cstdint>
#include <span>

namespace net {

    /** Integer identifier representing a logical network node. */
    using node_t = int;

    /**
     * @brief In-memory representation of a framed message.
     */
    struct Packet {
        node_t src_node;
        std::byte payload[2048];
        std::size_t payload_len;
    };

    /**
     * @brief Transport protocol used for a remote peer.
     */
    enum class Protocol { UDP, TCP };

    /**
     * @brief Network driver configuration structure.
     */
    struct Config {
        node_t node_id;
        std::uint16_t port;
        std::size_t max_queue_length;
    };

    /**
     * @brief Unbound network driver interface for the bare-metal kernel.
     *
     * Provides the interface that lattice_ipc.cpp expects. Operations return
     * failure because no E1000 or virtio-net backend owns a QEMU device,
     * interrupt route, descriptor ring, or packet buffer yet.
     */
    class NetDriver {
    public:
        bool init(const Config &cfg) noexcept;
        bool send(node_t node, std::span<const std::byte> data) noexcept;
        bool recv(Packet &out) noexcept;
        void shutdown() noexcept;
    };

    /**
     * @brief Return the local node identifier.
     *
     * Returns 1 (the default single-node ID) until multi-node networking
     * is implemented.
     */
    [[nodiscard]] node_t local_node() noexcept;

} // namespace net
