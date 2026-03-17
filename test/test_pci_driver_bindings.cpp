#include <xinim/boot/bootinfo.hpp>
#include <xinim/drivers/pci_binding.hpp>
#include <xinim/drivers/ahci.hpp>
#include <xinim/drivers/e1000.hpp>

#include <cstdlib>
#include <iostream>
#include <type_traits>

namespace {

bool expect(bool condition, const char* message, int& failures) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
        return false;
    }
    return true;
}

} // namespace

namespace xinim::boot {

const BootInfo& get_info() {
    static const BootInfo info{
        .protocol = BootProtocol::Unknown,
        .cmdline = nullptr,
        .acpi_rsdp = nullptr,
        .hhdm_offset = 0x100000000ULL,
    };
    return info;
}

} // namespace xinim::boot

int main() {
    int failures = 0;

    static_assert(std::is_same_v<decltype(&xinim::drivers::AHCIDriver::configure_from_pci),
                                 bool (xinim::drivers::AHCIDriver::*)(const xinim::pci::PCIDevice&)>,
                  "AHCIDriver should expose configure_from_pci(const PCIDevice&)");
    static_assert(std::is_same_v<decltype(&xinim::drivers::AHCIDriver::matches_pci_device),
                                 bool (*)(const xinim::pci::PCIDevice&) noexcept>,
                  "AHCIDriver should expose matches_pci_device(const PCIDevice&)");
    static_assert(std::is_same_v<decltype(&xinim::drivers::E1000Driver::configure_from_pci),
                                 bool (xinim::drivers::E1000Driver::*)(const xinim::pci::PCIDevice&)>,
                  "E1000Driver should expose configure_from_pci(const PCIDevice&)");
    static_assert(std::is_same_v<decltype(&xinim::drivers::E1000Driver::matches_pci_device),
                                 bool (*)(const xinim::pci::PCIDevice&) noexcept>,
                  "E1000Driver should expose matches_pci_device(const PCIDevice&)");

    {
        xinim::pci::PCIDevice device{};
        device.vendor_id = 0x8086;
        device.device_id = static_cast<uint16_t>(xinim::drivers::E1000Driver::DeviceID::E1000_82540EM);
        device.bars[0].address = 0xFEB00000ULL;
        device.bars[0].size = 0x20000ULL;
        device.bars[0].is_mmio = true;

        xinim::drivers::pci_binding::MappedBar bar{};
        expect(xinim::drivers::pci_binding::bind_mmio_bar(device, 0, bar),
               "pci binding should map E1000 BAR0 MMIO from an enumerated PCI device",
               failures);
        expect(bar.physical == 0xFEB00000ULL,
               "pci binding should preserve the physical BAR base",
               failures);
        expect(bar.size == 0x20000ULL,
               "pci binding should preserve the BAR span",
               failures);
        expect(reinterpret_cast<uintptr_t>(bar.base) == 0x1FEB00000ULL,
               "pci binding should map BAR0 through the shared x86 MMIO helper",
               failures);
    }

    {
        xinim::pci::PCIDevice device{};
        device.vendor_id = 0x8086;
        device.device_id = 0x2922;
        device.class_code = xinim::pci::class_code::MASS_STORAGE;
        device.subclass = xinim::pci::mass_storage::SATA;
        device.prog_if = 0x01;
        device.bars[5].address = 0xFEBF0000ULL;
        device.bars[5].size = 0x1100ULL;
        device.bars[5].is_mmio = true;

        xinim::drivers::pci_binding::MappedBar bar{};
        expect(xinim::drivers::pci_binding::bind_mmio_bar(device, 5, bar),
               "pci binding should map AHCI ABAR from BAR5",
               failures);
        expect(bar.physical == 0xFEBF0000ULL,
               "pci binding should preserve the physical ABAR base",
               failures);
        expect(bar.size == 0x1100ULL,
               "pci binding should preserve the ABAR span",
               failures);
        expect(reinterpret_cast<uintptr_t>(bar.base) == 0x1FEBF0000ULL,
               "pci binding should map BAR5 through the shared x86 MMIO helper",
               failures);
        expect(xinim::drivers::AHCIDriver::matches_pci_device(device),
               "ahci matching should accept SATA AHCI class devices",
               failures);
    }

    {
        xinim::pci::PCIDevice device{};
        device.vendor_id = 0x8086;
        device.device_id = 0x2922;
        device.class_code = xinim::pci::class_code::MASS_STORAGE;
        device.subclass = xinim::pci::mass_storage::SATA;
        device.prog_if = 0x00;
        device.bars[5].address = 0xFEBF0000ULL;
        device.bars[5].size = 0x1100ULL;
        device.bars[5].is_mmio = false;

        xinim::drivers::pci_binding::MappedBar bar{};
        expect(!xinim::drivers::pci_binding::bind_mmio_bar(device, 5, bar),
               "pci binding should reject a non-MMIO BAR",
               failures);
        expect(!xinim::drivers::AHCIDriver::matches_pci_device(device),
               "ahci matching should reject SATA controllers that are not AHCI programming interface",
               failures);
    }

    {
        xinim::pci::PCIDevice device{};
        device.vendor_id = 0x8086;
        device.device_id = static_cast<uint16_t>(xinim::drivers::E1000Driver::DeviceID::E1000_82540EM);
        device.bars[0].address = 0x0ULL;
        device.bars[0].size = 0x20000ULL;
        device.bars[0].is_mmio = true;

        xinim::drivers::pci_binding::MappedBar bar{};
        expect(!xinim::drivers::pci_binding::bind_mmio_bar(device, 0, bar),
               "pci binding should reject an unmappable zero MMIO base",
               failures);
        expect(xinim::drivers::E1000Driver::matches_pci_device(device),
               "e1000 matching should accept supported Intel device IDs",
               failures);
    }

    {
        xinim::pci::PCIDevice device{};
        device.vendor_id = 0x1234;
        device.device_id = 0x1111;

        expect(!xinim::drivers::E1000Driver::matches_pci_device(device),
               "e1000 matching should reject non-Intel devices",
               failures);
    }

    if (failures != 0) {
        std::cerr << failures << " PCI driver binding test(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "ALL PCI driver binding tests passed.\n";
    return EXIT_SUCCESS;
}
