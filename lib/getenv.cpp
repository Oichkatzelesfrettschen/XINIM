#include <cstddef>

// Retrieve the value of environment variable 'name'.
char *getenv(const char *name) {
    extern char **environ;
    char **v = environ;
    while (char *p = *v++) {
        const char *q = name;
        while (*p++ == *q)
            if (*q++ == 0)
                continue;
        if (*(p - 1) != '=')
            continue;
        return p;
    }
    return nullptr;
}
