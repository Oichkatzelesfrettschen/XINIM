/**
 * @file virtio_net_i486.cpp
 * @brief i486-native virtio-net driver with DMA ring allocation.
 *
 * Self-contained driver using i486 DMA allocator for physically contiguous
 * ring buffers. No hosted STL dependencies.
 */

#include "virtio_net_i486.hpp"
#include "dma_pages.hpp"
#include "console.hpp"
#include <xinim/pci/pci.hpp>

namespace xinim::i486::virtio_net {
namespace {

// Port I/O primitives
inline void outb(uint16_t port, uint8_t value) noexcept {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
inline void outw(uint16_t port, uint16_t value) noexcept {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}
inline void outl(uint16_t port, uint32_t value) noexcept {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}
inline uint8_t inb(uint16_t port) noexcept {
    uint8_t v; asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port)); return v;
}
inline uint16_t inw(uint16_t port) noexcept {
    uint16_t v; asm volatile("inw %1, %0" : "=a"(v) : "Nd"(port)); return v;
}
inline uint32_t inl(uint16_t port) noexcept {
    uint32_t v; asm volatile("inl %1, %0" : "=a"(v) : "Nd"(port)); return v;
}

// VirtIO constants
constexpr uint16_t VIRTIO_VENDOR = 0x1AF4U;
constexpr uint16_t VIRTIO_NET_LEGACY = 0x1000U;
constexpr uint16_t VIRTIO_NET_MODERN = 0x1041U;

constexpr uint8_t STATUS_ACK = 0x01U;
constexpr uint8_t STATUS_DRIVER = 0x02U;
constexpr uint8_t STATUS_DRIVER_OK = 0x04U;
constexpr uint8_t STATUS_FEATURES_OK = 0x08U;

constexpr uint32_t F_MAC = (1U << 5);
constexpr uint32_t F_STATUS = (1U << 16);

constexpr uint16_t REG_DEVICE_FEATURES = 0x00U;
constexpr uint16_t REG_GUEST_FEATURES = 0x04U;
constexpr uint16_t REG_QUEUE_PFN = 0x08U;
constexpr uint16_t REG_QUEUE_SIZE = 0x0CU;
constexpr uint16_t REG_QUEUE_SELECT = 0x0EU;
constexpr uint16_t REG_QUEUE_NOTIFY = 0x10U;
constexpr uint16_t REG_DEVICE_STATUS = 0x12U;
constexpr uint16_t REG_MAC_BASE = 0x14U;

constexpr uint16_t VRING_DESC_F_WRITE = 0x2U;

struct [[gnu::packed]] VringDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct [[gnu::packed]] VringAvail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[256]; // Max queue size
};

struct [[gnu::packed]] VringUsedElem {
    uint32_t id;
    uint32_t len;
};

struct [[gnu::packed]] VringUsed {
    uint16_t flags;
    uint16_t idx;
    VringUsedElem ring[256];
};

struct [[gnu::packed]] VirtioNetHdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
};

struct Virtqueue {
    VringDesc* desc;
    VringAvail* avail;
    VringUsed* used;
    uint16_t size;
    uint16_t last_used_idx;
    uint16_t next_avail;
};

constexpr uint32_t RX_BUF_SIZE = 1536U;
constexpr uint32_t MAX_QUEUE = 256U;

uint16_t g_io_base = 0U;
uint8_t g_mac[6]{};
Virtqueue g_rx{};
Virtqueue g_tx{};
bool g_ready = false;

// RX buffers (pre-allocated)
uint8_t* g_rx_buffers[MAX_QUEUE]{};
dma::DmaBuffer g_rx_dma_buf{};

uint32_t vring_size(uint32_t qsz) noexcept {
    // VirtIO spec: desc + avail + padding + used
    uint32_t desc_sz = qsz * 16U; // sizeof(VringDesc)
    uint32_t avail_sz = 4U + qsz * 2U + 2U;
    uint32_t used_sz = 4U + qsz * 8U + 2U;
    uint32_t align = 4096U;
    return ((desc_sz + avail_sz + align - 1U) & ~(align - 1U)) + used_sz;
}

