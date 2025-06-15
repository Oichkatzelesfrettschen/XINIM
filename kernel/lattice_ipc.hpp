#pragma once
/**
 * @file lattice_ipc.hpp
 * @brief Directed acyclic graph based IPC primitives.
 */

#include "pqcrypto.hpp"
#include "proc.hpp"
#include <unordered_map>
#include <vector>

namespace lattice {

/** Import the global ::message type into the lattice namespace. */
using ::message;

/**
 * @brief Channel connecting two processes.
 */
struct Channel {
    int src;                             //!< Source process id
    int dst;                             //!< Destination process id
    int node{0};                         //!< Destination node identifier
    std::vector<message> queue;          //!< Pending messages
    std::array<std::uint8_t, 32> secret; //!< Shared secret derived by PQ crypto
};

/**
 * @brief Graph storing channels as adjacency lists.
 */
class Graph {
  public:
    /**
     * @brief Add an edge between @p s and @p d on @p node creating a channel if
     *        absent.
     */
    Channel &connect(int s, int d, int node = 0);
    /** Find an existing channel or return nullptr. */
    Channel *find(int s, int d) noexcept;
    /** Mark @p pid as waiting for a message. */
    void set_listening(int pid, bool flag) noexcept { listening[pid] = flag; }
    /** Check if @p pid is currently waiting for a message. */
    [[nodiscard]] bool is_listening(int pid) const noexcept;

    std::unordered_map<int, std::vector<Channel>> edges; //!< adjacency list
    std::unordered_map<int, bool> listening;             //!< listen state per pid
    std::unordered_map<int, message> inbox;              //!< ready messages
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
 * @return Always returns ::OK for now.
 */
int lattice_connect(int src, int dst, int node = 0);

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
 * Otherwise the message is queued on the channel.
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
 * If no message is pending the process is marked as listening and
 * ::E_NO_MESSAGE is returned.
 *
 * @param pid Process identifier.
 * @param msg Buffer to store the received message.
 * @return ::OK on success or ::E_NO_MESSAGE when no message is available.
 */
int lattice_recv(int pid, message *msg);

} // namespace lattice
