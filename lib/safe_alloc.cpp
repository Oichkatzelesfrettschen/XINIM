#include "../include/lib.hpp" // C++17 header
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Allocate memory and abort on failure.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory.
 */
void *safe_malloc(size_t size) noexcept {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return ptr;
}

/**
 * @brief Free memory if pointer not @c nullptr.
 *
 * @param ptr Pointer previously allocated with ::safe_malloc.
 */
void safe_free(void *ptr) noexcept {
    if (ptr != NULL) {
        free(ptr);
    }
}
