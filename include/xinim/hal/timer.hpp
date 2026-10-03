#pragma once
#include <stdint.h>

namespace xinim::hal {

class Timer {
  public:
    virtual ~Timer() = default;
    virtual void init() = 0;
    virtual std::uint64_t monotonic_ns() const = 0;
    virtual void oneshot_after_ns(std::uint64_t ns) = 0;
};

} // namespace xinim::hal
