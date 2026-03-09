#pragma once

#include <stdint.h>

namespace xinim::i486::console {

void initialize() noexcept;
void write_char(char c) noexcept;
void write_string(const char* text) noexcept;
void debug_write_char(char c) noexcept;
void debug_write_string(const char* text) noexcept;
char debug_read_char() noexcept;
void write_bool(bool value) noexcept;
void write_dec32(uint32_t value) noexcept;
void write_hex32(uint32_t value) noexcept;
void write_hex64(uint64_t value) noexcept;
void newline() noexcept;

} // namespace xinim::i486::console
