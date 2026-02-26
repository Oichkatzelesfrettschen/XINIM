/**
 * @file src/drivers/net/virtio_net.hpp
 * @brief virtio-net network driver public interface.
 *
 * WHY: Declares the three entry points that the kernel or net_driver.cpp
 *      calls to control the virtio-net device. Having a header prevents
 *      clang-tidy from flagging them as candidates for internal linkage.
 */

#pragma once
#include <cstddef>

namespace xinim::drivers::net {

/**
 * @brief Probe PCI bus for a virtio-net device and initialise it.
 * @return true if a device was found and initialised successfully.
 */
bool virtio_net_init();

/**
 * @brief Transmit a raw Ethernet frame.
 * @param data   Pointer to frame bytes (must include Ethernet header).
 * @param length Frame length in bytes.
 * @return true if the frame was enqueued, false on error or busy.
 */
bool virtio_net_send(const void* data, std::size_t length);

/**
 * @brief Receive a raw Ethernet frame (polling mode).
 * @param buf     Destination buffer.
 * @param buflen  Size of destination buffer.
 * @param out_len Set to number of bytes received on success.
 * @return true if a frame was dequeued into buf.
 */
bool virtio_net_recv(void* buf, std::size_t buflen, std::size_t& out_len);

} // namespace xinim::drivers::net
