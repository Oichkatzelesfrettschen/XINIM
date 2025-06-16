#pragma once
/**
 * @file lattice_ipc.hpp
 * @brief Directed acyclic graph based IPC primitives.
 */

#include "../include/xinim/core_types.hpp"
#include "net_driver.hpp"
#include "octonion.hpp"
#include "pqcrypto.hpp"
#include "proc.hpp"
#include <map>
#include <unordered_map>
#include <vector>

namespace lattice {

/**
 * @brief Special node identifier meaning "search all nodes".
 */
inline constexpr net::node_t ANY_NODE = -1;

/** Import the global ::message type into the lattice namespace. */
using ::message;

/**
 * @brief Flags controlling send and receive behavior.
 */
enum class IpcFlags : unsigned {
    NONE = 0,     //!< Blocking semantics
    NONBLOCK = 1, //!< Return immediately if no message can be sent or received
};

/**
 * @brief Channel connecting two processes.
 */
struct Channel {
    xinim::pid_t src; //!< Source process identifier
    xinim::pid_t dst; //!< Destination process identifier
    /**
     * @brief Identifier of the remote node or 0 for local delivery.
     */
    net::node_t node_id{0};
    std::vector<message> queue; //!< Pending messages encrypted with @c secret
    Octonion secret;            //!< Capability derived from PQ secret
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
    Channel &connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id = 0);
    /** Find an existing channel or search all nodes when @p node_id equals ::ANY_NODE. */
    Channel *find(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id = ANY_NODE) noexcept;
    [[deprecated("Use find() with ANY_NODE")]]
    Channel *find_any(xinim::pid_t src, xinim::pid_t dst) noexcept {
        return find(src, dst, ANY_NODE);
    }
    /** Mark @p pid as waiting for a message. */
    void set_listening(xinim::pid_t pid, bool flag) noexcept;
    /** Check if @p pid is currently waiting for a message. */
    [[nodiscard]] bool is_listening(xinim::pid_t pid) const noexcept;

    std::map<std::tuple<xinim::pid_t, xinim::pid_t, net::node_t>, Channel>
        edges_;                                        //!< channel storage keyed by (src,dst,node)
    std::unordered_map<xinim::pid_t, bool> listening_; //!< listen state per pid
    std::unordered_map<xinim::pid_t, message> inbox_;  //!< ready messages
};

extern Graph g_graph; //!< Global DAG instance

/**
 * @brief Establish a channel between two processes.
 *
 * If absent a new channel is created. A capability token derived
 * from a Kyber secret is installed on both directions of the link.
 *
 * @param src Source process identifier.
 * @param dst Destination process identifier.
 * @param node_id Identifier of the remote node, 0 meaning local.
 * @return Always returns ::OK for now.
 */
int lattice_connect(xinim::pid_t src, xinim::pid_t dst, net::node_t node_id = 0);

/**
 * @brief Mark a process as waiting for an incoming message.
 *
 * When a subsequent send targets this process the message will be
 * delivered immediately and control transferred to the receiver.
 *
 * @param pid Process identifier.
 */
void lattice_listen(xinim::pid_t pid);

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
 * @param msg   Message to send.
 * @param flags Behaviour control flags such as ::IpcFlags::NONBLOCK.
 * @return ::OK on success or ::E_TRY_AGAIN when non-blocking fails.
 */
int lattice_send(xinim::pid_t src, xinim::pid_t dst, const message &msg,
                 IpcFlags flags = IpcFlags::NONE);

/**
 * @brief Receive a message for a process.
 *
 * Queued messages are decrypted using the channel secret before being
 * delivered. If no message is pending the process is marked as
 * listening and ::E_NO_MESSAGE is returned. Remote messages are retrieved
 * from the inbox populated by the network layer.
 *
 * @param pid Process identifier.
 * @param msg   Buffer to store the received message.
 * @param flags Behaviour control flags, e.g. ::IpcFlags::NONBLOCK.
 * @return ::OK on success or ::E_NO_MESSAGE when no message is available.
 */
int lattice_recv(xinim::pid_t pid, message *msg, IpcFlags flags = IpcFlags::NONE);

/**
 * @brief Poll the network driver for incoming packets.
 *
 * Received packets are transformed back into messages and encrypted with the
 * corresponding channel secret before being queued on that channel.
 */
void poll_network();

} // namespace lattice
