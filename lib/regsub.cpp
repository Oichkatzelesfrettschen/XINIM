/*
 * regsub
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 */
#include "../include/regexp.hpp"
#include "../include/stdio.hpp"
/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define MAGIC 0234

#ifndef CHARBITS
#define UCHARAT(p) ((int)*(unsigned char *)(p))
#else
#define UCHARAT(p) ((int)*(p) & CHARBITS)
#endif

/*
 - regsub - perform substitutions after a regexp match
 */
void regsub(regexp *prog, const char *source, char *dest) {
    register const char *src;
    register char *dst;
    register char c;
    register int no;
    register int len;
    extern char *strncpy();

    if (prog == NULL || source == NULL || dest == NULL) {
        regerror("NULL parm to regsub");
        return;
    }
    if (UCHARAT(prog->program) != MAGIC) {
        regerror("damaged regexp fed to regsub");
        return;
    }

    src = source;
    dst = dest;
    while ((c = *src++) != '\0') {
        if (c == '&')
            no = 0;
        else if (c == '\\' && '0' <= *src && *src <= '9')
            no = *src++ - '0';
        else
            no = -1;

        if (no < 0) { /* Ordinary character. */
            if (c == '\\' && (*src == '\\' || *src == '&'))
                c = *src++;
            *dst++ = c;
        } else if (prog->startp[no] != NULL && prog->endp[no] != NULL) {
            len = prog->endp[no] - prog->startp[no];
            strncpy(dst, prog->startp[no], len);
            dst += len;
            if (len != 0 && *(dst - 1) == '\0') { /* strncpy hit NUL. */
                regerror("damaged match string");
                return;
            }
        }
    }
    *dst++ = '\0';
}
