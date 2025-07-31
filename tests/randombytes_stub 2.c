#include "randombytes.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Fill a buffer with pseudo random bytes for testing.
 */
void randombytes(uint8_t *out, size_t outlen) {
    for (size_t i = 0; i < outlen; ++i) {
        out[i] = (uint8_t)(rand() % 256);
    }
}
