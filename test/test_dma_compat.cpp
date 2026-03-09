#include <xinim/mm/dma.hpp>
#include <xinim/mm/dma_allocator.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <print>

namespace {

bool expect(bool condition, const char* message, int& failures) {
    if (!condition) {
        std::println(std::cerr, "FAIL: {}", message);
        ++failures;
        return false;
    }
    return true;
}

} // namespace

int main() {
    int failures = 0;

    expect(xinim::mm::DMAAllocator::initialize(), "DMA allocator should initialize", failures);
    expect(xinim::mm::DMAAllocator::get_available_memory() > 0,
           "host DMA allocator should report available synthetic DMA space",
           failures);

    auto buffer = xinim::mm::DMAAllocator::allocate(4096, xinim::mm::DMAFlags::ZERO);
    expect(buffer.is_valid(), "DMAAllocator::allocate should return a valid buffer", failures);
    if (buffer.is_valid()) {
        auto* bytes = static_cast<unsigned char*>(buffer.virtual_addr);
        expect(bytes != nullptr, "allocated buffer should expose a virtual pointer", failures);
        if (bytes != nullptr) {
            expect(bytes[0] == 0, "ZERO allocations should be zero-filled", failures);
            bytes[0] = 0x5A;
        }

        xinim::mm::DMAMapping mapping(buffer, true);
        expect(mapping.device_addr() == buffer.physical_addr,
               "DMAMapping should expose the physical address",
               failures);
    }

    auto isa_buffer = xinim::mm::DMAAllocator::allocate(
        8192, xinim::mm::DMAFlags::ZERO | xinim::mm::DMAFlags::BELOW_16MB);
    expect(isa_buffer.is_valid(), "legacy ISA-safe DMA allocation should succeed", failures);
    if (isa_buffer.is_valid()) {
        expect(isa_buffer.physical_addr < 0x01000000ULL,
               "legacy ISA-safe DMA allocation should stay below 16MiB",
               failures);
        expect(((isa_buffer.physical_addr & 0xFFFFULL) + isa_buffer.size) <= 0x10000ULL,
               "legacy ISA-safe DMA allocation should not cross a 64KiB boundary",
               failures);

        auto* isa_bytes = static_cast<unsigned char*>(isa_buffer.virtual_addr);
        expect(isa_bytes != nullptr, "legacy ISA buffer should expose a virtual pointer", failures);
        if (isa_bytes != nullptr) {
            isa_bytes[17] = 0xA5;
            expect(xinim::mm::DMAAllocator::virt_to_phys(isa_bytes + 17) ==
                       isa_buffer.physical_addr + 17,
                   "virt_to_phys should preserve offsets within DMA buffers",
                   failures);
            expect(xinim::mm::DMAAllocator::phys_to_virt(isa_buffer.physical_addr + 17) ==
                       isa_bytes + 17,
                   "phys_to_virt should preserve offsets within DMA buffers",
                   failures);
        }
    }

    auto too_large_isa = xinim::mm::DMAAllocator::allocate(
        70 * 1024, xinim::mm::DMAFlags::BELOW_16MB);
    expect(!too_large_isa.is_valid(),
           "legacy ISA-safe DMA allocation should reject buffers larger than 64KiB",
           failures);

    xinim::mm::SGList list;
    expect(list.add_entry(0x1000, 256), "SGList should accept the first entry", failures);
    expect(list.add_entry(0x1100, 128), "SGList should coalesce adjacent entries", failures);
    expect(list.count() == 1, "SGList should merge adjacent entries", failures);
    expect(list.total_length() == 384, "SGList total length should match the merged size", failures);

    xinim::mm::DMAPool pool(256, 4, xinim::mm::DMAFlags::ZERO);
    uint64_t phys_addr = 0;
    void* block = pool.allocate(phys_addr);
    expect(block != nullptr, "DMAPool should allocate a block", failures);
    expect(phys_addr != 0, "DMAPool should provide a physical address", failures);
    pool.free(block);

    if (buffer.is_valid()) {
        xinim::mm::DMAAllocator::free(buffer);
    }
    if (isa_buffer.is_valid()) {
        xinim::mm::DMAAllocator::free(isa_buffer);
    }
    xinim::mm::DMAAllocator::shutdown();

    if (failures != 0) {
        std::println(std::cerr, "{} DMA compatibility test(s) failed.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "ALL DMA compatibility tests passed.");
    return EXIT_SUCCESS;
}
