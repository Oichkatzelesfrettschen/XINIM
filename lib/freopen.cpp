// clang-format off
#include "../include/stdio.hpp"
// clang-format on

// Forward declarations for stream functions.
FILE *fopen(const char *name, const char *mode);
int fclose(FILE *fp);

// Reopen an existing stream using a new file name and mode.
FILE *freopen(const char *name, const char *mode, FILE *stream) {
    // Close the current stream and release resources.
    if (fclose(stream) != 0) {
        return nullptr;
    }

    // Delegate to fopen to create the new stream instance.
    return fopen(name, mode);
}
