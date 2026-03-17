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

#include "virtio_net.hpp"
#include <xinim/pci/pci.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include "../../kernel/early/serial_16550.hpp"
#include <xinim/arch/x86/portio.hpp>

extern xinim::early::Serial16550 early_serial; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace xinim::drivers::net {
namespace {

using xinim::arch::x86::inb;
using xinim::arch::x86::inw;
using xinim::arch::x86::inl;
using xinim::arch::x86::outb;
using xinim::arch::x86::outw;
using xinim::arch::x86::outl;

// ============================================================================
// virtio PCI constants (VirtIO Spec v1.2, Appendix A)
// ============================================================================

/// PCI Vendor ID assigned to Red Hat / QEMU virtio devices.
constexpr uint16_t VIRTIO_VENDOR_ID  = 0x1AF4;
/// PCI Device ID for virtio-net (legacy = 0x1000; modern = 0x1041).
constexpr uint16_t VIRTIO_NET_DEVICE_ID_LEGACY = 0x1000;
constexpr uint16_t VIRTIO_NET_DEVICE_ID_MODERN = 0x1041;

// ============================================================================
// virtio status bits (VirtIO Spec v1.2, Section 2.1)
// ============================================================================

constexpr uint8_t VIRTIO_STATUS_ACKNOWLEDGE  = 0x01;
constexpr uint8_t VIRTIO_STATUS_DRIVER       = 0x02;
[[maybe_unused]] constexpr uint8_t VIRTIO_STATUS_DRIVER_OK    = 0x04;
constexpr uint8_t VIRTIO_STATUS_FEATURES_OK  = 0x08;
constexpr uint8_t VIRTIO_STATUS_FAILED       = 0x80;

// ============================================================================
// virtio-net feature bits (VirtIO Spec v1.2, Section 5.1.3)
// ============================================================================

[[maybe_unused]] constexpr uint32_t VIRTIO_NET_F_CSUM       = (1U << 0);
constexpr uint32_t VIRTIO_NET_F_MAC        = (1U << 5);
constexpr uint32_t VIRTIO_NET_F_STATUS     = (1U << 16);
[[maybe_unused]] constexpr uint32_t VIRTIO_NET_F_MRG_RXBUF  = (1U << 15);

// ============================================================================
// virtio-net queue indices
// ============================================================================

constexpr uint16_t VIRTIO_NET_RX_QUEUE = 0;
constexpr uint16_t VIRTIO_NET_TX_QUEUE = 1;

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
// Legacy virtio I/O BAR register offsets (VirtIO Spec v1.2, Appendix A)
// ============================================================================

constexpr uint16_t VIRTIO_REG_DEVICE_FEATURES = 0x00; // R, 32b
constexpr uint16_t VIRTIO_REG_GUEST_FEATURES  = 0x04; // W, 32b
[[maybe_unused]] constexpr uint16_t VIRTIO_REG_QUEUE_PFN       = 0x08; // W, 32b
constexpr uint16_t VIRTIO_REG_QUEUE_SIZE       = 0x0C; // R, 16b
constexpr uint16_t VIRTIO_REG_QUEUE_SELECT     = 0x0E; // W, 16b
[[maybe_unused]] constexpr uint16_t VIRTIO_REG_QUEUE_NOTIFY     = 0x10; // W, 16b
constexpr uint16_t VIRTIO_REG_DEVICE_STATUS    = 0x12; // RW, 8b
[[maybe_unused]] constexpr uint16_t VIRTIO_REG_ISR_STATUS       = 0x13; // R, 8b
constexpr uint16_t VIRTIO_REG_MAC_BASE         = 0x14; // Device-specific config

// ============================================================================
// Virtqueue descriptor (VirtIO Spec v1.2, Section 2.7.5)
// ============================================================================

[[maybe_unused]] constexpr uint16_t VRING_DESC_F_NEXT  = 0x1;
[[maybe_unused]] constexpr uint16_t VRING_DESC_F_WRITE = 0x2;

struct VringDesc {
    uint64_t addr;   ///< Guest physical address
    uint32_t len;    ///< Buffer length
    uint16_t flags;
    uint16_t next;   ///< Next descriptor index (if VRING_DESC_F_NEXT)
} __attribute__((packed));

// Virtqueue avail/used ring headers (ring arrays accessed via pointer math)
struct VringAvailHdr {
    uint16_t flags;
    uint16_t idx;
    // Followed by uint16_t ring[queue_size]
} __attribute__((packed));

struct VringUsedElem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct VringUsedHdr {
    uint16_t flags;
    uint16_t idx;
    // Followed by VringUsedElem ring[queue_size]
} __attribute__((packed));

// ============================================================================
// Driver state
// ============================================================================

constexpr std::size_t MAC_ADDR_LEN = 6;
[[maybe_unused]] constexpr std::size_t MAX_QUEUE_SIZE = 256;
[[maybe_unused]] constexpr std::size_t RX_BUF_SIZE = 1536; // MTU + virtio-net header

struct Virtqueue {
    VringDesc*     desc{nullptr};
    VringAvailHdr* avail{nullptr};
    VringUsedHdr*  used{nullptr};
    uint16_t       size{0};
    uint16_t       last_used_idx{0};
};

struct VirtioNetState {
    bool     initialized{false};
    uint16_t io_base{0};           // Legacy I/O BAR base port
    uint32_t negotiated_features{0};
    std::array<uint8_t, MAC_ADDR_LEN> mac{};
    Virtqueue rx_queue;
    Virtqueue tx_queue;
};

VirtioNetState g_state; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// ============================================================================
// Hex formatting helpers (no sprintf in freestanding)
// ============================================================================

static void write_hex16(char* buf, uint16_t val) {
    static constexpr char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(val >> 12) & 0xF];
    buf[1] = hex[(val >>  8) & 0xF];
    buf[2] = hex[(val >>  4) & 0xF];
    buf[3] = hex[(val >>  0) & 0xF];
    buf[4] = '\0';
}

static void write_hex8(char* buf, uint8_t val) {
    static constexpr char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(val >> 4) & 0xF];
    buf[1] = hex[(val >> 0) & 0xF];
    buf[2] = '\0';
}

} // anonymous namespace

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialise the virtio-net device via PCI enumeration.
 *
 * Steps: find device on PCI bus, negotiate features, read MAC,
 * set DRIVER_OK. Virtqueue allocation is deferred until a DMA
 * allocator provides physically-contiguous pages.
 */
