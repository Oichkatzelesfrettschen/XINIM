/**
 * @file src/drivers/net/virtio_net.cpp
 * @brief virtio-net network driver skeleton for XINIM.
 *
 * WHY: virtio-net is the standard QEMU-provided NIC. Implementing it avoids
 *      dependency on physical hardware and gives a clean PCI-attached device
 *      to drive the NetDriver interface.
 *
 * CURRENT STATUS (Phase 7 skeleton):
 *   - PCI capability walk and device detection: stubbed.
 *   - Virtqueue layout constants: defined.
 *   - Init/send/recv: return failure (no hardware access yet).
 *   Phase 8 will wire PCI MMIO access and complete the ring buffer logic.
 *
 * REFERENCES:
 *   - VirtIO Specification v1.2, Section 5.1 (Network Device)
 *   - QEMU hw/net/virtio-net.c
 */

#include <cstddef>
#include <cstdint>
#include "../../kernel/early/serial_16550.hpp"

extern xinim::early::Serial16550 early_serial;

namespace xinim::drivers::net {

// ============================================================================
// virtio PCI constants (VirtIO Spec v1.2, Appendix A)
// ============================================================================

/// PCI Vendor ID assigned to Red Hat / QEMU virtio devices.
static constexpr uint16_t VIRTIO_VENDOR_ID  = 0x1AF4;
/// PCI Device ID for virtio-net (legacy = 0x1000; modern = 0x1041).
static constexpr uint16_t VIRTIO_NET_DEVICE_ID_LEGACY = 0x1000;
static constexpr uint16_t VIRTIO_NET_DEVICE_ID_MODERN = 0x1041;

// ============================================================================
// virtio status bits (VirtIO Spec v1.2, Section 2.1)
// ============================================================================

static constexpr uint8_t VIRTIO_STATUS_ACKNOWLEDGE  = 0x01;
static constexpr uint8_t VIRTIO_STATUS_DRIVER       = 0x02;
static constexpr uint8_t VIRTIO_STATUS_DRIVER_OK    = 0x04;
static constexpr uint8_t VIRTIO_STATUS_FEATURES_OK  = 0x08;
static constexpr uint8_t VIRTIO_STATUS_FAILED       = 0x80;

// ============================================================================
// virtio-net feature bits (VirtIO Spec v1.2, Section 5.1.3)
// ============================================================================

static constexpr uint32_t VIRTIO_NET_F_CSUM       = (1U << 0);
static constexpr uint32_t VIRTIO_NET_F_MAC        = (1U << 5);
static constexpr uint32_t VIRTIO_NET_F_STATUS     = (1U << 16);
static constexpr uint32_t VIRTIO_NET_F_MRG_RXBUF  = (1U << 15);

// ============================================================================
// virtio-net queue indices
// ============================================================================

static constexpr uint16_t VIRTIO_NET_RX_QUEUE = 0;
static constexpr uint16_t VIRTIO_NET_TX_QUEUE = 1;

// ============================================================================
// virtio-net header (prepended to every packet, VirtIO Spec v1.2 Section 5.1.6)
// ============================================================================

struct VirtioNetHdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    // num_buffers only present when VIRTIO_NET_F_MRG_RXBUF negotiated
} __attribute__((packed));

// ============================================================================
// Virtqueue descriptor (VirtIO Spec v1.2, Section 2.7.5)
// ============================================================================

static constexpr uint16_t VRING_DESC_F_NEXT  = 0x1; ///< Buffer continues in next field
static constexpr uint16_t VRING_DESC_F_WRITE = 0x2; ///< Buffer is device-writable

struct VringDesc {
    uint64_t addr;   ///< Guest physical address
    uint32_t len;    ///< Buffer length
    uint16_t flags;
    uint16_t next;   ///< Next descriptor index (if VRING_DESC_F_NEXT)
} __attribute__((packed));

// ============================================================================
// Driver state
// ============================================================================

struct VirtioNetState {
    bool     initialized{false};
    uint32_t negotiated_features{0};
    uint8_t  mac[6]{};
};

static VirtioNetState g_state;

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Attempt to initialise the virtio-net device.
 *
 * Phase 7: Logs the attempt and returns false (no PCI MMIO access yet).
 * Phase 8: Walk PCI bus, locate 1AF4:1000/1041, negotiate features, set up
 *           virtqueues, enable DRIVER_OK bit.
 *
 * @return false until Phase 8 PCI walk is implemented.
 */
bool virtio_net_init() {
    early_serial.write("[virtio-net] init: PCI walk not yet implemented (Phase 8)\n");
    early_serial.write("[virtio-net] Expected device: VID=1AF4 DID=1000 (legacy) / 1041 (modern)\n");
    g_state.initialized = false;
    return false;
}

/**
 * @brief Transmit a raw Ethernet frame.
 *
 * @param data   Pointer to frame bytes (must include Ethernet header).
 * @param length Frame length in bytes.
 * @return false until virtqueues are initialised.
 */
bool virtio_net_send([[maybe_unused]] const void* data,
                     [[maybe_unused]] size_t length) {
    return false; // Phase 8: enqueue to TX virtqueue
}

/**
 * @brief Receive a raw Ethernet frame (polling mode).
 *
 * @param buf    Destination buffer (must be >= 1514 bytes).
 * @param buflen Size of destination buffer.
 * @param out_len Set to number of bytes received on success.
 * @return false if no frame available or driver not initialised.
 */
bool virtio_net_recv([[maybe_unused]] void* buf,
                     [[maybe_unused]] size_t buflen,
                     [[maybe_unused]] size_t& out_len) {
    return false; // Phase 8: dequeue from RX virtqueue
}

} // namespace xinim::drivers::net
