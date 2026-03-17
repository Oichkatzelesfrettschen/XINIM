#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* Minimal patch: apply unified diff format (-u) */

static char g_patch[65536];
static char g_file[65536];
static char g_output[65536];

static int read_all(int fd, char *buf, int bufsz) {
    int total = 0;
    ssize_t n;
    while (total < bufsz - 1 && (n = read(fd, buf + total, bufsz - 1 - total)) > 0)
        total += (int)n;
    buf[total] = '\0';
    return total;
}

static int write_all(const char *path, const char *data, int len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    write(fd, data, len);
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: patch < patchfile\n  or:  patch -i patchfile\n", 50); return 1; }

    const char *patch_file = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) patch_file = argv[++i];
        else patch_file = argv[i];
    }

    int pfd = 0;
    if (patch_file) {
        pfd = open(patch_file, O_RDONLY, 0);
        if (pfd < 0) { write(2, "patch: cannot open patch file\n", 30); return 1; }
    }
    int patch_len = read_all(pfd, g_patch, sizeof(g_patch));
    if (pfd > 0) close(pfd);

    /* Parse unified diff: find --- and +++ lines for filename */
    char target_file[256] = {0};
    const char *p = g_patch;
    while (*p) {
        if (strncmp(p, "+++ ", 4) == 0) {
            p += 4;
            if (*p == 'b' && p[1] == '/') p += 2; /* strip b/ prefix */
            int len = 0;
            while (*p && *p != '\n' && *p != '\t' && len < 255) target_file[len++] = *p++;
            target_file[len] = '\0';
            break;
        }
        while (*p && *p != '\n') ++p;
        if (*p) ++p;
    }

    if (target_file[0] == '\0') {
        write(2, "patch: no target file found in patch\n", 37);
        return 1;
    }

    /* Read target file */
    int tfd = open(target_file, O_RDONLY, 0);
    int file_len = 0;
    if (tfd >= 0) {
        file_len = read_all(tfd, g_file, sizeof(g_file));
        close(tfd);
    }

    /* Split file into lines */
    char *file_lines[4096];
    int file_line_count = 0;
    {
        char *fp = g_file;
        while (*fp && file_line_count < 4096) {
            file_lines[file_line_count++] = fp;
            while (*fp && *fp != '\n') ++fp;
            if (*fp) *fp++ = '\0';
        }
    }

    /* Apply hunks -- simplified: just copy lines, applying +/- changes */
    int out_len = 0;
    int file_line = 0;
    p = g_patch;

    /* Skip to first @@ */
    while (*p) {
        if (*p == '@' && p[1] == '@') break;
        while (*p && *p != '\n') ++p;
        if (*p) ++p;
    }

    while (*p) {
        if (*p == '@' && p[1] == '@') {
            /* Parse hunk header: @@ -old_start,old_count +new_start,new_count @@ */
            p += 4;
            int old_start = 0;
            while (*p >= '0' && *p <= '9') old_start = old_start * 10 + (*p++ - '0');
            --old_start; /* Convert to 0-based */
            while (*p && *p != '\n') ++p;
            if (*p) ++p;

            /* Copy unchanged lines before hunk */
            while (file_line < old_start && file_line < file_line_count) {
                int len = (int)strlen(file_lines[file_line]);
                if (out_len + len + 1 < (int)sizeof(g_output)) {
                    memcpy(g_output + out_len, file_lines[file_line], len);
                    out_len += len;
                    g_output[out_len++] = '\n';
                }
                ++file_line;
            }

            /* Process hunk lines */
            while (*p && *p != '@') {
                if (*p == ' ') {
                    /* Context line: copy and advance */
                    ++p;
                    while (*p && *p != '\n') {
                        if (out_len < (int)sizeof(g_output) - 1) g_output[out_len++] = *p;
                        ++p;
                    }
                    if (out_len < (int)sizeof(g_output) - 1) g_output[out_len++] = '\n';
                    if (*p) ++p;
                    ++file_line;
                } else if (*p == '-') {
                    /* Removed line: skip in original */
                    while (*p && *p != '\n') ++p;
                    if (*p) ++p;
                    ++file_line;
                } else if (*p == '+') {
                    /* Added line: output */
                    ++p;
                    while (*p && *p != '\n') {
                        if (out_len < (int)sizeof(g_output) - 1) g_output[out_len++] = *p;
                        ++p;
                    }
                    if (out_len < (int)sizeof(g_output) - 1) g_output[out_len++] = '\n';
                    if (*p) ++p;
                } else {
                    while (*p && *p != '\n') ++p;
                    if (*p) ++p;
                }
            }
        } else {
            while (*p && *p != '\n') ++p;
            if (*p) ++p;
        }
    }

    /* Copy remaining lines */
    while (file_line < file_line_count) {
        int len = (int)strlen(file_lines[file_line]);
        if (out_len + len + 1 < (int)sizeof(g_output)) {
            memcpy(g_output + out_len, file_lines[file_line], len);
            out_len += len;
            g_output[out_len++] = '\n';
        }
        ++file_line;
    }

    write(1, "patching file ", 14);
    write(1, target_file, strlen(target_file));
    write(1, "\n", 1);
    write_all(target_file, g_output, out_len);
    return 0;
}
