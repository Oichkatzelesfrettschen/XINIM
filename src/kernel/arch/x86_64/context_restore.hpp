#pragma once

#include "context.hpp"

extern "C" {

[[noreturn]] void load_context(xinim::kernel::CpuContext *context);
[[noreturn]] void load_context_ring3(xinim::kernel::CpuContext *context);
}