bool setup_queue(uint16_t index, Virtqueue& vq) noexcept {
    outw(g_io_base + REG_QUEUE_SELECT, index);
    uint16_t qsz = inw(g_io_base + REG_QUEUE_SIZE);
    if (qsz == 0U || qsz > MAX_QUEUE) return false;

    uint32_t total = vring_size(qsz);
    dma::DmaBuffer ring = dma::allocate(total, 4096U);
    if (ring.address == nullptr) return false;

    vq.size = qsz;
    vq.desc = reinterpret_cast<VringDesc*>(ring.address);
    uint32_t avail_off = qsz * 16U;
    vq.avail = reinterpret_cast<VringAvail*>(ring.address + avail_off);
    uint32_t used_off = ((avail_off + 4U + qsz * 2U + 2U + 4095U) & ~4095U);
    vq.used = reinterpret_cast<VringUsed*>(ring.address + used_off);
    vq.last_used_idx = 0U;
    vq.next_avail = 0U;

    // Tell device the PFN (page frame number = physical address / 4096)
    uint32_t pfn = reinterpret_cast<uint32_t>(ring.address) / 4096U;
    outl(g_io_base + REG_QUEUE_PFN, pfn);

    return true;
}

void populate_rx_buffers() noexcept {
    // Allocate a block of RX buffers
    g_rx_dma_buf = dma::allocate(g_rx.size * RX_BUF_SIZE, 4096U);
    if (g_rx_dma_buf.address == nullptr) return;

    for (uint16_t i = 0U; i < g_rx.size; ++i) {
        g_rx_buffers[i] = g_rx_dma_buf.address + (i * RX_BUF_SIZE);

        // Fill descriptor: device writes to this buffer
        g_rx.desc[i].addr = reinterpret_cast<uint32_t>(g_rx_buffers[i]);
        g_rx.desc[i].len = RX_BUF_SIZE;
        g_rx.desc[i].flags = VRING_DESC_F_WRITE;
        g_rx.desc[i].next = 0U;

        // Add to avail ring
        g_rx.avail->ring[g_rx.next_avail % g_rx.size] = i;
        ++g_rx.next_avail;
    }
    g_rx.avail->idx = g_rx.next_avail;

    // Notify device that RX buffers are available
    outw(g_io_base + REG_QUEUE_NOTIFY, 0U); // RX queue = 0
}

} // namespace

bool initialize() noexcept {
    const xinim::pci::PCIDevice* dev = xinim::pci::PCI::find_device(VIRTIO_VENDOR, VIRTIO_NET_LEGACY);
    if (dev == nullptr) dev = xinim::pci::PCI::find_device(VIRTIO_VENDOR, VIRTIO_NET_MODERN);
    if (dev == nullptr) {
        console::write_string("virtio-net: no PCI device found");
        console::newline();
        return false;
    }

    if (!dev->bars[0].is_valid() || dev->bars[0].is_mmio) {
        console::write_string("virtio-net: BAR0 not I/O port");
        console::newline();
        return false;
    }

    g_io_base = static_cast<uint16_t>(dev->bars[0].address);
    xinim::pci::PCI::enable_io_space(*dev);
    xinim::pci::PCI::enable_bus_master(*dev);

    // Reset
    outb(g_io_base + REG_DEVICE_STATUS, 0U);
    outb(g_io_base + REG_DEVICE_STATUS, STATUS_ACK);
    outb(g_io_base + REG_DEVICE_STATUS, STATUS_ACK | STATUS_DRIVER);

    // Negotiate features
    uint32_t features = inl(g_io_base + REG_DEVICE_FEATURES);
    uint32_t guest = features & (F_MAC | F_STATUS);
    outl(g_io_base + REG_GUEST_FEATURES, guest);
    outb(g_io_base + REG_DEVICE_STATUS, STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);

    if ((inb(g_io_base + REG_DEVICE_STATUS) & STATUS_FEATURES_OK) == 0U) {
        console::write_string("virtio-net: FEATURES_OK rejected");
        console::newline();
        return false;
    }

    // Read MAC
    if ((guest & F_MAC) != 0U) {
        for (int i = 0; i < 6; ++i) {
            g_mac[i] = inb(g_io_base + REG_MAC_BASE + static_cast<uint16_t>(i));
        }
        console::write_string("virtio-net: MAC ");
        for (int i = 0; i < 6; ++i) {
            if (i > 0) console::write_char(':');
            console::write_hex32(g_mac[i]);
        }
        console::newline();
    }

    // Setup virtqueues with DMA
    if (!setup_queue(0U, g_rx)) {
        console::write_string("virtio-net: RX queue setup failed");
        console::newline();
        return false;
    }
    if (!setup_queue(1U, g_tx)) {
        console::write_string("virtio-net: TX queue setup failed");
        console::newline();
        return false;
    }

    console::write_string("virtio-net: RX=");
    console::write_dec32(g_rx.size);
    console::write_string(" TX=");
    console::write_dec32(g_tx.size);
    console::newline();

    // Populate RX ring with pre-allocated buffers
    populate_rx_buffers();

    // Set DRIVER_OK -- device is now live
    outb(g_io_base + REG_DEVICE_STATUS,
         STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK | STATUS_DRIVER_OK);

    console::write_string("virtio-net: DRIVER_OK set, device live");
    console::newline();

    g_ready = true;
    return true;
}

