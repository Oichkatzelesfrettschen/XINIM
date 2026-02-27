#pragma once
/**
 * @file lattice_ipc.hpp
 * @brief Directed acyclic graph based IPC primitives.
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
 * Refactored for bare-metal: uses fixed-size buffer.
 */
struct Channel {
    static constexpr std::size_t QUEUE_SIZE = 8;
    
    xinim::pid_t src{-1};
    xinim::pid_t dst{-1};
    net::node_t node_id{0};
    
    message queue[QUEUE_SIZE]{};
    std::size_t head{0};
    std::size_t tail{0};
    std::size_t count{0};

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
};

/**
 * @brief IPC graph managing channels and wait states.
 */
class Graph {
  public:
    static constexpr int MAX_CHANNELS = 128;
    static constexpr int MAX_PROCS = 64;

    Graph() {
        for (int i = 0; i < MAX_PROCS; ++i) {
            listening_[i] = false;
            inbox_full_[i] = false;
        }
    }

    Channel &connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id);
    Channel *find(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id) noexcept;

    bool is_listening(xinim::pid_t pid) const noexcept;
    void set_listening(xinim::pid_t pid, bool flag) noexcept;

    // Bare-metal: direct handoff via inbox array
    message inbox_[MAX_PROCS]{};
    bool inbox_full_[MAX_PROCS]{};

  private:
    Channel channels_[MAX_CHANNELS]{};
    int channel_count_{0};
    bool listening_[MAX_PROCS]{};
};

extern Graph g_graph;

int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id = 0);
void lattice_listen(xinim::pid_t pid);
int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg, IpcFlags flags = IpcFlags::NONE);
int lattice_recv(xinim::pid_t pid, message *out, IpcFlags flags = IpcFlags::NONE);
void poll_network();

} // namespace lattice
