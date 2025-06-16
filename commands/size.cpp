/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* size - tell size of an object file		Author: Andy Tanenbaum */

#define HLONG 8 /* # longs in the header */
#define TEXT 2
#define DATA 3
#define BSS 4
#define CHMEM 6
#define MAGIC 0x0301      /* magic number for an object file */
#define SEPBIT 0x00200000 /* this bit is set for separate I/D */

int heading; /* set when heading printed */
int error;

// Entry point using modern parameters
int main(int argc, char *argv[]) {
    int i;

    if (argc == 1) {
        size("a.out");
        return error;
    }

    for (i = 1; i < argc; i++)
        size(argv[i]);
    return error;
}

static void size(const char *name) {
    int fd, separate;
    long head[HLONG], dynam, allmem;

    if ((fd = open(name, 0)) < 0) {
        stderr3("size: can't open ", name, "\n");
        return;
    }

    if (read(fd, head, sizeof(head)) != sizeof(head)) {
        stderr3("size: ", name, ": header too short\n");
        error = 1;
        close(fd);
        return;
    }

    if ((head[0] & 0xFFFF) != MAGIC) {
        stderr3("size: ", name, " not an object file\n");
        close(fd);
        return;
    }

    separate = (head[0] & SEPBIT ? 1 : 0);
    dynam = head[CHMEM] - head[TEXT] - head[DATA] - head[BSS];
    if (separate)
        dynam += head[TEXT];
    allmem = (separate ? head[CHMEM] + head[TEXT] : head[CHMEM]);
    if (heading++ == 0)
        prints("  text\t  data\t   bss\t stack\tmemory\n");
    printf("%6D\t%6D\t%6D\t%6D\t%6D\t%s\n", head[TEXT], head[DATA], head[BSS], dynam, allmem, name);
    close(fd);
}

static void stderr3(const char *s1, const char *s2, const char *s3) {
    std_err(s1);
    std_err(s2);
    std_err(s3);
    error = 1;
}
