/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#include "errno.hpp"
#include "setjmp.hpp"
#include "sh.hpp"
#include "signal.hpp"

/* -------- csyn.cpp -------- */
/*
 * shell: syntax (C version)
 */

typedef union {
    char *cp;
    char **wp;
    int i;
    struct op *o;
} YYSTYPE;
#define WORD 256
#define LOGAND 257
#define LOGOR 258
#define BREAK 259
#define IF 260
#define THEN 261
#define ELSE 262
#define ELIF 263
#define FI 264
#define CASE 265
#define ESAC 266
#define FOR 267
#define WHILE 268
#define UNTIL 269
#define DO 270
#define DONE 271
#define IN 272
#define YYERRCODE 300

/* flags to yylex */
#define CONTIN 01 /* skip new lines to complete command */

/* #include "sh.hpp" */
#define SYNTAXERR zzerr()
static int startl = 1;
static int peeksym = 0;
static void zzerr();
static void word();
static char **copyw();
static struct op *block(), *namelist(), *list(), *newtp();
static struct op *pipeline(), *andor(), *command();
static struct op *nested(), *simple(), *c_list();
static struct op *dogroup(), *thenpart(), *casepart(), *caselist();
static struct op *elsepart();
static char **wordlist(), **pattern();
static void musthave();
static int yylex();
static struct ioword *io();
static struct ioword **copyio();
static char *tree();
static void diag();
static int nlseen;
static int iounit = IODEFAULT;
static struct op *tp;
struct op *newtp();

static YYSTYPE yylval;

int yyparse() {
    peeksym = 0;
    yynerrs = 0;
    outtree = c_list();
    musthave('\n', 0);
    return (yynerrs != 0);
}

static struct op *pipeline(cf)
int cf;
{
    register struct op *t, *p;
    register int c;

    t = command(cf);
    if (t != NULL) {
        while ((c = yylex(0)) == '|') {
            if ((p = command(CONTIN)) == NULL)
                SYNTAXERR;
            if (t->type != TPAREN && t->type != TCOM) {
                /* shell statement */
                t = block(TPAREN, t, nullptr, nullptr);
            }
            t = block(TPIPE, t, p, nullptr);
        }
        peeksym = c;
    }
    return (t);
}

static struct op *andor() {
    register struct op *t, *p;
    register int c;

    t = pipeline(0);
    if (t != NULL) {
        while ((c = yylex(0)) == LOGAND || c == LOGOR) {
            if ((p = pipeline(CONTIN)) == NULL)
                SYNTAXERR;
            t = block(c == LOGAND ? TAND : TOR, t, p, nullptr);
        }
        peeksym = c;
    }
    return (t);
}

static struct op *c_list() {
    register struct op *t, *p;
    register int c;

    t = andor();
    if (t != NULL) {
        while ((c = yylex(0)) == ';' || c == '&' || multiline && c == '\n') {
            if (c == '&')
                t = block(TASYNC, t, nullptr, nullptr);
            if ((p = andor()) == NULL)
                return (t);
            t = list(t, p);
        }
        peeksym = c;
    }
    return (t);
}

static int synio(cf)
int cf;
{
    register struct ioword *iop;
    register int i;
    register int c;

    if ((c = yylex(cf)) != '<' && c != '>') {
        peeksym = c;
        return (0);
    }
    i = yylval.i;
    musthave(WORD, 0);
    iop = io(iounit, i, yylval.cppp);
    iounit = IODEFAULT;
    if (i & IOHERE)
        markhere(yylval.cppp, iop);
}

static void musthave(c, cf) int c, cf;
{
    if ((peeksym = yylex(cf)) != c)
        SYNTAXERR;
    peeksym = 0;
}

static struct op *simple() {
    register struct op *t;

    t = NULL;
    for (;;) {
        switch (peeksym = yylex(0)) {
        case '<':
        case '>':
            (void)synio(0);
            break;

        case WORD:
            if (t == NULL) {
                t = newtp();
                t->type = TCOM;
            }
            peeksym = 0;
            word(yylval.cppp);
            break;

        default:
            return (t);
        }
    }
}

static struct op *nested(type, mark)
int type, mark;
{
    register struct op *t;

    multiline++;
    t = c_list();
    musthave(mark, 0);
    multiline--;
    return (block(type, t, nullptr, nullptr));
}

