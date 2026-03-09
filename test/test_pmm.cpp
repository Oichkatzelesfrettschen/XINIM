#include <xinim/mm/pmm.hpp>

#include <cstdio>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1; \
        } \
    } while (0)

static int test_zone_alloc_and_free() {
    auto& pmm = xinim::mm::PhysicalMemoryManager::instance();
    CHECK(pmm.initialize(64ULL << 20, 1ULL << 20, 2ULL << 20));
    pmm.add_memory_region(16ULL << 20, 16ULL << 20, false);

    const std::uint64_t free_before = pmm.get_free_memory();
    CHECK(free_before > 0);

    const std::uint64_t page = pmm.alloc_page(xinim::mm::MemoryZone::NORMAL);
    CHECK(page != 0);
    CHECK(pmm.get_zone_for_address(page) == xinim::mm::MemoryZone::NORMAL);

    auto* frame = pmm.get_page_frame(page);
    CHECK(frame != nullptr);
    CHECK(frame->flags == xinim::mm::PageFrame::FLAG_ALLOCATED);
    CHECK(frame->ref_count == 1);

    pmm.ref_page(page);
    CHECK(frame->ref_count == 2);
    pmm.unref_page(page);
    CHECK(frame->ref_count == 1);

    pmm.free_page(page);
    CHECK(frame->flags == xinim::mm::PageFrame::FLAG_FREE);
    CHECK(frame->ref_count == 0);
    CHECK(pmm.get_free_memory() == free_before);
    return 0;
}

int main() {
    if (test_zone_alloc_and_free() != 0) {
        return 1;
    }
    std::printf("PASS: pmm zone alloc/free\n");
    return 0;
}
