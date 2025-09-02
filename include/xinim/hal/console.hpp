#pragma once
#include <stddef.h>

namespace xinim::hal {

class Console {
  public:
    virtual ~Console() = default;
    virtual void write(const char* s, std::size_t n) = 0;
};

} // namespace xinim::hal
