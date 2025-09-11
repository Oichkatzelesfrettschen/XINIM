#pragma once
#include <stdint.h>

namespace xinim::hal {

using IrqHandler = void(*)(void*);

class Interrupts {
  public:
    virtual ~Interrupts() = default;
    virtual void init() = 0;
    virtual int  allocate_vector() = 0;
    virtual void install(int vector, IrqHandler handler, void* ctx) = 0;
    virtual void mask(int vector) = 0;
    virtual void unmask(int vector) = 0;
    virtual void eoi(int vector) = 0;
};

} // namespace xinim::hal
