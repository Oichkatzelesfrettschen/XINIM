// Modern C++17 implementation of the classic getenv routine.
#include <cstring>

// The environment is provided by the hosting process.
extern "C" char **environ;

// Retrieve the value of the environment variable ``name`` or ``nullptr``.
char *getenv(const char *name) {
    size_t len = std::strlen(name);
    for (char **env = environ; *env != nullptr; ++env) {
        const char *entry = *env;
        if (std::strncmp(entry, name, len) == 0 && entry[len] == '=') {
            // Cast away const for legacy compatibility.
            return const_cast<char *>(entry + len + 1);
        }
    }
    return nullptr;
}
