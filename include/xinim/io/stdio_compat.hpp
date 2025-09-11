#pragma once

/**
 * @file stdio_compat.hpp
 * @brief C stdio compatibility shims implemented via `std::fstream` and
 *        `std::span`.
 */

#include "../../stdio.hpp"
#include <cstddef>
#include <span>

namespace minix::io::compat {

extern "C" {
/**
 * @brief Open a file using `std::fstream` semantics.
 *
 * @param path Path to the file.
 * @param mode Mode string compatible with `fopen`.
 * @return Opaque `FILE` handle or `nullptr` on failure.
 */
FILE *fopen_compat(const char *path, const char *mode);

/**
 * @brief Close a file previously opened with `fopen_compat`.
 *
 * @param fp Handle returned by `fopen_compat`.
 * @return `0` on success, `EOF` on error.
 */
int fclose_compat(FILE *fp);

/**
 * @brief Read a sequence of objects from a file.
 *
 * @param ptr   Destination buffer.
 * @param size  Size of each object.
 * @param nmemb Number of objects to read.
 * @param fp    Handle returned by `fopen_compat`.
 * @return Number of objects successfully read.
 */
size_t fread_compat(void *ptr, size_t size, size_t nmemb, FILE *fp);

/**
 * @brief Write a sequence of objects to a file.
 *
 * @param ptr   Source buffer.
 * @param size  Size of each object.
 * @param nmemb Number of objects to write.
 * @param fp    Handle returned by `fopen_compat`.
 * @return Number of objects successfully written.
 */
size_t fwrite_compat(const void *ptr, size_t size, size_t nmemb, FILE *fp);

/**
 * @brief Print formatted data to a file.
 *
 * @param fp     Handle returned by `fopen_compat`.
 * @param format `printf`-style format string.
 * @return Number of characters written or negative on error.
 */
int fprintf_compat(FILE *fp, const char *format, ...);
}

} // namespace minix::io::compat
