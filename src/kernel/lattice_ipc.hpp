#pragma once
/**
 * @file lattice_ipc.hpp
 * @brief Directed acyclic graph based IPC primitives.
 *
 * v1.2.0: Queue depth increased to 32, channel exhaustion returns error,
 * message source tagging, blocked sender/receiver tracking.
 */

#include "../include/xinim/core_types.hpp"
#include <xinim/net/net_driver.hpp>
#include "sys/type.hpp"
#include <array>

namespace lattice {

/** @brief IPC flags for send/receive operations. */
enum class IpcFlags : uint32_t {
    NONE = 0,
    NONBLOCK = 1,
};

/** @brief Identifier for any remote node. */
constexpr net::node_t ANY_NODE = -1;

/**
 * @brief Bidirectional communication channel between two processes.
 *
 * v1.2.0: Queue depth increased from 8 to 32 for better throughput
 * under load. Blocked sender tracking added for back-pressure.
 */
struct Channel {
    static constexpr std::size_t QUEUE_SIZE = 32;
    static constexpr int MAX_BLOCKED_SENDERS = 4;

    xinim::pid_t src{-1};
    xinim::pid_t dst{-1};
    net::node_t node_id{0};

    message queue[QUEUE_SIZE]{};
    std::size_t head{0};
    std::size_t tail{0};
    std::size_t count{0};

    // v1.2.0: Blocked sender tracking for back-pressure
    xinim::pid_t blocked_senders[MAX_BLOCKED_SENDERS]{};
    int blocked_sender_count{0};

    bool push(const message& msg) noexcept {
        if (count >= QUEUE_SIZE) return false;
        queue[tail] = msg;
        tail = (tail + 1) % QUEUE_SIZE;
        count++;
        return true;
    }

    bool pop(message& msg) noexcept {
        if (count == 0) return false;
        msg = queue[head];
        head = (head + 1) % QUEUE_SIZE;
        count--;
        return true;
    }

    void add_blocked_sender(xinim::pid_t pid) noexcept {
        if (blocked_sender_count < MAX_BLOCKED_SENDERS) {
            blocked_senders[blocked_sender_count++] = pid;
        }
    }

    xinim::pid_t pop_blocked_sender() noexcept {
        if (blocked_sender_count <= 0) return -1;
        xinim::pid_t pid = blocked_senders[0];
        for (int i = 1; i < blocked_sender_count; i++) {
            blocked_senders[i - 1] = blocked_senders[i];
        }
        blocked_sender_count--;
        return pid;
    }
};

/**
 * @brief IPC graph managing channels and wait states.
 *
 * v1.2.0: connect() returns error on channel exhaustion instead of
 * silently recycling. Per-process incoming channel bitset for O(1) recv.
 */
class Graph {
  public:
    static constexpr int MAX_CHANNELS = 128;
    static constexpr int MAX_PROCS = 64;

    Graph() {
        for (int i = 0; i < MAX_PROCS; ++i) {
            listening_[i] = false;
            inbox_full_[i] = false;
            incoming_channels_[i] = 0;
        }
    }

    /**
     * @brief Create or find a channel. Returns nullptr if table is full.
     *
     * v1.2.0: No longer silently recycles channels on exhaustion.
     */
    Channel* connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) {
        Channel *ch = find(src, dst, node_id);
        if (ch) return ch;
        if (channel_count_ >= MAX_CHANNELS) return nullptr;
        ch = &channels_[channel_count_++];
        ch->src = src;
        ch->dst = dst;
        ch->node_id = node_id;
        ch->head = 0;
        ch->tail = 0;
        ch->count = 0;
        ch->blocked_sender_count = 0;
        return ch;
    }

    Channel *find(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) noexcept {
        for (int i = 0; i < channel_count_; ++i) {
            if (channels_[i].src == src && channels_[i].dst == dst &&
                (node_id == ANY_NODE || channels_[i].node_id == node_id))
                return &channels_[i];
        }
        return nullptr;
    }

    bool is_listening(xinim::pid_t pid) const noexcept {
        if (pid >= 0 && pid < MAX_PROCS) return listening_[pid];
        return false;
    }

    void set_listening(xinim::pid_t pid, bool flag) noexcept {
        if (pid >= 0 && pid < MAX_PROCS) listening_[pid] = flag;
    }

    void mark_incoming(xinim::pid_t dst, xinim::pid_t src) noexcept {
        if (dst >= 0 && dst < MAX_PROCS && src >= 0 && src < 64) {
            incoming_channels_[dst] |= (1ULL << static_cast<unsigned>(src));
        }
    }

    uint64_t get_incoming(xinim::pid_t pid) const noexcept {
        if (pid >= 0 && pid < MAX_PROCS) return incoming_channels_[pid];
        return 0;
    }

    // Bare-metal: direct handoff via inbox array
    message inbox_[MAX_PROCS]{};
    bool inbox_full_[MAX_PROCS]{};

  private:
    Channel channels_[MAX_CHANNELS]{};
    int channel_count_{0};
    bool listening_[MAX_PROCS]{};
    uint64_t incoming_channels_[MAX_PROCS]{};
};

extern Graph g_graph;

int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id = 0);
void lattice_listen(xinim::pid_t pid);
int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg, IpcFlags flags = IpcFlags::NONE);
int lattice_recv(xinim::pid_t pid, message *out, IpcFlags flags = IpcFlags::NONE);
void poll_network();

} // namespace lattice
