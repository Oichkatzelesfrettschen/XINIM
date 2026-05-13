#include <stddef.h>

extern "C" void* memset(void* destination, int value, size_t count) noexcept {
    auto* bytes = static_cast<unsigned char*>(destination);
    for (size_t index = 0U; index < count; ++index) {
        bytes[index] = static_cast<unsigned char>(value);
    }
    return destination;
}

extern "C" void* memcpy(void* destination, const void* source, size_t count) noexcept {
    auto* dst = static_cast<unsigned char*>(destination);
    const auto* src = static_cast<const unsigned char*>(source);
    for (size_t index = 0U; index < count; ++index) {
        dst[index] = src[index];
    }
    return destination;
}

extern "C" void* memmove(void* destination, const void* source, size_t count) noexcept {
    auto* dst = static_cast<unsigned char*>(destination);
    const auto* src = static_cast<const unsigned char*>(source);
    if (dst == src || count == 0U) {
        return destination;
    }
    if (dst < src || dst >= src + count) {
        for (size_t index = 0U; index < count; ++index) {
            dst[index] = src[index];
        }
    } else {
        for (size_t index = count; index > 0U; --index) {
            dst[index - 1U] = src[index - 1U];
        }
    }
    return destination;
}

extern "C" int memcmp(const void* lhs, const void* rhs, size_t count) noexcept {
    const auto* left = static_cast<const unsigned char*>(lhs);
    const auto* right = static_cast<const unsigned char*>(rhs);
    for (size_t index = 0U; index < count; ++index) {
        if (left[index] != right[index]) {
            return static_cast<int>(left[index]) - static_cast<int>(right[index]);
        }
    }
    return 0;
}