bool virtio_net_init() {
    early_serial.write("[virtio-net] Scanning PCI bus for virtio-net device...\n");

    // Try legacy device ID first, then modern
    const xinim::pci::PCIDevice* dev = xinim::pci::PCI::find_device(
        VIRTIO_VENDOR_ID, VIRTIO_NET_DEVICE_ID_LEGACY);
    if (!dev) {
        dev = xinim::pci::PCI::find_device(
            VIRTIO_VENDOR_ID, VIRTIO_NET_DEVICE_ID_MODERN);
    }
    if (!dev) {
        early_serial.write("[virtio-net] No virtio-net PCI device found\n");
        return false;
    }

    char hex[5];
    early_serial.write("[virtio-net] Found device VID=");
    write_hex16(hex, dev->vendor_id);
    early_serial.write(hex);
    early_serial.write(" DID=");
    write_hex16(hex, dev->device_id);
    early_serial.write(hex);
    early_serial.write("\n");

    // Get I/O BAR (BAR0 for legacy virtio)
    if (!dev->bars[0].is_valid() || dev->bars[0].is_mmio) {
        early_serial.write("[virtio-net] BAR0 is not an I/O BAR -- unsupported\n");
        return false;
    }
    g_state.io_base = static_cast<uint16_t>(dev->bars[0].address);

    // Enable I/O space + bus mastering
    xinim::pci::PCI::enable_io_space(*dev);
    xinim::pci::PCI::enable_bus_master(*dev);

    // Reset device
    outb(g_state.io_base + VIRTIO_REG_DEVICE_STATUS, 0);

    // Set ACKNOWLEDGE
    outb(g_state.io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    // Set DRIVER
    outb(g_state.io_base + VIRTIO_REG_DEVICE_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // Read device features
    uint32_t device_features = inl(g_state.io_base + VIRTIO_REG_DEVICE_FEATURES);

    // Negotiate: request MAC + STATUS
    g_state.negotiated_features = device_features & (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS);
    outl(g_state.io_base + VIRTIO_REG_GUEST_FEATURES, g_state.negotiated_features);

    // Set FEATURES_OK
    outb(g_state.io_base + VIRTIO_REG_DEVICE_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    // Verify FEATURES_OK stuck
    uint8_t status = inb(g_state.io_base + VIRTIO_REG_DEVICE_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        early_serial.write("[virtio-net] FEATURES_OK not accepted by device\n");
        outb(g_state.io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    // Read MAC address if F_MAC negotiated
    if (g_state.negotiated_features & VIRTIO_NET_F_MAC) {
        for (size_t i = 0; i < MAC_ADDR_LEN; ++i) {
            g_state.mac[i] = inb(g_state.io_base + VIRTIO_REG_MAC_BASE +
                                 static_cast<uint16_t>(i));
        }
        char h[3];
        early_serial.write("[virtio-net] MAC: ");
        for (size_t i = 0; i < MAC_ADDR_LEN; ++i) {
            write_hex8(h, g_state.mac[i]);
            early_serial.write(h);
            if (i < MAC_ADDR_LEN - 1) early_serial.write(":");
        }
        early_serial.write("\n");
    }

    // Read virtqueue sizes (for logging; actual ring allocation needs DMA allocator)
    outw(g_state.io_base + VIRTIO_REG_QUEUE_SELECT, VIRTIO_NET_RX_QUEUE);
    uint16_t rx_size = inw(g_state.io_base + VIRTIO_REG_QUEUE_SIZE);
    outw(g_state.io_base + VIRTIO_REG_QUEUE_SELECT, VIRTIO_NET_TX_QUEUE);
    uint16_t tx_size = inw(g_state.io_base + VIRTIO_REG_QUEUE_SIZE);

    g_state.rx_queue.size = rx_size;
    g_state.tx_queue.size = tx_size;

    early_serial.write("[virtio-net] RX queue size: ");
    write_hex16(hex, rx_size);
    early_serial.write(hex);
    early_serial.write("  TX queue size: ");
    write_hex16(hex, tx_size);
    early_serial.write(hex);
    early_serial.write("\n");

    // Virtqueue ring allocation requires physically-contiguous DMA pages.
    // Defer to Phase D2: for now, mark initialized but queues not armed.
    // Do NOT set DRIVER_OK until queues are allocated.
    early_serial.write("[virtio-net] PCI walk complete; virtqueue DMA allocation pending\n");

    g_state.initialized = true;
    return true;
}

/**
 * @brief Transmit a raw Ethernet frame.
 *
 * Requires virtqueue DMA allocation (pending). Returns false until
 * TX ring is armed and DRIVER_OK is set.
 */
bool virtio_net_send([[maybe_unused]] const void* data,
                     [[maybe_unused]] size_t length) {
    if (!g_state.initialized || !g_state.tx_queue.desc) {
        return false;
    }
    // TODO: prepend VirtioNetHdr, write to TX avail ring, kick queue_notify
    return false;
}

/**
 * @brief Receive a raw Ethernet frame (polling mode).
 *
 * Requires virtqueue DMA allocation (pending). Returns false until
 * RX ring is armed and DRIVER_OK is set.
 */
bool virtio_net_recv([[maybe_unused]] void* buf,
                     [[maybe_unused]] size_t buflen,
                     [[maybe_unused]] size_t& out_len) {
    if (!g_state.initialized || !g_state.rx_queue.desc) {
        return false;
    }
    // TODO: poll used ring, copy to caller buffer, recycle to avail ring
    return false;
}

} // namespace xinim::drivers::net
