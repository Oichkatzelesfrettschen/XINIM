#pragma once
// Shared kernel utility functions for the i486 ring3 subsystem.

#include "ring3_internal.hpp"

namespace xinim::i486::ring3 {

// -- String utilities ------------------------------------------------------

[[nodiscard]] bool string_equals(const char* lhs, const char* rhs) noexcept;
[[nodiscard]] uint32_t string_length(const char* text) noexcept;
void copy_c_string(char* destination, uint32_t capacity, const char* source) noexcept;

// -- Memory utilities ------------------------------------------------------

void zero_region(uint8_t* base, uint32_t size) noexcept;
void copy_region(uint8_t* out, const uint8_t* in, uint32_t size) noexcept;

// -- Alignment utilities ---------------------------------------------------

[[nodiscard]] uint32_t align_down(uint32_t value, uint32_t alignment) noexcept;
[[nodiscard]] uint32_t align_up(uint32_t value, uint32_t alignment) noexcept;

// -- fd_map utilities ------------------------------------------------------

[[nodiscard]] int resolve_fd(const Process* process, int fd) noexcept;
[[nodiscard]] int allocate_fd_map_entry(Process* process, int global_slot) noexcept;
[[nodiscard]] int allocate_fd_map_entry_at_or_above(Process* process,
                                                    int global_slot,
                                                    int minimum_fd) noexcept;
void reset_fd_map_to_console(Process* process) noexcept;

// -- User memory access ----------------------------------------------------

[[nodiscard]] bool translate_user_region(Process* process,
                                         uint32_t user_address,
                                         uint32_t size,
                                         uint8_t** out) noexcept;
[[nodiscard]] bool read_user_u32(Process* process,
                                 uint32_t user_address,
                                 uint32_t* out) noexcept;
[[nodiscard]] bool write_user_u32(Process* process,
                                  uint32_t user_address,
                                  uint32_t value) noexcept;
[[nodiscard]] bool write_user_bytes(Process* process,
                                    uint32_t user_address,
                                    const void* source,
                                    uint32_t size) noexcept;
[[nodiscard]] bool copy_user_string(Process* process,
                                    uint32_t user_address,
                                    char* buffer,
                                    uint32_t capacity) noexcept;

// -- Path resolution -------------------------------------------------------

bool resolve_path(const Process* process,
                  const char* input,
                  char* output,
                  uint32_t capacity) noexcept;
bool copy_and_resolve_user_path(Process* process,
                                uint32_t user_address,
                                char* buffer,
                                uint32_t capacity) noexcept;

// -- Context capture -------------------------------------------------------

[[nodiscard]] UserContext capture_user_context(RegisterFrame* frame) noexcept;

// -- ExecVector helpers (templates, defined inline) -------------------------

template <uint32_t Count>
[[nodiscard]] bool append_exec_string(ExecVector<Count>* vector,
                                      const char* text) noexcept {
    if (vector == nullptr || text == nullptr || vector->count >= Count) {
        return false;
    }
    const uint32_t length = string_length(text) + 1U;
    if (length > kMaxExecStringBytes - vector->used_bytes) {
        return false;
    }
    char* destination = vector->storage + vector->used_bytes;
    copy_region(reinterpret_cast<uint8_t*>(destination),
                reinterpret_cast<const uint8_t*>(text),
                length);
    vector->values[vector->count] = destination;
    ++vector->count;
    vector->used_bytes += length;
    return true;
}

template <uint32_t Count>
[[nodiscard]] bool copy_exec_vector(Process* process,
                                    uint32_t user_vector_address,
                                    ExecVector<Count>* out,
                                    const char* fallback0 = nullptr) noexcept {
    if (out == nullptr) {
        return false;
    }
    zero_region(reinterpret_cast<uint8_t*>(out), static_cast<uint32_t>(sizeof(*out)));

    if (user_vector_address == 0U) {
        if (fallback0 != nullptr) {
            return append_exec_string(out, fallback0);
        }
        return true;
    }

    for (uint32_t index = 0U; index < Count; ++index) {
        uint32_t element_address = 0U;
        if (!read_user_u32(process,
                           user_vector_address + (index * sizeof(uint32_t)),
                           &element_address)) {
            return false;
        }
        if (element_address == 0U) {
            return out->count != 0U || fallback0 == nullptr ||
                   append_exec_string(out, fallback0);
        }

        char buffer[128]{};
        if (!copy_user_string(process, element_address, buffer, sizeof(buffer)) ||
            !append_exec_string(out, buffer)) {
            return false;
        }
    }

    return true;
}

} // namespace xinim::i486::ring3
