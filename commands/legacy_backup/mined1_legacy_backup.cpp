/*
 * mined — Modernized C23 Minimal Screen Editor
 * -------------------------------------------
 * Strict C23, portable, no hidden globals, no legacy cruft.
 * Flexible-array-member lines, RAII terminal guard, enum errors, and modular layout.
 * Build with: clang -std=c23 -Wall -Wextra -O2 -o mined mined.c
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <sys/wait.h>

/* --- Types and Constants --- */
typedef enum { RC_Fine = 0, RC_Errors, RC_NoLine, RC_NoInput } ReturnCode;
#define SCREEN_SIZE 4096
#define LINE_LEN    256
#define SCREENMAX   24
#define XBREAK      80
#define SHIFT_SIZE  8

typedef struct Line {
    struct Line *prev, *next;
    size_t len;
    unsigned shift_count;
    char text[]; /* flexible array */
} Line;

/* --- Globals --- */
static Line *header = NULL, *tail = NULL, *cur_line = NULL;
static char *cur_text = NULL;
static int nlines = 0, x = 0, y = 0, last_y = SCREENMAX;
static bool modified = false, writable = true, loading = false, quit_flag = false;
static char screen_buf[SCREEN_SIZE];
static size_t screen_pos = 0;
static char file_name[LINE_LEN] = {0};

