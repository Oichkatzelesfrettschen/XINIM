/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#include "errno.hpp"
#include "setjmp.hpp"
#include "sh.hpp"
#include "signal.hpp"
#include "stat.hpp"

/* -------- eval.cpp -------- */
/* #include "sh.hpp" */
/* #include "word.h" */

/*
 * ${}
 * `command`
 * blank interpretation
 * quoting
 * glob
 */

static char *blank();
static int grave();
static int expand();
static int dollar();

char **eval(ap, f)
register char **ap;
{
    struct wdblock *wb;
    char **wp;
    jmp_buf ev;

    inword++;
    wp = NULL;
    wb = NULL;
    errpt = &ev;
    if (newenv(setjmp(ev)) == 0) {
        wb = addword((char *)0, wb); /* space for shell name, if command file */
        while (expand(*ap++, &wb, f))
            ;
        wb = addword((char *)0, wb);
        wp = getwords(wb) + 1;
        quitenv();
    } else
        gflg = 1;
    inword--;
    return (gflg ? NULL : wp);
}

/*
 * Make the exported environment from the exported
 * names in the dictionary.  Keyword assignments
 * ought to be taken from wp (the list of words on the command line)
 * but aren't, yet. Until then: ARGSUSED
 */
char **makenv(wp)
char **wp;
{
    register struct wdblock *wb;
    register struct var *vp;

    wb = NULL;
    for (vp = vlist; vp; vp = vp->next)
        if (vp->status & EXPORT)
            wb = addword(vp->name, wb);
    wb = addword((char *)0, wb);
    return (getwords(wb));
}

char *evalstr(cp, f)
register char *cp;
int f;
{
    struct wdblock *wb;

    inword++;
    wb = NULL;
    if (expand(cp, &wb, f)) {
        if (wb == NULL || wb->w_nword == 0 || (cp = wb->w_words[0]) == NULL)
            cp = "";
        DELETE(wb);
    } else
        cp = NULL;
    inword--;
    return (cp);
}

static int expand(cp, wbp, f)
register char *cp;
register struct wdblock **wbp;
{
    jmp_buf ev;

    gflg = 0;
    if (cp == NULL)
        return (0);
    if (!anys("$`'\"", cp) && !anys(ifs->value, cp) && ((f & DOGLOB) == 0 || !anys("[*?", cp))) {
        cp = strsave(cp, areanum);
        if (f & DOTRIM)
            unquote(cp);
        *wbp = addword(cp, *wbp);
        return (1);
    }
    errpt = &ev;
    if (newenv(setjmp(ev)) == 0) {
        PUSHIO(aword, cp, strchar);
        e.iobase = e.iop;
        while ((cp = blank(f)) && gflg == 0) {
            e.linep = cp;
            cp = strsave(cp, areanum);
            if ((f & DOGLOB) == 0) {
                if (f & DOTRIM)
                    unquote(cp);
                *wbp = addword(cp, *wbp);
            } else
                *wbp = glob(cp, *wbp);
        }
        quitenv();
    } else
        gflg = 1;
    return (gflg == 0);
}

/*
 * Blank interpretation and quoting
 */
static char *blank(f) {
    register c, c1;
    register char *sp;

    sp = e.linep;

loop:
    switch (c = subgetc('"', 0)) {
    case 0:
        if (sp == e.linep)
            return (0);
        *e.linep++ = 0;
        return (sp);

    default:
        if (f & DOBLANK && any(c, ifs->value))
            goto loop;
        break;

    case '"':
    case '\'':
        if (INSUB())
            break;
        for (c1 = c; (c = subgetc(c1, 1)) != c1;) {
            if (c == 0)
                break;
            if (c == '\'' || !any(c, "$`\""))
                c |= QUOTE;
            *e.linep++ = c;
        }
        c = 0;
    }
    unget(c);
    for (;;) {
        c = subgetc('"', 0);
        if (c == 0 || f & DOBLANK && any(c, ifs->value) || !INSUB() && any(c, "\"'`")) {
            unget(c);
            if (any(c, "\"'`"))
                goto loop;
            break;
        }
        *e.linep++ = c;
    }
    *e.linep++ = 0;
    return (sp);
}

/*
 * Get characters, substituting for ` and $
 */
