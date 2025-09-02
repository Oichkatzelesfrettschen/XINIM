#pragma once

/**
 * @file idt.hpp
 * @brief Declarations for initializing the interrupt descriptor table.
 */

#ifdef __cplusplus
extern "C" {
#endif

void idt_init() noexcept;

#ifdef __cplusplus
}
#endif
