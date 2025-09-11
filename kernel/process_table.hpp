#pragma once

/**
 * @file process_table.hpp
 * @brief Lightweight process table abstraction for the kernel.
 */

#include "platform_traits.hpp"

/**
 * @class ProcessTable
 * @brief Singleton wrapper around the kernel's process descriptors.
 *
 * This is a provisional interface used for build and unit-test
 * scaffolding. It exposes a minimal API while the traditional global
 * structures are gradually refactored.
 */
class ProcessTable {
  public:
    /** Obtain the global instance. */
    static ProcessTable &instance() noexcept {
        static ProcessTable table{};
        return table;
    }

    /**
     * Initialize the table during boot.
     *
     * @param mm_base Starting click for the memory manager.
     */
    void initialize_all(PlatformTraits::phys_clicks_t mm_base) noexcept {
        (void)mm_base;
        // Implementation pending: allocate structures and set up
        // initial processes. Placeholder to satisfy the build.
    }

  private:
    ProcessTable() = default;
};
