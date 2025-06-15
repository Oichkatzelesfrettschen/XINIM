#pragma once
/**
 * @file lattice_ipc.hpp
 * @brief Directed acyclic graph based IPC primitives.
 */

#include "pqcrypto.hpp"
#include "proc.hpp"
#include <map>
#include <unordered_map>
#include <vector>

namespace lattice {

/**
 * @brief Special node identifier meaning "search all nodes".
 */
inline constexpr int ANY_NODE = -1;

/** Import the global ::message type into the lattice namespace. */
using ::message;

/**
 * @brief Channel connecting two processes.
 */
struct Channel {
    int src; //!< Source process id
    int dst; //!< Destination process id
    /** Identifier of the remote node or 0 for local. */
    int node_id{0};
    std::vector<message> queue;          //!< Pending messages encrypted with @c secret
    std::array<std::uint8_t, 32> secret; //!< Shared secret derived by PQ crypto
};

/**
 * @brief Graph storing channels as adjacency lists.
 */
class Graph {
  public:
    /**
     * @brief Add an edge between @p s and @p d on @p node_id creating a channel
     *        if absent.
     */
    Channel &connect(int s, int d, int node_id = 0);
    /** Find an existing channel or search all nodes when @p node_id equals ::ANY_NODE. */
    Channel *find(int s, int d, int node_id = ANY_NODE) noexcept;
    [[deprecated("Use find() with ANY_NODE")]]
    Channel *find_any(int s, int d) noexcept {
        return find(s, d, ANY_NODE);
    }
    /** Mark @p pid as waiting for a message. */
    void set_listening(int pid, bool flag) noexcept;
    /** Check if @p pid is currently waiting for a message. */
    [[nodiscard]] bool is_listening(int pid) const noexcept;

    std::map<std::tuple<int, int, int>, Channel>
        edges_;                               //!< channel storage keyed by (src,dst,node)
    std::unordered_map<int, bool> listening_; //!< listen state per pid
    std::unordered_map<int, message> inbox_;  //!< ready messages
};

extern Graph g_graph; //!< Global DAG instance

/**
 * @brief Establish a channel between two processes.
 *
 * If the channel does not yet exist it is created and a shared
 * secret derived using the PQ cryptography routines.
 *
 * @param src Source process identifier.
 * @param dst Destination process identifier.
 * @param node_id Identifier of the remote node, 0 meaning local.
 * @return Always returns ::OK for now.
 */
int lattice_connect(int src, int dst, int node_id = 0);

/**
 * @brief Mark a process as waiting for an incoming message.
 *
 * When a subsequent send targets this process the message will be
 * delivered immediately and control transferred to the receiver.
 *
 * @param pid Process identifier.
 */
void lattice_listen(int pid);

/**
 * @brief Send a message over an established channel.
 *
 * If the destination is listening the message is delivered and the
 * scheduler yields directly to the receiver via ::sched::Scheduler::yield_to.
 * When @p dst resides on another node the payload is forwarded to the
 * network layer. Otherwise the message is XOR encrypted with the channel
 * secret and queued on the channel.
 *
 * @see sched::Scheduler::yield_to
 *
 * @param src Sending process identifier.
 * @param dst Receiving process identifier.
 * @param msg Message to send.
 * @return ::OK on success.
 */
int lattice_send(int src, int dst, const message &msg);

/**
 * @brief Receive a message for a process.
 *
 * Queued messages are decrypted using the channel secret before being
 * delivered. If no message is pending the process is marked as
 * listening and ::E_NO_MESSAGE is returned. Remote messages are retrieved
 * from the inbox populated by the network layer.
 *
 * @param pid Process identifier.
 * @param msg Buffer to store the received message.
 * @return ::OK on success or ::E_NO_MESSAGE when no message is available.
 */
int lattice_recv(int pid, message *msg);

} // namespace lattice