static struct op *command(cf)
int cf;
{
    register struct ioword *io;
    register struct op *t;
    struct wdblock *iosave;
    register int c;

    iosave = iolist;
    iolist = NULL;
    if (multiline)
        cf |= CONTIN;
    while (synio(cf))
        cf = 0;
    switch (c = yylex(cf)) {
    default:
        peeksym = c;
        if ((t = simple()) == NULL) {
            if (iolist == NULL)
                return (NULL);
            t = newtp();
            t->type = TCOM;
        }
        break;

    case '(':
        t = nested(TPAREN, ')');
        break;

    case '{':
        t = nested(TBRACE, '}');
        break;

    case FOR:
        t = newtp();
        t->type = TFOR;
        musthave(WORD, 0);
        startl = 1;
        t->str = yylval.cppp;
        multiline++;
        t->words = wordlist();
        if ((c = yylex(0)) != '\n' && c != ';')
            SYNTAXERR;
        t->left = dogroup(0);
        multiline--;
        break;

    case WHILE:
    case UNTIL:
        multiline++;
        t = newtp();
        t->type = c == WHILE ? TWHILE : TUNTIL;
        t->left = c_list();
        t->right = dogroup(1);
        t->words = NULL;
        multiline--;
        break;

    case CASE:
        t = newtp();
        t->type = TCASE;
        musthave(WORD, 0);
        t->str = yylval.cppp;
        startl++;
        multiline++;
        musthave(IN, CONTIN);
        startl++;
        t->left = caselist();
        musthave(ESAC, 0);
        multiline--;
        break;

    case IF:
        multiline++;
        t = newtp();
        t->type = TIF;
        t->left = c_list();
        t->right = thenpart();
        musthave(FI, 0);
        multiline--;
        break;
    }
    while (synio(0))
        ;
    t = namelist(t);
    iolist = iosave;
    return (t);
}

static struct op *dogroup(onlydone)
int onlydone;
{
    register int c;
    register struct op *list;

    c = yylex(CONTIN);
    if (c == DONE && onlydone)
        return (NULL);
    if (c != DO)
        SYNTAXERR;
    list = c_list();
    musthave(DONE, 0);
    return (list);
}

static struct op *thenpart() {
    register int c;
    register struct op *t;

    if ((c = yylex(0)) != THEN) {
        peeksym = c;
        return (NULL);
    }
    t = newtp();
    t->type = 0;
    t->left = c_list();
    if (t->left == NULL)
        SYNTAXERR;
    t->right = elsepart();
    return (t);
}

static struct op *elsepart() {
    register int c;
    register struct op *t;

    switch (c = yylex(0)) {
    case ELSE:
        if ((t = c_list()) == NULL)
            SYNTAXERR;
        return (t);

    case ELIF:
        t = newtp();
        t->type = TELIF;
        t->left = c_list();
        t->right = thenpart();
        return (t);

    default:
        peeksym = c;
        return (NULL);
    }
}

static struct op *caselist() {
    register struct op *t;
    register int c;

    t = NULL;
    while ((peeksym = yylex(CONTIN)) != ESAC)
        t = list(t, casepart());
    return (t);
}

static struct op *casepart() {
    register struct op *t;
    register int c;

    t = newtp();
    t->type = TPAT;
    t->words = pattern();
    musthave(')', 0);
    t->left = c_list();
    if ((peeksym = yylex(CONTIN)) != ESAC)
        musthave(BREAK, CONTIN);
    return (t);
}

static char **pattern() {
    register int c, cf;

    cf = CONTIN;
    do {
        musthave(WORD, cf);
        word(yylval.cppp);
        cf = 0;
    } while ((c = yylex(0)) == '|');
    peeksym = c;
    word(nullptr);
    return (copyw());
}

static char **wordlist() {
    register int c;

    if ((c = yylex(0)) != IN) {
        peeksym = c;
        return (NULL);
    }
    startl = 0;
    while ((c = yylex(0)) == WORD)
        word(yylval.cppp);
    word(nullptr);
    peeksym = c;
    return (copyw());
}

/*
 * supporting functions
 */
static struct op *list(t1, t2)
register struct op *t1, *t2;
{
    if (t1 == NULL)
        return (t2);
    if (t2 == NULL)
        return (t1);
    return (block(TLIST, t1, t2, nullptr));
}

static struct op *block(type, t1, t2, wp)
struct op *t1, *t2;
char **wp;
{
    register struct op *t;

    t = newtp();
    t->type = type;
    t->left = t1;
    t->right = t2;
    t->words = wp;
    return (t);
}

struct res {
    char *r_name;
    int r_val;
} restab[] = {
    "for",  FOR,   "case",  CASE,  "esac", ESAC,   "while", WHILE, "do",   DO,
    "done", DONE,  "if",    IF,    "in",   IN,     "then",  THEN,  "else", ELSE,
    "elif", ELIF,  "until", UNTIL, "fi",   FI,

    ";;",   BREAK, "||",    LOGOR, "&&",   LOGAND, "{",     '{',   "}",    '}',

    0,
};

rlookup(n) register char *n;
{
    register struct res *rp;

    for (rp = restab; rp->r_name; rp++)
        if (strcmp(rp->r_name, n) == 0)
            return (rp->r_val);
    return (0);
}

static struct op *newtp() {
    register struct op *t;

    t = (struct op *)tree(sizeof(*t));
    t->type = 0;
    t->words = NULL;
    t->ioact = NULL;
    t->left = NULL;
    t->right = NULL;
    t->str = NULL;
    return (t);
}

static struct op *namelist(t)
register struct op *t;
{
    if (iolist) {
        iolist = addword((char *)NULL, iolist);
        t->ioact = copyio();
    } else
        t->ioact = NULL;
    if (t->type != TCOM) {
        if (t->type != TPAREN && t->ioact != NULL) {
            t = block(TPAREN, t, nullptr, nullptr);
            t->ioact = t->left->ioact;
            t->left->ioact = NULL;
        }
        return (t);
    }
    word(nullptr);
    t->words = copyw();
    return (t);
}

