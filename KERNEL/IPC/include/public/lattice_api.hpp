#ifndef XINIM_KERNEL_IPC_LATTICE_API_HPP
#define XINIM_KERNEL_IPC_LATTICE_API_HPP

#include <cstddef>  // For size_t
#include <cstdint>  // For uint32_t
#include <sys/types.h> // For ssize_t (POSIX) - consider a kernel-specific type later

namespace XINIM {
namespace IPC {

/** @brief Opaque handle for a lattice channel. */
using LatticeHandle = int; // Or a more complex type if needed later

/** @brief Represents an invalid or uninitialized lattice handle. */
constexpr LatticeHandle INVALID_LATTICE_HANDLE = -1;

// Forward declaration for capability token
struct ChannelCapabilityToken;

/**
 * @brief Flags for lattice_connect operation.
 */
enum class LatticeConnectFlags : uint32_t {
    NONE = 0,          ///< Default blocking behavior.
    NON_BLOCKING = 1 << 0 ///< Request non-blocking connection.
};

/**
 * @brief Flags for lattice_send operation.
 */
enum class LatticeSendFlags : uint32_t {
    NONE = 0,          ///< Default blocking behavior.
    NON_BLOCKING = 1 << 0, ///< Request non-blocking send.
    URGENT = 1 << 1      ///< Mark message as urgent (conceptual).
};

/**
 * @brief Flags for lattice_recv operation.
 */
enum class LatticeRecvFlags : uint32_t {
    NONE = 0,          ///< Default blocking behavior.
    NON_BLOCKING = 1 << 0, ///< Request non-blocking receive.
    PEEK = 1 << 1          ///< Peek at the message without removing it from the queue.
};

// --- Core IPC Primitives ---

/**
 * @brief Connects to a named service.
 * @param service_name The null-terminated string identifying the service.
 * @param flags Connection flags from LatticeConnectFlags.
 * @return A LatticeHandle on success, or INVALID_LATTICE_HANDLE on error (errno set).
 */
[[nodiscard]] LatticeHandle lattice_connect(const char* service_name, LatticeConnectFlags flags);

/**
 * @brief Creates a listening endpoint for a named service.
 * @param service_name The null-terminated string identifying the service to offer.
 * @param backlog The maximum length to which the queue of pending connections may grow.
 * @return A LatticeHandle representing the listener on success, or INVALID_LATTICE_HANDLE on error (errno set).
 */
[[nodiscard]] LatticeHandle lattice_listen(const char* service_name, int backlog);

/**
 * @brief Accepts an incoming connection on a listening handle.
 * @param listener_handle The handle returned by lattice_listen.
 * @param flags Connection flags for the accepted channel (e.g., NON_BLOCKING).
 * @return A new LatticeHandle for the accepted connection on success, or INVALID_LATTICE_HANDLE on error (errno set).
 *         If listener_handle is non-blocking and no connections are present, returns INVALID_LATTICE_HANDLE with errno set to EAGAIN.
 */
// Note: client_addr and addr_len parameters (like in POSIX accept) are omitted for simplicity here,
// but could be added if client identification is needed at this stage.
[[nodiscard]] LatticeHandle lattice_accept(LatticeHandle listener_handle, LatticeConnectFlags flags);

/**
 * @brief Sends data over a connected lattice channel.
 * @param handle The handle of the connected channel.
 * @param buffer Pointer to the data to send.
 * @param length Number of bytes to send.
 * @param flags Send flags from LatticeSendFlags.
 * @return Number of bytes sent on success (can be less than length if non-blocking and buffer is full).
 *         Returns -1 on error (errno set).
 */
[[nodiscard]] ssize_t lattice_send(LatticeHandle handle, const void* buffer, size_t length, LatticeSendFlags flags);

/**
 * @brief Receives data from a connected lattice channel.
 * @param handle The handle of the connected channel.
 * @param buffer Pointer to the buffer to store received data.
 * @param length Maximum number of bytes to receive.
 * @param flags Receive flags from LatticeRecvFlags.
 * @return Number of bytes received on success. Returns 0 if the peer has performed an orderly shutdown.
 *         Returns -1 on error (errno set). If non-blocking and no data, errno is EAGAIN.
 */
[[nodiscard]] ssize_t lattice_recv(LatticeHandle handle, void* buffer, size_t length, LatticeRecvFlags flags);

/**
 * @brief Closes a lattice channel or listening handle.
 * @param handle The handle to close.
 * @return 0 on success, -1 on error (errno set).
 */
int lattice_close(LatticeHandle handle);


// --- Potential future additions for capability passing or advanced features ---
// (Commented out as they are not implemented in this step)
/*
 * @brief Sends a capability token over a connected lattice channel.
 * @param handle The handle of the connected channel.
 * @param cap_token The capability token to send.
 * @param flags Send flags from LatticeSendFlags.
 * @return 0 on success, -1 on error (errno set).
 */
// int lattice_send_cap(LatticeHandle handle, const ChannelCapabilityToken& cap_token, LatticeSendFlags flags);

/*
 * @brief Receives a capability token from a connected lattice channel.
 * @param handle The handle of the connected channel.
 * @param cap_token Reference to store the received capability token.
 * @param flags Receive flags from LatticeRecvFlags.
 * @return 0 on success, -1 on error (errno set).
 */
// int lattice_recv_cap(LatticeHandle handle, ChannelCapabilityToken& cap_token, LatticeRecvFlags flags);

} // namespace IPC
} // namespace XINIM

#endif // XINIM_KERNEL_IPC_LATTICE_API_HPP
