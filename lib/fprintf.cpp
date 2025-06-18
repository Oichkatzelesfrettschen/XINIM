// clang-format off
#include <cstdarg>  // va_list handling
#include <vector>   // dynamic buffer
#include "../include/stdio.hpp" // internal FILE definition

extern "C" int vsnprintf(char *, std::size_t, const char *, va_list);
// clang-format on

/**
 * @brief Write formatted data to the given file stream.
 *
 * @param file Target stream.
 * @param fmt  Format string.
 * @return Number of characters printed or -1 on error.
 */
int fprintf(FILE *file, const char *fmt, ...) {
    // Capture the variable arguments.
    va_list args;
    va_start(args, fmt);

    // Determine the length of the formatted output.
    va_list args_copy;
    va_copy(args_copy, args);
    int count = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);
    if (count < 0) {
        va_end(args);
        return -1; // formatting failed
    }

    // Allocate a buffer large enough for the formatted string.
    std::vector<char> buffer(static_cast<size_t>(count) + 1);
    vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);

    // Output the formatted bytes to the target stream.
    for (int i = 0; i < count; ++i)
        putc(buffer[i], file);

    // Optionally flush if the stream is set to auto-flush.
    if (testflag(file, PERPRINTF))
        fflush(file);

    return count;
}

/**
 * @brief Print formatted output to the standard output stream.
 *
 * @param fmt Format string.
 * @return Number of characters printed or -1 on error.
 */
int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int count = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);
    if (count < 0) {
        va_end(args);
        return -1;
    }

    std::vector<char> buffer(static_cast<size_t>(count) + 1);
    vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);

    for (int i = 0; i < count; ++i)
        putc(buffer[i], stdout);

    if (testflag(stdout, PERPRINTF))
        fflush(stdout);

    return count;
}
