#pragma once
#include <stddef.h>
#include <stdint.h>

namespace xinim::hal {

enum class PageSize : uint32_t { Size4K = 4096, Size2M = 2 * 1024 * 1024, Size1G = 1024 * 1024 * 1024 };

struct MapFlags {
    bool read{true};
    bool write{false};
    bool exec{false};
    bool global{false};
    bool user{false};
    bool cache_disable{false};
};

class Mmu {
  public:
    virtual ~Mmu() = default;
    virtual void* create_address_space() = 0;
    virtual void  destroy_address_space(void* as) = 0;
    virtual bool  map(void* as, uintptr_t va, uintptr_t pa, PageSize sz, MapFlags fl) = 0;
    virtual bool  unmap(void* as, uintptr_t va, PageSize sz) = 0;
    virtual void  activate(void* as) = 0;
    virtual void  tlb_shootdown(uintptr_t va) = 0;
};

} // namespace xinim::hal
