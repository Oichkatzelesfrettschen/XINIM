#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

static void write_str(const char *s) { write(1, s, strlen(s)); }
static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond) {
    if (cond) { write_str("  PASS: "); ++g_pass; }
    else { write_str("  FAIL: "); ++g_fail; }
    write_str(name);
    write_str("\n");
}

int main(void) {
    write_str("=== ext2 Mutation Stress Test ===\n");

    /* Test 1: Create file, write, read back */
    {
        int fd = open("/tmp/stress1", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        check("create /tmp/stress1", fd >= 0);
        if (fd >= 0) {
            write(fd, "test data 123", 13);
            close(fd);
            fd = open("/tmp/stress1", O_RDONLY, 0);
            char buf[32] = {0};
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            check("read back matches", n == 13 && strncmp(buf, "test data 123", 13) == 0);
        }
    }

    /* Test 2: Rename file */
    {
        int r = rename("/tmp/stress1", "/tmp/stress2");
        check("rename", r == 0);
        struct stat st;
        check("old name gone", stat("/tmp/stress1", &st) != 0);
        check("new name exists", stat("/tmp/stress2", &st) == 0);
    }

    /* Test 3: Unlink file */
    {
        int r = unlink("/tmp/stress2");
        check("unlink", r == 0);
        struct stat st;
        check("unlinked file gone", stat("/tmp/stress2", &st) != 0);
    }

    /* Test 4: mkdir + rmdir cycle */
    {
        int r = mkdir("/tmp/stressdir", 0755);
        check("mkdir", r == 0);
        struct stat st;
        check("dir exists", stat("/tmp/stressdir", &st) == 0);
        r = rmdir("/tmp/stressdir");
        check("rmdir", r == 0);
        check("dir gone", stat("/tmp/stressdir", &st) != 0);
    }

    /* Test 5: Multiple file create/delete cycle */
    {
        int ok = 1;
        for (int i = 0; i < 5; ++i) {
            char path[32] = "/tmp/cyc_";
            path[9] = (char)('0' + i);
            path[10] = '\0';
            int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { ok = 0; break; }
            write(fd, "x", 1);
            close(fd);
        }
        check("create 5 files", ok);
        ok = 1;
        for (int i = 0; i < 5; ++i) {
            char path[32] = "/tmp/cyc_";
            path[9] = (char)('0' + i);
            path[10] = '\0';
            if (unlink(path) != 0) { ok = 0; break; }
        }
        check("unlink 5 files", ok);
    }

    /* Test 6: Append write */
    {
        int fd = open("/tmp/append_test", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        write(fd, "first", 5);
        close(fd);
        fd = open("/tmp/append_test", O_WRONLY | O_APPEND, 0);
        write(fd, "second", 6);
        close(fd);
        fd = open("/tmp/append_test", O_RDONLY, 0);
        char buf[32] = {0};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        check("append write", n == 11 && strncmp(buf, "firstsecond", 11) == 0);
        unlink("/tmp/append_test");
    }

    char num[4];
    write_str("=== ");
    num[0] = (char)('0' + g_pass / 10); num[1] = (char)('0' + g_pass % 10); num[2] = '\0';
    if (g_pass < 10) { num[0] = num[1]; num[1] = '\0'; }
    write_str(num);
    write_str(" passed, ");
    num[0] = (char)('0' + g_fail); num[1] = '\0';
    write_str(num);
    write_str(" failed ===\n");
    return g_fail > 0 ? 1 : 0;
}
