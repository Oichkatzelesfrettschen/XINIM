/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* gres - grep and substitute		Author: Martin C. Atkins */

/*
 *	globally search, and replace
 *
 *<-xtx-*>cc -o gres gres.cpp -lregexp
 */

/*
 *	This program was written by:
 *		Martin C. Atkins,
 *		University of York,
 *		Heslington,
 *		York. Y01 5DD
 *		England
 *	and is released into the public domain, on the condition
 *	that this comment is always included without alteration.
 */

#include "../include/lib.hpp" // C++17 header
#include "regexp.hpp"
#include "stdio.hpp"

#define MAXLINE (1024)

int status = 1;
char *usagemsg = "Usage: gres [-g] search replace [file ...]\n";
char *progname;
int gflag = 0; /* != 0 => only do first substitution on line */
extern char *index();
FILE *fopen();

int main(int argc, char *argv[]) {
    regexp *exp;
    char *repstr;
    char **argp = &argv[1];

    progname = argv[0];
    if (*argp != 0 && argp[0][0] == '-' && argp[0][1] == 'g') {
        gflag = 1;
        argp++, argc--;
    }
    if (argc < 3) {
        std_err(usagemsg);
        done(2);
    }
    if (argp[0][0] == '\0') {
        std_err("gres: null match string is silly\n");
        done(2);
    }
    if ((exp = regcomp(*argp++)) == NULL) {
        std_err("gres: regcomp failed\n");
        done(2);
    }
    repstr = *argp++;
    if (*argp == 0)
        process(stdin, exp, repstr);
    else
        while (*argp) {
            FILE *inf;

            if (strcmp(*argp, "-") == 0)
                process(stdin, exp, repstr);
            else {
                if ((inf = fopen(*argp, "r")) == NULL) {
                    std_err("gres: Can't open ");
                    std_err(*argp);
                    std_err("\n");
                    status = 2;
                } else {
                    process(inf, exp, repstr);
                    fclose(inf);
                }
            }
            argp++;
        }
    done(status);
}

/*
 *	This routine does the processing
 */
static void process(FILE *inf, regexp *exp, char *repstr) {
    char ibuf[MAXLINE];

    while (fgets(ibuf, MAXLINE, inf) != NULL) {
        char *cr = index(ibuf, '\n');
        if (cr == 0)
            std_err("gres: Line broken\n");
        else
            *cr = '\0';
        if (regexec(exp, ibuf, 1)) {
            pline(exp, ibuf, repstr);
            if (status != 2)
                status = 0;
        } else
            printf("%s\n", ibuf);
    }
}

void static void regerror(char *s) {
    std_err("gres: ");
    std_err(s);
    std_err("\n");
    done(2);
}

char *static char *getbuf(regexp *exp, char *repstr) {
    static bufsize = 0;
    static char *buf = 0;
    int guess = 10;
    int ch;

    while (*repstr) {
        switch (*repstr) {
        case '&':
            guess += exp->endp[0] - exp->startp[0];
            break;
        case '\\':
            if ((ch = *++repstr) < '0' || ch > '9')
                guess += 2;
            else {
                ch -= '0';
                guess += exp->endp[ch] - exp->startp[ch];
            }
            break;
        default:
            guess++;
        }
        repstr++;
    }
    if (bufsize < guess) {
        if (buf != 0)
            safe_free((char *)buf);
        buf = safe_malloc(guess);
    }
    return buf;
}

static void pline(regexp *exp, char ibuf[], char *repstr) {
    do {
        dosub(exp, ibuf, repstr);
        ibuf = exp->endp[0];
        if (*ibuf == '\0')
            break;
        if (ibuf == exp->startp[0])
            putchar(*ibuf++);
    } while (!gflag && regexec(exp, ibuf, 0));
    printf("%s\n", ibuf);
}

/*
 *	print one subsitution
 */
static void dosub(regexp *exp, char ibuf[], char *repstr) {
    char *buf = getbuf(exp, repstr);
    char *end = exp->startp[0];
    int ch = *end;

    *end = '\0';
    fputs(ibuf, stdout); /* output the initial part of line */
    *end = ch;
    regsub(exp, repstr, buf);
    fputs(buf, stdout);
}

static void done(int n) {
    _cleanup(); /* flush stdio's internal buffers */
    exit(n);
}