bool send(const void* data, uint32_t length) noexcept {
    if (!g_ready || data == nullptr || length == 0U) return false;

    // Allocate a TX buffer (header + frame)
    const uint32_t total = sizeof(VirtioNetHdr) + length;
    dma::DmaBuffer buf = dma::allocate(total, 16U);
    if (buf.address == nullptr) return false;

    // Write virtio-net header (all zeros = no offload)
    auto* hdr = reinterpret_cast<VirtioNetHdr*>(buf.address);
    *hdr = {};

    // Copy frame data after header
    const auto* src = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0U; i < length; ++i) {
        buf.address[sizeof(VirtioNetHdr) + i] = src[i];
    }

    // Fill TX descriptor
    uint16_t desc_idx = g_tx.next_avail % g_tx.size;
    g_tx.desc[desc_idx].addr = reinterpret_cast<uint32_t>(buf.address);
    g_tx.desc[desc_idx].len = total;
    g_tx.desc[desc_idx].flags = 0U;
    g_tx.desc[desc_idx].next = 0U;

    // Add to avail ring
    g_tx.avail->ring[g_tx.next_avail % g_tx.size] = desc_idx;
    ++g_tx.next_avail;
    g_tx.avail->idx = g_tx.next_avail;

    // Kick TX queue
    outw(g_io_base + REG_QUEUE_NOTIFY, 1U);

    return true;
}

bool recv(void* buffer, uint32_t buffer_size, uint32_t* out_length) noexcept {
    if (!g_ready || buffer == nullptr || out_length == nullptr) return false;

    // Check if device has placed any frames in the used ring
    if (g_rx.last_used_idx == g_rx.used->idx) return false;

    // Get the used element
    uint16_t used_slot = g_rx.last_used_idx % g_rx.size;
    uint32_t desc_id = g_rx.used->ring[used_slot].id;
    uint32_t frame_len = g_rx.used->ring[used_slot].len;

    if (desc_id >= g_rx.size) return false;

    // Copy frame data (skip VirtioNetHdr)
    const uint8_t* rx_buf = g_rx_buffers[desc_id];
    uint32_t payload_len = (frame_len > sizeof(VirtioNetHdr))
                               ? (frame_len - sizeof(VirtioNetHdr))
                               : 0U;
    if (payload_len > buffer_size) payload_len = buffer_size;

    auto* dst = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0U; i < payload_len; ++i) {
        dst[i] = rx_buf[sizeof(VirtioNetHdr) + i];
    }
    *out_length = payload_len;

    // Recycle buffer back to avail ring
    g_rx.avail->ring[g_rx.next_avail % g_rx.size] = static_cast<uint16_t>(desc_id);
    ++g_rx.next_avail;
    g_rx.avail->idx = g_rx.next_avail;
    ++g_rx.last_used_idx;

    // Notify device
    outw(g_io_base + REG_QUEUE_NOTIFY, 0U);

    return true;
}

bool is_initialized() noexcept { return g_ready; }
const uint8_t* mac_address() noexcept { return g_mac; }

} // namespace xinim::i486::virtio_net