static char **copyw() {
    register char **wd;

    wd = getwords(wdlist);
    wdlist = 0;
    return (wd);
}

static void word(cp) char *cp;
{
    wdlist = addword(cp, wdlist);
}

static struct ioword **copyio() {
    register struct ioword **iop;

    iop = (struct ioword **)getwords(iolist);
    iolist = 0;
    return (iop);
}

static struct ioword *io(u, f, cp)
char *cp;
{
    register struct ioword *iop;

    iop = (struct ioword *)tree(sizeof(*iop));
    iop->io_unit = u;
    iop->io_flag = f;
    iop->io_un.io_name = cp;
    iolist = addword((char *)iop, iolist);
    return (iop);
}

static void zzerr() { yyerror("syntax error"); }

yyerror(s) char *s;
{
    yynerrs++;
    if (talking) {
        if (multiline && nlseen)
            unget('\n');
        multiline = 0;
        while (yylex(0) != '\n')
            ;
    }
    err(s);
    fail();
}

static int yylex(cf)
int cf;
{
    register int c, c1;
    int atstart;

    if ((c = peeksym) > 0) {
        peeksym = 0;
        if (c == '\n')
            startl = 1;
        return (c);
    }
    nlseen = 0;
    e.linep = line;
    atstart = startl;
    startl = 0;
    yylval.i = 0;

loop:
    while ((c = getc(0)) == ' ' || c == '\t')
        ;
    switch (c) {
    default:
        if (any(c, "0123456789")) {
            unget(c1 = getc(0));
            if (c1 == '<' || c1 == '>') {
                iounit = c - '0';
                goto loop;
            }
            *e.linep++ = c;
            c = c1;
        }
        break;

    case '#':
        while ((c = getc(0)) != 0 && c != '\n')
            ;
        unget(c);
        goto loop;

    case 0:
        return (c);

    case '$':
        *e.linep++ = c;
        if ((c = getc(0)) == '{') {
            if ((c = collect(c, '}')) != '\0')
                return (c);
            goto pack;
        }
        break;

    case '`':
    case '\'':
    case '"':
        if ((c = collect(c, c)) != '\0')
            return (c);
        goto pack;

    case '|':
    case '&':
    case ';':
        if ((c1 = dual(c)) != '\0') {
            startl = 1;
            return (c1);
        }
        startl = 1;
        return (c);
    case '^':
        startl = 1;
        return ('|');
    case '>':
    case '<':
        diag(c);
        return (c);

    case '\n':
        nlseen++;
        gethere();
        startl = 1;
        if (multiline || cf & CONTIN) {
            if (talking && e.iop <= iostack)
                prs(cprompt->value);
            if (cf & CONTIN)
                goto loop;
        }
        return (c);

    case '(':
    case ')':
        startl = 1;
        return (c);
    }

    unget(c);

pack:
    while ((c = getc(0)) != 0 && !any(c, "`$ '\"\t;&<>()|^\n"))
        if (e.linep >= elinep)
            err("word too long");
        else
            *e.linep++ = c;
    unget(c);
    if (any(c, "\"'`$"))
        goto loop;
    *e.linep++ = '\0';
    if (atstart && (c = rlookup(line)) != 0) {
        startl = 1;
        return (c);
    }
    yylval.cppp = strsave(line, areanum);
    return (WORD);
}

int collect(c, c1)
register c, c1;
{
    char s[2];

    *e.linep++ = c;
    while ((c = getc(c1)) != c1) {
        if (c == 0) {
            unget(c);
            s[0] = c1;
            s[1] = 0;
            prs("no closing ");
            yyerror(s);
            return (YYERRCODE);
        }
        if (talking && c == '\n' && e.iop <= iostack)
            prs(cprompt->value);
        *e.linep++ = c;
    }
    *e.linep++ = c;
    return (0);
}

int dual(c)
register c;
{
    char s[3];
    register char *cp = s;

    *cp++ = c;
    *cp++ = getc(0);
    *cp = 0;
    if ((c = rlookup(s)) == 0)
        unget(*--cp);
    return (c);
}

static void diag(ec) register int ec;
{
    register int c;

    c = getc(0);
    if (c == '>' || c == '<') {
        if (c != ec)
            zzerr();
        yylval.i = ec == '>' ? IOWRITE | IOCAT : IOHERE;
        c = getc(0);
    } else
        yylval.i = ec == '>' ? IOWRITE : IOREAD;
    if (c != '&' || yylval.i == IOHERE)
        unget(c);
    else
        yylval.i |= IODUP;
}

static char *tree(size)
unsigned size;
{
    register char *t;

    if ((t = getcell(size)) == NULL) {
        prs("command line too complicated\n");
        fail();
        /* NOTREACHED */
    }
    return (t);
}

/* VARARGS1 */
/* ARGSUSED */
printf(s) /* yyparse calls it */
    char *s;
{
}
