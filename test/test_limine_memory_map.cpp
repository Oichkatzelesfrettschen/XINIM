#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limine.h>
#include <xinim/boot/limine_shim.hpp>

namespace {

    int failures = 0;

    void check(bool condition, const char *expression, int line) {
        if (!condition) {
            std::fprintf(stderr, "FAIL:%d: %s\n", line, expression);
            ++failures;
        }
    }

#define CHECK(expression) check((expression), #expression, __LINE__)

    void test_type_translation() {
        struct TypeCase {
            uint64_t limine_type;
            uint32_t xinim_type;
        };
        constexpr TypeCase type_cases[] = {
            {LIMINE_MEMMAP_USABLE, xinim::boot::MEMORY_RANGE_USABLE},
            {LIMINE_MEMMAP_RESERVED, xinim::boot::MEMORY_RANGE_RESERVED},
            {LIMINE_MEMMAP_ACPI_RECLAIMABLE, xinim::boot::MEMORY_RANGE_ACPI_RECLAIMABLE},
            {LIMINE_MEMMAP_ACPI_NVS, xinim::boot::MEMORY_RANGE_ACPI_NVS},
            {LIMINE_MEMMAP_BAD_MEMORY, xinim::boot::MEMORY_RANGE_BAD},
            {LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE,
             xinim::boot::MEMORY_RANGE_BOOTLOADER_RECLAIMABLE},
            {LIMINE_MEMMAP_EXECUTABLE_AND_MODULES, xinim::boot::MEMORY_RANGE_KERNEL_AND_MODULES},
            {LIMINE_MEMMAP_FRAMEBUFFER, xinim::boot::MEMORY_RANGE_FRAMEBUFFER},
            {LIMINE_MEMMAP_ACPI_TABLES, xinim::boot::MEMORY_RANGE_ACPI_TABLES},
        };

        for (const TypeCase &type_case : type_cases) {
            CHECK(xinim::boot::translate_limine_memory_type(type_case.limine_type) ==
                  type_case.xinim_type);
        }
        CHECK(xinim::boot::translate_limine_memory_type(UINT64_MAX) ==
              xinim::boot::MEMORY_RANGE_RESERVED);
    }

    void test_valid_and_empty_maps() {
        limine_memmap_entry first_entry{0x1000U, 0x3000U, LIMINE_MEMMAP_USABLE};
        limine_memmap_entry second_entry{0x4000U, 0x1000U, LIMINE_MEMMAP_RESERVED};
        limine_memmap_entry *entry_pointers[]{&first_entry, &second_entry};
        limine_memmap_response response{0U, 2U, entry_pointers};
        xinim::boot::MemRange output[2]{};
        size_t output_count = 99U;

        CHECK(xinim::boot::normalize_limine_memory_map(&response, output, 2U, output_count));
        CHECK(output_count == 2U);
        CHECK(output[0].base == first_entry.base);
        CHECK(output[0].length == first_entry.length);
        CHECK(output[0].type == xinim::boot::MEMORY_RANGE_USABLE);
        CHECK(output[1].type == xinim::boot::MEMORY_RANGE_RESERVED);

        limine_memmap_response empty_response{0U, 0U, nullptr};
        output_count = 99U;
        CHECK(xinim::boot::normalize_limine_memory_map(&empty_response, output, 2U, output_count));
        CHECK(output_count == 0U);
    }

    void test_malformed_maps_are_rejected_atomically() {
        limine_memmap_entry entry{0x1000U, 0x1000U, LIMINE_MEMMAP_USABLE};
        limine_memmap_entry *null_entry_pointers[]{&entry, nullptr};
        limine_memmap_response null_entries_response{0U, 1U, nullptr};
        limine_memmap_response null_entry_response{0U, 2U, null_entry_pointers};
        xinim::boot::MemRange output[2]{};
        size_t output_count = 99U;

        CHECK(!xinim::boot::normalize_limine_memory_map(nullptr, output, 2U, output_count));
        CHECK(output_count == 0U);
        output_count = 99U;
        CHECK(!xinim::boot::normalize_limine_memory_map(&null_entries_response, output, 2U,
                                                        output_count));
        CHECK(output_count == 0U);
        output_count = 99U;
        CHECK(!xinim::boot::normalize_limine_memory_map(&null_entry_response, output, 2U,
                                                        output_count));
        CHECK(output_count == 0U);
        output_count = 99U;
        CHECK(!xinim::boot::normalize_limine_memory_map(&null_entry_response, nullptr, 2U,
                                                        output_count));
        CHECK(output_count == 0U);
        output_count = 99U;
        CHECK(!xinim::boot::normalize_limine_memory_map(&null_entry_response, output, 0U,
                                                        output_count));
        CHECK(output_count == 0U);
    }

    void test_capacity_boundary() {
        limine_memmap_entry entries[xinim::boot::kLimineMemoryRangeCapacity]{};
        limine_memmap_entry *entry_pointers[xinim::boot::kLimineMemoryRangeCapacity]{};
        xinim::boot::MemRange output[xinim::boot::kLimineMemoryRangeCapacity]{};
        for (size_t index = 0U; index < xinim::boot::kLimineMemoryRangeCapacity; ++index) {
            entries[index] = limine_memmap_entry{index * 0x1000U, 0x1000U, LIMINE_MEMMAP_USABLE};
            entry_pointers[index] = &entries[index];
        }

        limine_memmap_response exact_response{0U, xinim::boot::kLimineMemoryRangeCapacity,
                                              entry_pointers};
        size_t output_count = 0U;
        CHECK(xinim::boot::normalize_limine_memory_map(
            &exact_response, output, xinim::boot::kLimineMemoryRangeCapacity, output_count));
        CHECK(output_count == xinim::boot::kLimineMemoryRangeCapacity);

        limine_memmap_response oversized_response{0U, xinim::boot::kLimineMemoryRangeCapacity + 1U,
                                                  entry_pointers};
        output_count = 99U;
        CHECK(!xinim::boot::normalize_limine_memory_map(
            &oversized_response, output, xinim::boot::kLimineMemoryRangeCapacity, output_count));
        CHECK(output_count == 0U);
    }

} // namespace

int main() {
    test_type_translation();
    test_valid_and_empty_maps();
    test_malformed_maps_are_rejected_atomically();
    test_capacity_boundary();
    return failures == 0 ? 0 : 1;
}