int subgetc(ec, quoted)
register char ec;
int quoted;
{
    register char c;

again:
    c = getc(ec);
    if (!INSUB() && ec != '\'') {
        if (c == '`') {
            if (grave(quoted) == 0)
                return (0);
            e.iop->task = XGRAVE;
            goto again;
        }
        if (c == '$' && (c = dollar(quoted)) == 0) {
            e.iop->task = XDOLL;
            goto again;
        }
    }
    return (c);
}

/*
 * Prepare to generate the string returned by ${} substitution.
 */
static int dollar(quoted)
int quoted;
{
    int otask;
    struct io *oiop;
    char *dolp;
    register char *s, c, *cp;
    struct var *vp;

    c = readc();
    s = e.linep;
    if (c != '{') {
        *e.linep++ = c;
        if (letter(c)) {
            while ((c = readc()) != 0 && letnum(c))
                if (e.linep < elinep)
                    *e.linep++ = c;
            unget(c);
        }
        c = 0;
    } else {
        oiop = e.iop;
        otask = e.iop->task;
        e.iop->task = XOTHER;
        while ((c = subgetc('"', 0)) != 0 && c != '}' && c != '\n')
            if (e.linep < elinep)
                *e.linep++ = c;
        if (oiop == e.iop)
            e.iop->task = otask;
        if (c != '}') {
            err("unclosed ${");
            gflg++;
            return (c);
        }
    }
    if (e.linep >= elinep) {
        err("string in ${} too long");
        gflg++;
        e.linep -= 10;
    }
    *e.linep = 0;
    if (*s)
        for (cp = s + 1; *cp; cp++)
            if (any(*cp, "=-+?")) {
                c = *cp;
                *cp++ = 0;
                break;
            }
    if (s[1] == 0 && (*s == '*' || *s == '@')) {
        if (dolc > 1) {
            /* currently this does not distinguish $* and $@ */
            /* should check dollar */
            e.linep = s;
            PUSHIO(awordlist, dolv + 1, dolchar);
            return (0);
        } else { /* trap the nasty ${=} */
            s[0] = '1';
            s[1] = 0;
        }
    }
    vp = lookup(s);
    if ((dolp = vp->value) == null) {
        switch (c) {
        case '=':
            if (digit(*s)) {
                err("cannot use ${...=...} with $n");
                gflg++;
                break;
            }
            setval(vp, cp);
            dolp = vp->value;
            break;

        case '-':
            dolp = strsave(cp, areanum);
            break;

        case '?':
            if (*cp == 0) {
                prs("missing value for ");
                err(s);
            } else
                err(cp);
            gflg++;
            break;
        }
    } else if (c == '+')
        dolp = strsave(cp, areanum);
    if (flag['u'] && dolp == null) {
        prs("unset variable: ");
        err(s);
        gflg++;
    }
    e.linep = s;
    PUSHIO(aword, dolp, strchar);
    return (0);
}

/*
 * Run the command in `...` and read its output.
 */
static int grave(quoted)
int quoted;
{
    register char *cp;
    register int i;
    int pf[2];

    for (cp = e.iop->arg.aword; *cp != '`'; cp++)
        if (*cp == 0) {
            err("no closing `");
            return (0);
        }
    if (openpipe(pf) < 0)
        return (0);
    if ((i = fork()) == -1) {
        closepipe(pf);
        err("try again");
        return (0);
    }
    if (i != 0) {
        e.iop->arg.aword = ++cp;
        close(pf[1]);
        PUSHIO(afile, remap(pf[0]), quoted ? qgravechar : gravechar);
        return (1);
    }
    *cp = 0;
    /* allow trapped signals */
    for (i = 0; i < NSIG; i++)
        if (ourtrap[i] && signal(i, SIG_IGN) != SIG_IGN)
            signal(i, SIG_DFL);
    dup2(pf[1], 1);
    closepipe(pf);
    flag['e'] = 0;
    flag['v'] = 0;
    flag['n'] = 0;
    cp = strsave(e.iop->arg.aword, 0);
    freearea(areanum = 1); /* free old space */
    e.oenv = NULL;
    e.iop = (e.iobase = iostack) - 1;
    unquote(cp);
    talking = 0;
    PUSHIO(aword, cp, nlchar);
    onecommand();
    exit(1);
}

