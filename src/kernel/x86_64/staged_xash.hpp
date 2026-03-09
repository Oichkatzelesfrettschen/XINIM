#pragma once

namespace xinim::boot {
struct BootInfo;
}

namespace xinim::kernel::x86_64 {

void set_staged_xash_boot_info(const xinim::boot::BootInfo* boot_info) noexcept;
[[gnu::no_stack_protector]] [[noreturn]] void run_staged_xash_init() noexcept;

} // namespace xinim::kernel::x86_64
