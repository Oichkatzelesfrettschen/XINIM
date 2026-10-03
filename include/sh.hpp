/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* -------- sh.h -------- */
/*
 * shell
 */

#define NULL 0
inline constexpr int LINELIM = 1000;
inline constexpr int NPUSH = 8; /* limit to input nesting */

inline constexpr int NOFILE = 20; /* Number of open files */
inline constexpr int NUFILE = 10; /* Number of user-accessible files */
inline constexpr int FDBASE = 10; /* First file usable by Shell */

/*
 * values returned by wait
 */
constexpr int WaitSig(int s) { return s & 0177; }
constexpr int WaitVal(int s) { return (s >> 8) & 0377; }
constexpr bool WaitCore(int s) { return (s & 0200) != 0; }

/*
 * library and system defintions
 */
using xint = int; /* base type of jmp_buf, for broken compilers */

/*
 * shell components
 */
/* #include "area.h" */
/* #include "word.h" */
/* #include "io.h" */
/* #include "var.h" */

inline constexpr char QUOTE = 0200;

/*
 * Description of a command or an operation on commands.
 * Might eventually use a union.
 */
enum OpType {
    TCOM = 1, /* command */
    TPAREN,   /* (c-list) */
    TPIPE,    /* a | b */
    TLIST,    /* a [&;] b */
    TOR,      /* || */
    TAND,     /* && */
    TFOR,
    TDO,
    TCASE,
    TIF,
    TWHILE,
    TUNTIL,
    TELIF,
    TPAT,   /* pattern in case */
    TBRACE, /* {c-list} */
    TASYNC  /* c & */
};

// Parsed command tree node.
struct op {
    OpType type;           /* operation type, see below */
    char **words;          /* arguments to a command */
    struct ioword **ioact; /* IO actions (eg, < > >>) */
    struct op *left;
    struct op *right;
    char *str; /* identifier for case and for */
};

/*
 * actions determining the environment of a process
 */
constexpr int BIT(int i) { return 1 << i; }
inline constexpr int FEXEC = BIT(0); /* execute without forking */

/*
 * flags to control evaluation of words
 */
inline constexpr int DOSUB = 1;   /* interpret $, `, and quotes */
inline constexpr int DOBLANK = 2; /* perform blank interpretation */
inline constexpr int DOGLOB = 4;  /* interpret [?* */
inline constexpr int DOKEY = 8;   /* move words with `=' to 2nd arg. list */
inline constexpr int DOTRIM = 16; /* trim resulting string */

inline constexpr int DOALL = DOSUB | DOBLANK | DOGLOB | DOKEY | DOTRIM;

char **dolv;
int dolc;
int exstat;
char gflg;
int talking; /* interactive (talking-type wireless) */
int execflg;
int multiline;      /* \n changed to ; */
struct op *outtree; /* result from parser */

jmp_buf *failpt; /* last failure point */
jmp_buf *errpt;  /* current error handler */

// Breakpoint context for loops.
struct brkcon {
    jmp_buf brkpt;
    struct brkcon *nextlev;
} *brklist;
int isbreak;

/*
 * redirection
 */
// IO redirection descriptor.
struct ioword {
    short io_unit; /* unit affected */
    short io_flag; /* action (below) */
    union {
        char *io_name;         /* file name */
        struct block *io_here; /* here structure pointer */
    } io_un;
};
enum IoFlag {
    IOREAD = 1,   /* < */
    IOHERE = 2,   /* << (here file) */
    IOWRITE = 4,  /* > */
    IOCAT = 8,    /* >> */
    IOXHERE = 16, /* ${}, ` in << */
    IODUP = 32,   /* >&digit */
    IOCLOSE = 64  /* >&- */
};

inline constexpr int IODEFAULT = -1; /* token for default IO unit */

struct wdblock *wdlist;
struct wdblock *iolist;

/*
 * parsing & execution environment
 */
// Execution environment used during parsing and evaluation.
extern struct env {
    char *linep;
    struct io *iobase;
    struct io *iop;
    jmp_buf *errpt; /* saved error jump buffer */
    int iofd;
    struct env *oenv;
} e;

/*
 * flags:
 * -e: quit on error
 * -k: look for name=value everywhere on command line
 * -n: no execution
 * -t: exit after reading and executing one command
 * -v: echo as read
 * -x: trace
 * -u: unset variables net diagnostic
 */
char *flag;

