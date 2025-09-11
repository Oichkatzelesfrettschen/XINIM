#include "acpi.hpp"

namespace xinim::acpi {

static uint8_t sum_bytes(const uint8_t* p, size_t n) {
    uint8_t s=0; for (size_t i=0;i<n;i++) s+=p[i]; return s;
}

Discovery probe(uint64_t rsdp_phys, uint64_t hhdm_offset) {
    Discovery d{};
    if (!rsdp_phys) return d;
    auto* rsdp = reinterpret_cast<const Rsdp*>(hhdm_offset + rsdp_phys);
    if (sum_bytes(reinterpret_cast<const uint8_t*>(rsdp), (rsdp->revision>=2)?rsdp->length:20) != 0) return d;
    uint64_t xsdt_phys = (rsdp->revision>=2 && rsdp->xsdt_address)? rsdp->xsdt_address : rsdp->rsdt_address;
    d.xsdt_phys = xsdt_phys;
    auto* xsdt = reinterpret_cast<const Xsdt*>(hhdm_offset + xsdt_phys);
    size_t entries = (xsdt->header.length - sizeof(SdtHeader)) / sizeof(uint64_t);
    for (size_t i=0;i<entries;i++) {
        auto phys = xsdt->entries[i];
        auto* hdr = reinterpret_cast<const SdtHeader*>(hhdm_offset + phys);
        if (hdr->signature[0]=='A' && hdr->signature[1]=='P' && hdr->signature[2]=='I' && hdr->signature[3]=='C') {
            d.madt_phys = phys;
            auto* madt = reinterpret_cast<const Madt*>(hdr);
            d.lapic_mmio = madt->lapic_address;
            // Parse MADT subtables to find IOAPIC entries (type 1)
            const uint8_t* p = madt->entries;
            const uint8_t* end = reinterpret_cast<const uint8_t*>(madt) + madt->header.length;
            while (p + 2 <= end) {
                uint8_t type = p[0];
                uint8_t len  = p[1];
                if (len < 2) break;
                if (type == 1 && len >= 12) { // IOAPIC
                    uint8_t  ioapic_id = p[2];
                    (void)ioapic_id;
                    uint32_t ioapic_addr = *reinterpret_cast<const uint32_t*>(p+4);
                    uint32_t gsi_base    = *reinterpret_cast<const uint32_t*>(p+8);
                    d.ioapic_phys = ioapic_addr;
                    d.ioapic_gsi_base = gsi_base;
                    break;
                }
                p += len;
            }
        } else if (hdr->signature[0]=='H' && hdr->signature[1]=='P' && hdr->signature[2]=='E' && hdr->signature[3]=='T') {
            d.hpet_phys = phys;
            auto* hpet = reinterpret_cast<const HpetTable*>(hdr);
            d.hpet_mmio = hpet->address;
        }
    }
    return d;
}

} // namespace xinim::acpi