char *unquote(as)
register char *as;
{
    register char *s;

    if ((s = as) != NULL)
        while (*s)
            *s++ &= ~QUOTE;
    return (as);
}

/* -------- glob.cpp -------- */
/* #include "sh.hpp" */

#define DIRSIZ 14
struct direct {
    unsigned short d_ino;
    char d_name[DIRSIZ];
};
/*
 * glob
 */

#define scopy(x) strsave((x), areanum)
#define BLKSIZ 512
#define NDENT ((BLKSIZ + sizeof(struct direct) - 1) / sizeof(struct direct))

static struct wdblock *cl, *nl;
static char spcl[] = "[?*";
static int xstrcmp();
static char *generate();
static int anyspcl();

struct wdblock *glob(cp, wb)
char *cp;
struct wdblock *wb;
{
    register i;
    register char *pp;

    if (cp == 0)
        return (wb);
    i = 0;
    for (pp = cp; *pp; pp++)
        if (any(*pp, spcl))
            i++;
        else if (!any(*pp & ~QUOTE, spcl))
            *pp &= ~QUOTE;
    if (i != 0) {
        for (cl = addword(scopy(cp), (struct wdblock *)0); anyspcl(cl); cl = nl) {
            nl = newword(cl->w_nword * 2);
            for (i = 0; i < cl->w_nword; i++) { /* for each argument */
                for (pp = cl->w_words[i]; *pp; pp++)
                    if (any(*pp, spcl)) {
                        globname(cl->w_words[i], pp);
                        break;
                    }
                if (*pp == '\0')
                    nl = addword(scopy(cl->w_words[i]), nl);
            }
            for (i = 0; i < cl->w_nword; i++)
                DELETE(cl->w_words[i]);
            DELETE(cl);
        }
        for (i = 0; i < cl->w_nword; i++)
            unquote(cl->w_words[i]);
        glob0((char *)cl->w_words, cl->w_nword, sizeof(char *), xstrcmp);
        if (cl->w_nword) {
            for (i = 0; i < cl->w_nword; i++)
                wb = addword(cl->w_words[i], wb);
            DELETE(cl);
            return (wb);
        }
    }
    wb = addword(unquote(cp), wb);
    return (wb);
}

globname(we, pp) char *we;
register char *pp;
{
    register char *np, *cp;
    char *name, *gp, *dp;
    int dn, j, n, k;
    struct direct ent[NDENT];
    char dname[DIRSIZ + 1];
    struct stat dbuf;

    for (np = we; np != pp; pp--)
        if (pp[-1] == '/')
            break;
    for (dp = cp = space(pp - np + 3); np < pp;)
        *cp++ = *np++;
    *cp++ = '.';
    *cp = '\0';
    for (gp = cp = space(strlen(pp) + 1); *np && *np != '/';)
        *cp++ = *np++;
    *cp = '\0';
    dn = open(dp, 0);
    if (dn < 0) {
        DELETE(dp);
        DELETE(gp);
        return;
    }
    dname[DIRSIZ] = '\0';
    while ((n = read(dn, (char *)ent, sizeof(ent))) >= sizeof(*ent)) {
        n /= sizeof(*ent);
        for (j = 0; j < n; j++) {
            if (ent[j].d_ino == 0)
                continue;
            strncpy(dname, ent[j].d_name, DIRSIZ);
            if (dname[0] == '.' && (dname[1] == '\0' || dname[1] == '.' && dname[2] == '\0'))
                if (*gp != '.')
                    continue;
            for (k = 0; k < DIRSIZ; k++)
                if (any(dname[k], spcl))
                    dname[k] |= QUOTE;
            if (gmatch(dname, gp)) {
                name = generate(we, pp, dname, np);
                if (*np && !anys(np, spcl)) {
                    if (stat(name, &dbuf)) {
                        DELETE(name);
                        continue;
                    }
                }
                nl = addword(name, nl);
            }
        }
    }
    close(dn);
    DELETE(dp);
    DELETE(gp);
}

/*
 * generate a pathname as below.
 * start..end1 / middle end
 * the slashes come for free
 */
static char *generate(start1, end1, middle, end)
char *start1;
register char *end1;
char *middle, *end;
{
    char *p;
    register char *op, *xp;

    p = op = space(end1 - start1 + strlen(middle) + strlen(end) + 2);
    for (xp = start1; xp != end1;)
        *op++ = *xp++;
    for (xp = middle; (*op++ = *xp++) != '\0';)
        ;
    op--;
    for (xp = end; (*op++ = *xp++) != '\0';)
        ;
    return (p);
}