char *null; /* null value for variable */
int intr;   /* interrupt pending */

char *trap[NSIG];
char ourtrap[NSIG];
int trapset; /* trap pending */

int inword; /* defer traps and interrupts */

int yynerrs; /* yacc */

char line[LINELIM];
char *elinep;

/*
 * other functions
 */
int (*inbuilt())(); /* find builtin command */
char *rexecve();
char *space();
char *getwd();
char *strsave();
char *evalstr();
char *putn();
char *itoa();
char *unquote();
struct var *lookup();
struct wdblock *add2args();
struct wdblock *glob();
char **makenv();
struct ioword *addio();
char **eval();
int setstatus();
int waitfor();

int onintr(); /* SIGINT handler */

/*
 * error handling
 */
void leave(); /* abort shell (or fail in subshell) */
void fail();  /* fail but return to process next command */
int sig();    /* default signal handler */

/*
 * library functions and system calls
 */
long lseek();
char *strncpy();
int strlen();
extern int errno;

/* -------- var.h -------- */

// Shell variable entry.
struct var {
    char *value;
    char *name;
    struct var *next;
    char status;
};
enum VarFlag {
    COPYV = 1,   /* flag to setval, suggesting copy */
    RONLY = 01,  /* variable is read-only */
    EXPORT = 02, /* variable is to be exported */
    GETCELL = 04 /* name & value space was got with getcell */
};

struct var *vlist; /* dictionary */

struct var *homedir; /* home directory */
struct var *prompt;  /* main prompt */
struct var *cprompt; /* continuation prompt */
struct var *path;    /* search path for commands */
struct var *shell;   /* shell to interpret command files */
struct var *ifs;     /* field separators */

struct var *lookup(/* char *s */);
void setval(/* struct var *, char * */);
void nameval(/* struct var *, char *val, *name */);
void export(/* struct var * */);
void ronly(/* struct var * */);
int isassign(/* char *s */);
int checkname(/* char *name */);
int assign(/* char *s, int copyflag */);
void putvlist(/* int key, int fd */);
int eqname(/* char *n1, char *n2 */);

/* -------- io.h -------- */
/* possible arguments to an IO function */
// Parameters to input/output functions.
struct ioarg {
    char *aword;
    char **awordlist;
    int afile; /* file descriptor */
};

/* an input generator's state */
// Runtime state of an input generator.
struct io {
    int (*iofn)();
    struct ioarg arg;
    int peekc;
    char nlcount; /* for `'s */
    char xchar;   /* for `'s */
    char task;    /* reason for pushed IO */
};
struct io iostack[NPUSH];
enum IoTask {
    XOTHER = 0, /* none of the below */
    XDOLL,      /* expanding ${} */
    XGRAVE,     /* expanding `'s */
    XIO         /* file IO */
};

/* in substitution */
inline bool INSUB() { return e.iop->task == XGRAVE || e.iop->task == XDOLL; }

/*
 * input generators for IO structure
 */
int nlchar();
int strchar();
int filechar();
int linechar();
int nextchar();
int gravechar();
int qgravechar();
int dolchar();
int wdchar();

/*
 * IO functions
 */
int getc();
int readc();
void unget();
void ioecho();
void prs();
void putc();
void prn();
void closef();
void closeall();

/*
 * IO control
 */
void pushio(/* struct ioarg arg, int (*gen)() */);
int remap();
int openpipe();
void closepipe();
struct io *setbase(/* struct io * */);

struct ioarg temparg; /* temporary for PUSHIO */
#define PUSHIO(what, arg, gen) ((temparg.what = (arg)), pushio(temparg, (gen)))
#define RUN(what, arg, gen) ((temparg.what = (arg)), run(temparg, (gen)))

/* -------- word.h -------- */
#ifndef WORD_H
#define WORD_H 1
// Flexible array of words used for argument storage.
struct wdblock {
    short w_bsize;
    short w_nword;
    /* bounds are arbitrary */
    char *w_words[1];
};

struct wdblock *addword();
struct wdblock *newword();
char **getwords();
#endif

/* -------- area.h -------- */

/*
 * storage allocation
 */
char *getcell(/* unsigned size */);
void garbage();
void setarea(/* char *obj, int to */);
void freearea(/* int area */);
void freecell(/* char *obj */);

int areanum; /* current allocation area */

#define NEW(type) (type *)getcell(sizeof(type))
#define DELETE(obj) freecell((char *)obj)
