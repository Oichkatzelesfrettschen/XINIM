#include "acpi.hpp"

namespace xinim::acpi {

    static uint8_t sum_bytes(const uint8_t *p, size_t n) {
        uint8_t s = 0;
        for (size_t i = 0; i < n; i++)
            s += p[i];
        return s;
    }

    static uint16_t read_le16(const uint8_t *bytes) {
        return static_cast<uint16_t>(bytes[0]) |
               static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8U);
    }

    static uint32_t read_le32(const uint8_t *bytes) {
        return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8U) |
               (static_cast<uint32_t>(bytes[2]) << 16U) | (static_cast<uint32_t>(bytes[3]) << 24U);
    }

    static const void *phys_to_hhdm_ptr(uint64_t phys_addr, uint64_t hhdm_offset) {
        return reinterpret_cast<const void *>(hhdm_offset + phys_addr);
    }

    static const Rsdp *resolve_rsdp(const void *rsdp_ptr, uint64_t hhdm_offset) {
        const auto raw = reinterpret_cast<uint64_t>(rsdp_ptr);
        if (raw == 0) {
            return nullptr;
        }
        if (raw < hhdm_offset) {
            return reinterpret_cast<const Rsdp *>(phys_to_hhdm_ptr(raw, hhdm_offset));
        }
        return reinterpret_cast<const Rsdp *>(rsdp_ptr);
    }

    Discovery probe(const void *rsdp_ptr, uint64_t hhdm_offset) {
        Discovery d{};
        for (uint32_t irq = 0U; irq < 16U; ++irq) {
            d.legacy_irq_routes[irq].gsi = irq;
        }
        auto *rsdp = resolve_rsdp(rsdp_ptr, hhdm_offset);
        if (rsdp == nullptr)
            return d;
        if (sum_bytes(reinterpret_cast<const uint8_t *>(rsdp),
                      (rsdp->revision >= 2) ? rsdp->length : 20) != 0)
            return d;
        const bool use_xsdt = rsdp->revision >= 2 && rsdp->xsdt_address != 0;
        const uint64_t table_phys = use_xsdt ? rsdp->xsdt_address : rsdp->rsdt_address;
        if (!table_phys)
            return d;
        d.xsdt_phys = table_phys;

        const auto *sdt =
            reinterpret_cast<const SdtHeader *>(phys_to_hhdm_ptr(table_phys, hhdm_offset));
        if (sdt->length < sizeof(SdtHeader))
            return d;

        const size_t entry_size = use_xsdt ? sizeof(uint64_t) : sizeof(uint32_t);
        const size_t entries = (sdt->length - sizeof(SdtHeader)) / entry_size;
        for (size_t i = 0; i < entries; i++) {
            uint64_t phys = 0;
            if (use_xsdt) {
                const auto *xsdt = reinterpret_cast<const Xsdt *>(sdt);
                phys = xsdt->entries[i];
            } else {
                const auto *rsdt = reinterpret_cast<const Rsdt *>(sdt);
                phys = rsdt->entries[i];
            }
            auto *hdr = reinterpret_cast<const SdtHeader *>(hhdm_offset + phys);
            if (hdr->signature[0] == 'A' && hdr->signature[1] == 'P' && hdr->signature[2] == 'I' &&
                hdr->signature[3] == 'C') {
                d.madt_phys = phys;
                auto *madt = reinterpret_cast<const Madt *>(hdr);
                d.lapic_mmio = madt->lapic_address;
                // Parse MADT subtables to find IOAPIC entries (type 1)
                const uint8_t *p = madt->entries;
                const uint8_t *end = reinterpret_cast<const uint8_t *>(madt) + madt->header.length;
                while (p + 2 <= end) {
                    uint8_t type = p[0];
                    uint8_t len = p[1];
                    if (len < 2 || p + len > end)
                        break;
                    if (type == 1 && len >= 12) { // IOAPIC
                        uint8_t ioapic_id = p[2];
                        (void) ioapic_id;
                        uint32_t ioapic_addr = read_le32(p + 4U);
                        uint32_t gsi_base = read_le32(p + 8U);
                        d.ioapic_phys = ioapic_addr;
                        d.ioapic_gsi_base = gsi_base;
                    } else if (type == 2 && len >= 10 && p[2] == 0U && p[3] < 16U) {
                        const uint8_t source_irq = p[3];
                        const uint16_t flags = read_le16(p + 8U);
                        const uint16_t polarity = flags & 0x3U;
                        const uint16_t trigger = (flags >> 2U) & 0x3U;
                        auto &route = d.legacy_irq_routes[source_irq];
                        route.gsi = read_le32(p + 4U);
                        route.active_low = polarity == 0x3U;
                        route.level_triggered = trigger == 0x3U;
                    }
                    p += len;
                }
            } else if (hdr->signature[0] == 'H' && hdr->signature[1] == 'P' &&
                       hdr->signature[2] == 'E' && hdr->signature[3] == 'T') {
                d.hpet_phys = phys;
                auto *hpet = reinterpret_cast<const HpetTable *>(hdr);
                d.hpet_mmio = hpet->address;
            }
        }
        return d;
    }

} // namespace xinim::acpi
