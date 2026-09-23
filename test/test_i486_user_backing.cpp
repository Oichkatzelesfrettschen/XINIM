#include "../src/kernel/i486/dma_pages.hpp"
#include "../src/kernel/i486/user_backing.hpp"
#include "i486_check.hpp"

#include <cstring>
#include <sys/mman.h>

int main() {
    using namespace xinim;
    constexpr uint32_t kPageBytes = 4096U;
    constexpr uint32_t kArenaBytes = 48U * 1024U * 1024U;
    constexpr uint64_t kUsableBytes = uint64_t{2U} * 1024U * 1024U;
    int mapping_flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_32BIT
    mapping_flags |= MAP_32BIT;
#endif
    void *mapping = mmap(nullptr, kArenaBytes, PROT_READ | PROT_WRITE, mapping_flags, -1, 0);
    CHECK(mapping != MAP_FAILED);
    const uintptr_t physical_base = reinterpret_cast<uintptr_t>(mapping);
    CHECK(physical_base >= 0x00100000U && physical_base <= UINT32_MAX - kArenaBytes);
    boot::MemRange memory_range{physical_base, kUsableBytes, boot::MEMORY_RANGE_USABLE};
    boot::BootInfo boot_info{};
    boot_info.memory_map = &memory_range;
    boot_info.memory_map_entries = 1U;
    const i486::dma::PhysicalRange kernel{physical_base, kPageBytes};
    const i486::dma::PhysicalRange metadata{physical_base + kPageBytes, kPageBytes};
    i486::dma::initialize(boot_info, kernel, metadata);
    CHECK(i486::user_backing::acquire() == nullptr);
    CHECK(i486::user_backing::live_bytes() == 0U);

    memory_range.length = kArenaBytes;
    i486::dma::initialize(boot_info, kernel, metadata);
    uint8_t *images[i486::user_backing::kMaximumImages]{};
    for (uint32_t index = 0U; index < i486::user_backing::kMaximumImages; ++index) {
        images[index] = i486::user_backing::acquire();
        CHECK(images[index] != nullptr);
        CHECK((reinterpret_cast<uintptr_t>(images[index]) & (kPageBytes - 1U)) == 0U);
        for (uint32_t prior = 0U; prior < index; ++prior) {
            CHECK(images[index] != images[prior]);
        }
    }
    const uint32_t maximum_bytes =
        i486::user_backing::kMaximumImages * i486::user_backing::kImageBytes;
    CHECK(i486::user_backing::live_bytes() == maximum_bytes);
    CHECK(i486::user_backing::reserved_bytes() == maximum_bytes);
    CHECK(i486::user_backing::acquire() == nullptr);
    CHECK(!i486::user_backing::release(images[0] + kPageBytes));
    CHECK(i486::user_backing::release(images[0]));
    CHECK(!i486::user_backing::release(images[0]));
    std::memset(images[0], 0xA5, i486::user_backing::kImageBytes);
    CHECK(i486::user_backing::acquire() == images[0]);
    for (uint32_t offset = 0U; offset < i486::user_backing::kImageBytes; ++offset) {
        CHECK(images[0][offset] == 0U);
    }
    for (uint8_t *image : images) {
        CHECK(i486::user_backing::release(image));
    }
    CHECK(i486::user_backing::live_bytes() == 0U);
    for (uint32_t cycle = 0U; cycle < 12U; ++cycle) {
        CHECK(i486::user_backing::acquire() == images[0]);
        CHECK(i486::user_backing::release(images[0]));
    }
    CHECK(i486::user_backing::reserved_bytes() == maximum_bytes);
    CHECK(munmap(mapping, kArenaBytes) == 0);
    std::puts("i486 bounded user backing exhaustion, reuse, and zeroing checks passed");
    return 0;
}