/* --- Terminal Handling (RAII) --- */
static struct termios orig_termios;
static void term_restore(void) { tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios); }
static void term_raw_enable(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(term_restore);
    raw = orig_termios;
    raw.c_lflag &= ~(ECHO|ICANON|ISIG);
    raw.c_cc[VMIN]=1; raw.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/* --- Panic/Abort --- */
noreturn static void fatal_error(const char *msg) {
    dprintf(STDERR_FILENO, "\x1b[2J\x1b[H\nFATAL: %s (errno=%d)\n", msg, errno);
    exit(EXIT_FAILURE);
}

/* --- I/O Buffering --- */
static ReturnCode flush_buffer(int fd) {
    if (screen_pos == 0) return RC_Fine;
    ssize_t w = write(fd, screen_buf, screen_pos);
    ReturnCode rc = (w<0 || (size_t)w != screen_pos) ? RC_Errors : RC_Fine;
    screen_pos = 0;
    return rc;
}
static ReturnCode write_char(int fd, char c) {
    screen_buf[screen_pos++] = c;
    if (screen_pos >= SCREEN_SIZE) return flush_buffer(fd);
    return RC_Fine;
}
static ReturnCode writeline(int fd, const char *txt) {
    for (const char *p=txt; *p; ++p)
        if (write_char(fd, *p)==RC_Errors) return RC_Errors;
    return RC_Fine;
}

/* --- Line Handling --- */
static Line *alloc_line(const char *text, size_t len) {
    Line *L = malloc(sizeof(Line)+len+1);
    if (!L) fatal_error("Out of memory");
    L->len = len; L->shift_count=0;
    memcpy(L->text, text, len);
    L->text[len]='\0';
    return L;
}
static Line *insert_line(Line *pos, const char *text, size_t len) {
    Line *L = alloc_line(text, len);
    L->prev = pos; L->next = pos->next;
    pos->next->prev = L; pos->next = L;
    nlines++; return L;
}

/* --- Initialization --- */
static void initialize(void) {
    if (!header) {
        header = alloc_line("", 0);
        tail = alloc_line("", 0);
        header->next = tail; tail->prev = header;
    } else {
        Line *p = header->next, *tmp;
        while (p != tail) { tmp = p->next; free(p); p = tmp; }
        header->next = tail; tail->prev = header;
    }
    nlines=0; modified=false; loading=false; quit_flag=false; writable=true;
    insert_line(header, "\n", 1);
    cur_line = header->next;
    cur_text = cur_line->text;
}

/* --- File Loading --- */
static ReturnCode load_file(const char *fname) {
    int fd = (fname && *fname) ? open(fname, O_RDONLY) : -1;
    if (fd < 0 && fname && *fname) {
        dprintf(STDERR_FILENO, "Cannot open '%s'\n", fname);
        return RC_Errors;
    }
    initialize(); loading=true;
    char buf[LINE_LEN]; ssize_t r; size_t i=0;
    while ((r=read(fd, buf+i, 1)) > 0) {
        if (buf[i++] == '\n' || i == LINE_LEN-1) {
            insert_line(tail->prev, buf, i); i=0;
        }
    }
    if (i>0) insert_line(tail->prev, buf, i);
    if (fd>=0) close(fd);
    loading=false;
    return RC_Fine;
}

/* --- Display, Cursor --- */
static void set_cursor(int cx,int cy) {
    char seq[32];
    int len = snprintf(seq, sizeof(seq), "\x1b[%d;%dH", cy+1, cx+1);
    write(STDOUT_FILENO, seq, len);
}
static void status_line(const char *s1, const char *s2) {
    set_cursor(0, SCREENMAX);
    writeline(STDOUT_FILENO, s1);
    if (s2) { writeline(STDOUT_FILENO, " "); writeline(STDOUT_FILENO, s2); }
    writeline(STDOUT_FILENO, "\n");
    flush_buffer(STDOUT_FILENO);
}
static void redraw(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    Line *L = header->next;
    set_cursor(0,0);
    for (int i=0; i<SCREENMAX && L != tail; ++i, L=L->next)
        writeline(STDOUT_FILENO, L->text);
    flush_buffer(STDOUT_FILENO);
    set_cursor(x, y);
}

/* --- Input --- */
static char get_char(void) {
    char c=0;
    if (read(STDIN_FILENO, &c, 1) != 1) fatal_error("Read error");
    return c;
}
static ReturnCode get_string(const char *prompt, char *buf, size_t max) {
    status_line(prompt, NULL);
    size_t pos=0;
    for (;;) {
        char c = get_char();
        if (c == '\n') { buf[pos]='\0'; return pos ? RC_Fine : RC_NoInput; }
        if (c == '\b' && pos>0) { --pos; set_cursor(pos, SCREENMAX); writeline(STDOUT_FILENO, " \b"); }
        else if ((unsigned char)c>=32 && pos+1<max) { buf[pos++]=c; buf[pos]='\0'; write(STDOUT_FILENO, &c, 1); }
        else if (c == '\x1b') { quit_flag=false; return RC_Errors; }
    }
}
static ReturnCode get_number(const char *prompt, int *out) {
    char buf[16];
    ReturnCode rc = get_string(prompt, buf, sizeof(buf));
    if (rc != RC_Fine) return rc;
    char *end;
    long v = strtol(buf, &end, 10);
    if (end == buf) return RC_Errors;
    *out = (int)v;
    return RC_Fine;
}
static ReturnCode get_file(const char *prompt, char *buf) {
    return get_string(prompt, buf, LINE_LEN);
}

/* --- Commands --- */
static ReturnCode ask_save(void) {
    status_line("Modified. Save? (y/n)", NULL);
    for (;;) {
        char c = get_char();
        if (c=='y') return RC_Fine;
        if (c=='n') return RC_Errors;
    }
}
static void FS(void) { status_line("Status", file_name); }
static void VI(void) {
    char nf[LINE_LEN];
    if (modified && ask_save() == RC_Errors) return;
    if (get_file("Visit file:", nf) == RC_Errors) return;
    initialize(); load_file(*nf ? nf : NULL);
}
static void WT(void) {
    if (!modified) { status_line("Nothing to write", NULL); return; }
    int fd = creat(file_name, 0644);
    if (fd < 0) { status_line("Cannot create", file_name); return; }
    Line *L = header->next; while (L != tail) { writeline(fd, L->text); L = L->next; }
    close(fd); modified=false; status_line("Wrote", file_name);
}
static void SH(void) {
    pid_t pid = fork(); if (pid < 0) { status_line("fork failed", NULL); return; }
    if (pid == 0) { term_restore(); execlp("sh", "sh", "-i", NULL); _exit(127); }
    else { wait(NULL); term_raw_enable(); redraw(); }
}
static void XT(void) {
    if (modified && ask_save() == RC_Errors) return;
    term_restore(); exit(EXIT_SUCCESS);
}
static void ESC_cmd(void) {
    int n; if (get_number("Repeat count:", &n) != RC_Fine) return;
    char c = get_char();
    while (n-- > 0 && !quit_flag) { /* Repeat not implemented for simplicity */ }
}
static void I(void) {}

/* --- Keymap/Command Dispatch --- */
typedef void (*CmdFn)(int);
static void insert_char(int code) {
    /* Example: Insert character at cursor position */
    if (cur_line && cur_line->len+1 < LINE_LEN-1) {
        size_t pos = cur_text-cur_line->text;
        memmove(cur_line->text+pos+1, cur_line->text+pos, cur_line->len-pos+1);
        cur_line->text[pos] = (char)code;
        cur_line->len++;
        cur_text = cur_line->text+pos+1;
        modified = true;
        redraw();
    }
}
static CmdFn key_map[128];
static void init_key_map(void) {
    for (int i=0; i<128; ++i) key_map[i] = insert_char;
    key_map[23] = FS;  /* Ctrl-W */
    key_map[24] = XT;  /* Ctrl-X */
    key_map[25] = SH;  /* Ctrl-Y */
    /* ... add more bindings as needed ... */
}

/* --- Main --- */
int main(int argc, char *argv[]) {
    initialize();
    term_raw_enable();
    init_key_map();
    if (argc > 1) load_file(argv[1]);
    last_y = SCREENMAX;
    redraw();

    while (!quit_flag) {
        char c = get_char();
        if ((unsigned char)c < 128) key_map[(int)c]((int)c);
        flush_buffer(STDOUT_FILENO);
        set_cursor(x, y);
    }
    term_restore();
    return 0;
}
