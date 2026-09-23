#include "kutil.hpp"

namespace xinim::i486::ring3 {

// -- String utilities ------------------------------------------------------

bool string_equals(const char* lhs, const char* rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

uint32_t string_length(const char* text) noexcept {
    if (text == nullptr) {
        return 0U;
    }
    uint32_t length = 0U;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void copy_c_string(char* destination, uint32_t capacity, const char* source) noexcept {
    if (destination == nullptr || capacity == 0U) {
        return;
    }
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    uint32_t index = 0U;
    while (index + 1U < capacity && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

// -- Memory utilities ------------------------------------------------------

void zero_region(uint8_t* base, uint32_t size) noexcept {
    // Use rep stosd for dword-aligned bulk zeroing
    uint32_t dwords = size / 4U;
    uint8_t* ptr = base;
    if (dwords > 0U) {
        asm volatile("rep stosl"
                     : "+D"(ptr), "+c"(dwords)
                     : "a"(0U)
                     : "memory");
    }
    // Zero remaining tail bytes
    const uint32_t tail = size & 3U;
    for (uint32_t index = 0U; index < tail; ++index) {
        ptr[index] = 0U;
    }
}

void copy_region(uint8_t* out, const uint8_t* in, uint32_t size) noexcept {
    for (uint32_t index = 0U; index < size; ++index) {
        out[index] = in[index];
    }
}

// -- Alignment utilities ---------------------------------------------------

uint32_t align_down(uint32_t value, uint32_t alignment) noexcept {
    return value & ~(alignment - 1U);
}

uint32_t align_up(uint32_t value, uint32_t alignment) noexcept {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

// -- fd_map utilities ------------------------------------------------------

int resolve_fd(const Process* process, int fd) noexcept {
    if (process == nullptr || fd < 0 || fd >= kMaxFds) {
        return -1;
    }
    return process->fd_map[fd];
}

int allocate_fd_map_entry(Process* process, int global_slot) noexcept {
    return allocate_fd_map_entry_at_or_above(process, global_slot, 0);
}

int allocate_fd_map_entry_at_or_above(Process* process,
                                      int global_slot,
                                      int minimum_fd) noexcept {
    if (process == nullptr || global_slot < 0) {
        return -1;
    }
    if (minimum_fd < 0 || minimum_fd >= kMaxFds) {
        return -1;
    }
    for (int i = minimum_fd; i < kMaxFds; ++i) {
        if (process->fd_map[i] == -1) {
            process->fd_map[i] = global_slot;
            return i;
        }
    }
    return -1;
}

void reset_fd_map_to_console(Process* process) noexcept {
    if (process == nullptr) {
        return;
    }
    for (int fd_index = 0; fd_index < kMaxFds; ++fd_index) {
        const int slot = process->fd_map[fd_index];
        if (slot >= 0) {
            bootfs::decrement_slot_refcount(slot);
        }
        process->fd_map[fd_index] = -1;
        process->fd_flags[fd_index] = 0;
    }
    process->fd_map[0] = 0;
    process->fd_map[1] = 1;
    process->fd_map[2] = 2;
    bootfs::increment_slot_refcount(0);
    bootfs::increment_slot_refcount(1);
    bootfs::increment_slot_refcount(2);
}

// -- User memory access ----------------------------------------------------

bool translate_user_region(Process* process,
                           uint32_t user_address,
                           uint32_t size,
                           uint8_t** out) noexcept {
    if (process == nullptr || process->address_space == nullptr || out == nullptr) {
        return false;
    }
    if (user_address < elf32::kUserVirtualBase) {
        return false;
    }
    const uint32_t offset = user_address - elf32::kUserVirtualBase;
    if (offset > elf32::kUserAddressSpaceSize) {
        return false;
    }
    if (size > elf32::kUserAddressSpaceSize - offset) {
        return false;
    }
    *out = process->address_space + offset;
    return true;
}

bool read_user_u32(Process* process,
                   uint32_t user_address,
                   uint32_t* out) noexcept {
    if (out == nullptr) {
        return false;
    }
    uint8_t* raw = nullptr;
    if (!translate_user_region(process, user_address, sizeof(uint32_t), &raw)) {
        return false;
    }
    *out = *reinterpret_cast<uint32_t*>(raw);
    return true;
}

bool copy_user_string(Process* process,
                      uint32_t user_address,
                      char* buffer,
                      uint32_t capacity) noexcept {
    if (buffer == nullptr || capacity == 0U) {
        return false;
    }
    uint8_t* start = nullptr;
    if (!translate_user_region(process, user_address, 1U, &start)) {
        return false;
    }
    const uint32_t offset = user_address - elf32::kUserVirtualBase;
    const uint32_t remaining = elf32::kUserAddressSpaceSize - offset;
    for (uint32_t index = 0U; index < remaining && index + 1U < capacity; ++index) {
        buffer[index] = static_cast<char>(start[index]);
        if (buffer[index] == '\0') {
            return true;
        }
    }
    buffer[capacity - 1U] = '\0';
    return false;
}

bool write_user_bytes(Process* process,
                      uint32_t user_address,
                      const void* source,
                      uint32_t size) noexcept {
    uint8_t* destination = nullptr;
    if (!translate_user_region(process, user_address, size, &destination)) {
        return false;
    }
    if (size == 0U) {
        return true;
    }
    copy_region(destination, reinterpret_cast<const uint8_t*>(source), size);
    return true;
}

bool write_user_u32(Process* process,
                    uint32_t user_address,
                    uint32_t value) noexcept {
    uint8_t* raw = nullptr;
    if (!translate_user_region(process, user_address, sizeof(uint32_t), &raw)) {
        return false;
    }
    auto* out = reinterpret_cast<uint32_t*>(raw);
    *out = value;
    return true;
}

// -- Path resolution -------------------------------------------------------

bool resolve_path(const Process* process,
                  const char* input,
                  char* output,
                  uint32_t capacity) noexcept {
    if (process == nullptr || input == nullptr || output == nullptr || capacity == 0U) {
        return false;
    }
    if (input[0] == '/' || input[0] == '\0') {
        copy_c_string(output, capacity, input);
        return true;
    }
    const uint32_t cwd_len = string_length(process->cwd);
    const uint32_t input_len = string_length(input);
    const bool needs_slash = (cwd_len > 0U && process->cwd[cwd_len - 1U] != '/');
    const uint32_t total = cwd_len + (needs_slash ? 1U : 0U) + input_len + 1U;
    if (total > capacity) {
        return false;
    }
    uint32_t pos = 0U;
    for (uint32_t i = 0U; i < cwd_len; ++i) {
        output[pos++] = process->cwd[i];
    }
    if (needs_slash) {
        output[pos++] = '/';
    }
    for (uint32_t i = 0U; i < input_len; ++i) {
        output[pos++] = input[i];
    }
    output[pos] = '\0';
    return true;
}

bool copy_and_resolve_user_path(Process* process,
                                uint32_t user_address,
                                char* buffer,
                                uint32_t capacity) noexcept {
    char raw[256]{};
    if (!copy_user_string(process, user_address, raw, sizeof(raw))) {
        return false;
    }
    return resolve_path(process, raw, buffer, capacity);
}

// -- Context capture -------------------------------------------------------

UserContext capture_user_context(RegisterFrame* frame) noexcept {
    UserContext context{};
    if (frame == nullptr) {
        return context;
    }
    const auto* tail = reinterpret_cast<const uint32_t*>(
        reinterpret_cast<const uint8_t*>(frame) + sizeof(RegisterFrame));
    context.eax = frame->eax;
    context.ebx = frame->ebx;
    context.ecx = frame->ecx;
    context.edx = frame->edx;
    context.esi = frame->esi;
    context.edi = frame->edi;
    context.ebp = frame->ebp;
    context.ds = frame->ds;
    context.es = frame->es;
    context.fs = frame->fs;
    context.gs = frame->gs;
    context.eip = tail[0];
    context.cs = tail[1];
    context.eflags = tail[2];
    context.esp = tail[3];
    context.ss = tail[4];
    return context;
}

} // namespace xinim::i486::ring3
