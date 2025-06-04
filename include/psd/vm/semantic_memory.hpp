#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace psd::vm {

// Semantic domain tag types used with semantic_region.  Each tag
// represents a logical memory purpose and drives compile-time
// policy selection for the associated region.
// Executable code memory.
struct semantic_code_tag {};
// Read/write data memory.
struct semantic_data_tag {};
// Process stack memory which grows downward.
struct semantic_stack_tag {};
// Dynamically allocated heap memory.
struct semantic_heap_tag {};
// Memory used for IPC message buffers.
struct semantic_message_tag {};
// Matrix or vector data with alignment requirements.
struct semantic_matrix_tag {};

// Trait template describing properties of a semantic domain.  Each
// specialization encodes policies for memory protection and usage.
template <typename Tag> struct semantic_traits {
    static constexpr bool is_executable = false;        // region may contain code
    static constexpr bool is_shareable = false;         // region can be shared
    static constexpr bool is_zero_copy_capable = false; // zero-copy allowed
    static constexpr bool grows_down = false;           // stack-like growth
    static constexpr std::size_t alignment = 8;         // required byte alignment
};

// Code segment specialization.  Executable regions are shareable
// and may be used with zero-copy mapping for efficient IPC.
template <> struct semantic_traits<semantic_code_tag> {
    static constexpr bool is_executable = true;
    static constexpr bool is_shareable = true;
    static constexpr bool is_zero_copy_capable = true;
    static constexpr bool grows_down = false;
    static constexpr std::size_t alignment = 16;
};

// Stack specialization.  Stacks grow downward and are never
// executable or zero-copy capable.
template <> struct semantic_traits<semantic_stack_tag> {
    static constexpr bool is_executable = false;
    static constexpr bool is_shareable = false;
    static constexpr bool is_zero_copy_capable = false;
    static constexpr bool grows_down = true;
    static constexpr std::size_t alignment = 16;
};

// Message segment specialization used for IPC buffers.  Message
// regions must support zero-copy and are typically shared.
template <> struct semantic_traits<semantic_message_tag> {
    static constexpr bool is_executable = false;
    static constexpr bool is_shareable = true;
    static constexpr bool is_zero_copy_capable = true;
    static constexpr bool grows_down = false;
    static constexpr std::size_t alignment = 64;
};

// semantic_region aligns memory ranges at compile time
template <typename SemanticTag> class semantic_region {
  public:
    using tag_type = SemanticTag;
    using traits = semantic_traits<SemanticTag>;

    constexpr semantic_region(std::uintptr_t base, std::size_t size) noexcept
        : base_{align_to(base, traits::alignment)}, size_{size} {
        static_assert(traits::alignment > 0 && (traits::alignment & (traits::alignment - 1)) == 0,
                      "Alignment must be a power of two");
    }

    // Zero-copy mapping only for capable domains
    template <typename T = SemanticTag>
    std::enable_if_t<semantic_traits<T>::is_zero_copy_capable, void *>
    zero_copy_map() const noexcept {
        return reinterpret_cast<void *>(base_);
    }

    // Standard mapping available for all domains
    void *map() const noexcept { return reinterpret_cast<void *>(base_); }

    constexpr std::uintptr_t base() const noexcept { return base_; }
    constexpr std::size_t size() const noexcept { return size_; }

    // Return true if the base address satisfies the required alignment.
    constexpr bool aligned() const noexcept { return base_ % traits::alignment == 0; }

    // Check whether the region contains the provided address.
    constexpr bool contains(std::uintptr_t addr) const noexcept {
        return addr >= base_ && addr < base_ + size_;
    }

  private:
    // Utility rounding up "addr" to the specified power-of-two alignment.
    static constexpr std::uintptr_t align_to(std::uintptr_t addr, std::size_t alignment) noexcept {
        return (addr + alignment - 1) & ~(alignment - 1);
    }

    std::uintptr_t base_{}; // start address after alignment
    std::size_t size_{};    // size in bytes of the region
};

} // namespace psd::vm
