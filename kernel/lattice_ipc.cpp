/*
 * @file lattice_ipc.cpp
 * @brief Reference-counted message buffer and lattice IPC logic.
 */

#include "lattice_ipc.hpp"
#include "glo.hpp"
#include "proc.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>
#include <algorithm>

namespace lattice {

/**
 * @class MessageBuffer
 * @brief Shared container for IPC message bytes.
 *
 * The buffer uses reference counting so multiple threads can access the same
 * underlying storage without copying. Lifetime is automatically managed via
 * std::shared_ptr.
 */
class MessageBuffer {
  public:
    using Byte = std::byte; ///< Convenience alias for byte type

    /// Construct an empty MessageBuffer.
    MessageBuffer() = default;

    /**
     * @brief Construct buffer of given @p size in bytes.
     * @param size Number of bytes to allocate.
     */
    explicit MessageBuffer(std::size_t size)
      : data_{std::make_shared<std::vector<Byte>>(size)} {}

    /**
     * @brief Obtain mutable view of the stored bytes.
     * @return Span covering the buffer contents.
     */
    [[nodiscard]] std::span<Byte> span() noexcept {
        return data_
            ? std::span<Byte>{data_->data(), data_->size()}
            : std::span<Byte>{};
    }

    /**
     * @brief Obtain const view of the stored bytes.
     * @return Span over immutable bytes.
     */
    [[nodiscard]] std::span<const Byte> span() const noexcept {
        return data_
            ? std::span<const Byte>{data_->data(), data_->size()}
            : std::span<const Byte>{};
    }

    /// Number of bytes in the buffer.
    [[nodiscard]] std::size_t size() const noexcept {
        return data_ ? data_->size() : 0U;
    }

    /// Access underlying shared pointer for advanced sharing.
    [[nodiscard]] std::shared_ptr<std::vector<Byte>> share() const noexcept {
        return data_;
    }

  private:
    std::shared_ptr<std::vector<Byte>> data_{}; ///< Shared data container
};

//------------------------------------------------------------------------------
//  IPC / Graph implementation
//------------------------------------------------------------------------------

Graph g_graph{};

/**
 * @brief Create or retrieve a channel between two processes.
 */
Channel &Graph::connect(int s, int d) {
    auto &vec = edges[s];
    auto it = std::find_if(vec.begin(), vec.end(),
                           [d](const Channel &c){ return c.dst == d; });
    if (it != vec.end()) {
        return *it;
    }
    pqcrypto::KeyPair a = pqcrypto::generate_keypair();
    pqcrypto::KeyPair b = pqcrypto::generate_keypair();
    Channel ch{ .src = s, .dst = d };
    ch.secret = pqcrypto::establish_secret(a, b);
    vec.push_back(ch);
    return vec.back();
}

/**
 * @brief Locate an existing channel.
 */
Channel *Graph::find(int s, int d) noexcept {
    auto it = edges.find(s);
    if (it == edges.end()) {
        return nullptr;
    }
    auto &vec = it->second;
    auto vit = std::find_if(vec.begin(), vec.end(),
                            [d](const Channel &c){ return c.dst == d; });
    return (vit != vec.end()) ? &*vit : nullptr;
}

/**
 * @brief Check whether a process is waiting for a message.
 */
bool Graph::is_listening(int pid) const noexcept {
    auto it = listening.find(pid);
    return (it != listening.end()) && it->second;
}

/**
 * @brief Wrapper around Graph::connect for external consumers.
 */
int lattice_connect(int src, int dst) {
    g_graph.connect(src, dst);
    return OK;
}

/**
 * @brief Register a process as listening for a message.
 */
void lattice_listen(int pid) {
    g_graph.set_listening(pid, true);
}

/**
 * @brief Helper to switch execution to another process.
 */
static void yield_to(int pid) {
    proc_ptr = proc_addr(pid);
    cur_proc = pid;
}

/**
 * @brief Send a message across a channel.
 */
int lattice_send(int src, int dst, const message &msg) {
    Channel *ch = g_graph.find(src, dst);
    if (ch == nullptr) {
        ch = &g_graph.connect(src, dst);
    }
    if (g_graph.is_listening(dst)) {
        g_graph.inbox[dst] = msg;
        g_graph.set_listening(dst, false);
        yield_to(dst);
    } else {
        ch->queue.push_back(msg);
    }
    return OK;
}

/**
 * @brief Receive a message destined for a process.
 */
int lattice_recv(int pid, message *msg) {
    auto it = g_graph.inbox.find(pid);
    if (it != g_graph.inbox.end()) {
        *msg = it->second;
        g_graph.inbox.erase(it);
        return OK;
    }
    for (auto &[src, vec] : g_graph.edges) {
        auto vit = std::find_if(vec.begin(), vec.end(),
                                [pid](const Channel &c){
                                    return c.dst == pid && !c.queue.empty();
                                });
        if (vit != vec.end()) {
            *msg = vit->queue.front();
            vit->queue.erase(vit->queue.begin());
            return OK;
        }
    }
    lattice_listen(pid);
    return static_cast<int>(ErrorCode::E_NO_MESSAGE);
}

} // namespace lattice