static int anyspcl(wb)
register struct wdblock *wb;
{
    register i;
    register char **wd;

    wd = wb->w_words;
    for (i = 0; i < wb->w_nword; i++)
        if (anys(spcl, *wd++))
            return (1);
    return (0);
}

static int xstrcmp(p1, p2)
char *p1, *p2;
{
    return (strcmp(*(char **)p1, *(char **)p2));
}

/* -------- word.cpp -------- */
/* #include "sh.hpp" */
/* #include "word.h" */
char *memcpy();

#define NSTART 16 /* default number of words to allow for initially */

struct wdblock *newword(nw)
register nw;
{
    register struct wdblock *wb;

    wb = (struct wdblock *)space(sizeof(*wb) + nw * sizeof(char *));
    wb->w_bsize = nw;
    wb->w_nword = 0;
    return (wb);
}

struct wdblock *addword(wd, wb)
char *wd;
register struct wdblock *wb;
{
    register struct wdblock *wb2;
    register nw;

    if (wb == NULL)
        wb = newword(NSTART);
    if ((nw = wb->w_nword) >= wb->w_bsize) {
        wb2 = newword(nw * 2);
        memcpy((char *)wb2->w_words, (char *)wb->w_words, nw * sizeof(char *));
        wb2->w_nword = nw;
        DELETE(wb);
        wb = wb2;
    }
    wb->w_words[wb->w_nword++] = wd;
    return (wb);
}

char **getwords(wb)
register struct wdblock *wb;
{
    register char **wd;
    register nb;

    if (wb == NULL)
        return (NULL);
    if (wb->w_nword == 0) {
        DELETE(wb);
        return (NULL);
    }
    wd = (char **)space(nb = sizeof(*wd) * wb->w_nword);
    memcpy((char *)wd, (char *)wb->w_words, nb);
    DELETE(wb); /* perhaps should done by caller */
    return (wd);
}

int (*func)();
int globv;

glob0(a0, a1, a2, a3) char *a0;
unsigned a1;
int a2;
int (*a3)();
{
    func = a3;
    globv = a2;
    glob1(a0, a0 + a1 * a2);
}

glob1(base, lim) char *base, *lim;
{
    register char *i, *j;
    int v2;
    char **k;
    char *lptr, *hptr;
    int c;
    unsigned n;

    v2 = globv;

top:
    if ((n = lim - base) <= v2)
        return;
    n = v2 * (n / (2 * v2));
    hptr = lptr = base + n;
    i = base;
    j = lim - v2;
    for (;;) {
        if (i < lptr) {
            if ((c = (*func)(i, lptr)) == 0) {
                glob2(i, lptr -= v2);
                continue;
            }
            if (c < 0) {
                i += v2;
                continue;
            }
        }

    begin:
        if (j > hptr) {
            if ((c = (*func)(hptr, j)) == 0) {
                glob2(hptr += v2, j);
                goto begin;
            }
            if (c > 0) {
                if (i == lptr) {
                    glob3(i, hptr += v2, j);
                    i = lptr += v2;
                    goto begin;
                }
                glob2(i, j);
                j -= v2;
                i += v2;
                continue;
            }
            j -= v2;
            goto begin;
        }

        if (i == lptr) {
            if (lptr - base >= lim - hptr) {
                glob1(hptr + v2, lim);
                lim = lptr;
            } else {
                glob1(base, lptr);
                base = hptr + v2;
            }
            goto top;
        }

        glob3(j, lptr -= v2, i);
        j = hptr -= v2;
    }
}

glob2(i, j) char *i, *j;
{
    register char *index1, *index2, c;
    int m;

    m = globv;
    index1 = i;
    index2 = j;
    do {
        c = *index1;
        *index1++ = *index2;
        *index2++ = c;
    } while (--m);
}

glob3(i, j, k) char *i, *j, *k;
{
    register char *index1, *index2, *index3;
    int c;
    int m;

    m = globv;
    index1 = i;
    index2 = j;
    index3 = k;
    do {
        c = *index1;
        *index1++ = *index3;
        *index3++ = *index2;
        *index2++ = c;
    } while (--m);
}
