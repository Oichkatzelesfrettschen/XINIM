#pragma once

#include <cstdarg>
#include <cstddef>

namespace xinim::kernel::format {

    using CharacterSink = void (*)(void *context, char character) noexcept;

    /**
     * Format the kernel subset used by linked freestanding callers.
     *
     * Supported conversions are s, d, i, u, x, X, o, p, c, and percent. The
     * integer conversions accept l, ll, and z length modifiers. String and
     * character conversions accept minus alignment and decimal field width.
     * Integer and pointer conversions also accept zero padding. Percent accepts
     * no flags, width, or length. Unsupported grammar, width overflow, and a
     * result larger than INT_MAX return -1.
     */
    [[nodiscard]] int to_buffer(char *buffer, std::size_t capacity, const char *format,
                                va_list arguments) noexcept;

    [[nodiscard]] int to_sink(CharacterSink sink, void *context, const char *format,
                              va_list arguments) noexcept;

} // namespace xinim::kernel::format
