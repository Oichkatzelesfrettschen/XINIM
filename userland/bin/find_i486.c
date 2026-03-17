#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

/* Minimal find: find PATH [-name PATTERN] [-type f|d] */

static int g_type_filter; /* 0=any, 'f'=file, 'd'=dir */
static const char *g_name_pattern;

static int match_name(const char *name, const char *pattern) {
    /* Simple glob: * matches anything, ? matches one char, else literal */
    while (*pattern) {
        if (*pattern == '*') {
            ++pattern;
            if (*pattern == '\0') return 1;
            while (*name) { if (match_name(name, pattern)) return 1; ++name; }
            return 0;
        } else if (*pattern == '?') {
            if (*name == '\0') return 0;
            ++pattern; ++name;
        } else {
            if (*name != *pattern) return 0;
            ++pattern; ++name;
        }
    }
    return *name == '\0';
}

static const char *basename_of(const char *path) {
    const char *p = path;
    const char *last = path;
    while (*p) { if (*p == '/' && p[1] != '\0') last = p + 1; ++p; }
    return last;
}

static void find_recursive(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return;

    int is_dir = ((st.st_mode & 0170000) == 0040000);
    int is_file = ((st.st_mode & 0170000) == 0100000);

    /* Check filters */
    int show = 1;
    if (g_type_filter == 'f' && !is_file) show = 0;
    if (g_type_filter == 'd' && !is_dir) show = 0;
    if (g_name_pattern && !match_name(basename_of(path), g_name_pattern)) show = 0;

    if (show) {
        write(1, path, strlen(path));
        write(1, "\n", 1);
    }

    if (!is_dir) return;

    DIR *dir = opendir(path);
    if (dir == 0) return;

    struct dirent *de;
    while ((de = readdir(dir)) != 0) {
        const char *name = de->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
            continue;

        char child[512];
        size_t plen = strlen(path);
        size_t nlen = strlen(name);
        if (plen + 1 + nlen + 1 > sizeof(child)) continue;
        memcpy(child, path, plen);
        if (plen > 0 && path[plen - 1] != '/') child[plen++] = '/';
        memcpy(child + plen, name, nlen + 1);

        find_recursive(child);
    }

    closedir(dir);
}

int main(int argc, char **argv) {
    const char *start_path = ".";
    int argi = 1;

    if (argi < argc && argv[argi][0] != '-') {
        start_path = argv[argi++];
    }

    while (argi < argc) {
        if (strcmp(argv[argi], "-name") == 0 && argi + 1 < argc) {
            g_name_pattern = argv[++argi];
        } else if (strcmp(argv[argi], "-type") == 0 && argi + 1 < argc) {
            g_type_filter = argv[++argi][0];
        }
        ++argi;
    }

    find_recursive(start_path);
    return 0;
}
