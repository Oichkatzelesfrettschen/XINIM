#pragma once
#include <stdint.h>
#include <stddef.h>

namespace xinim::acpi {

#pragma pack(push,1)
struct Rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oemid[6];
    uint8_t  revision;
    uint32_t rsdt_address; // ACPI 1.0
    uint32_t length;       // ACPI 2.0+
    uint64_t xsdt_address; // ACPI 2.0+
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
};

struct SdtHeader {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oemid[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct Xsdt {
    SdtHeader header;
    uint64_t  entries[];
};

struct Madt {
    SdtHeader header;
    uint32_t lapic_address;
    uint32_t flags;
    uint8_t  entries[];
};

struct HpetTable {
    SdtHeader header;
    uint8_t   hardware_rev_id;
    uint8_t   comparator_count:5;
    uint8_t   counter_size:1;
    uint8_t   reserved:1;
    uint8_t   legacy_replacement:1;
    uint16_t  pci_vendor_id;
    uint8_t   address_space_id; // 0=MMIO
    uint8_t   register_bit_width;
    uint8_t   register_bit_offset;
    uint8_t   reserved2;
    uint64_t  address; // base MMIO
    uint8_t   hpet_number;
    uint16_t  min_tick;
    uint8_t   page_protection;
};
#pragma pack(pop)

struct Discovery {
    uint64_t xsdt_phys{0};
    uint64_t madt_phys{0};
    uint64_t hpet_phys{0};
    uint32_t lapic_mmio{0};
    uint64_t hpet_mmio{0};
    uint64_t ioapic_phys{0};
    uint32_t ioapic_gsi_base{0};
};

// Probe ACPI using RSDP physical address and HHDM offset to read tables.
Discovery probe(uint64_t rsdp_phys, uint64_t hhdm_offset);

} // namespace xinim::acpi
