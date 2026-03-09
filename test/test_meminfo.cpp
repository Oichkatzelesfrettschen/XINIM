#include <xinim/mm/dma_allocator.hpp>
#include <xinim/mm/meminfo.hpp>
#include <xinim/mm/pmm.hpp>

#include <cstdio>
#include <string>

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1; \
        } \
    } while (0)

static int test_format_bytes() {
    CHECK(xinim::mm::MemInfo::format_bytes(512) == std::string("512 bytes"));
    CHECK(xinim::mm::MemInfo::format_bytes(2048).find("KB") != std::string::npos);
    CHECK(xinim::mm::MemInfo::format_bytes(5ULL << 20).find("MB") != std::string::npos);
    return 0;
}

static int test_get_stats() {
    auto& pmm = xinim::mm::PhysicalMemoryManager::instance();
    CHECK(pmm.initialize(64ULL << 20, 1ULL << 20, 2ULL << 20));
    pmm.add_memory_region(16ULL << 20, 16ULL << 20, false);

    CHECK(xinim::mm::DMAAllocator::initialize());

    const auto stats = xinim::mm::MemInfo::get_stats();
    CHECK(stats.total_memory == (64ULL << 20));
    CHECK(stats.free_memory > 0);
    CHECK(stats.dma_zone_total > 0);
    CHECK(stats.normal_zone_total > 0);
    CHECK(stats.fragmentation_ratio >= 0.0f);
    CHECK(stats.fragmentation_ratio <= 1.0f);
    CHECK(stats.largest_free_block > 0);
    CHECK(stats.dma_allocated == 0);

    xinim::mm::DMAAllocator::shutdown();
    return 0;
}

static void run_test(const char* name, int (*fn)(), int& passed, int& failed) {
    if (fn() == 0) {
        std::printf("PASS: %s\n", name);
        ++passed;
    } else {
        std::printf("FAIL: %s\n", name);
        ++failed;
    }
}

int main() {
    int passed = 0;
    int failed = 0;
    run_test("format_bytes", test_format_bytes, passed, failed);
    run_test("get_stats", test_get_stats, passed, failed);
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
