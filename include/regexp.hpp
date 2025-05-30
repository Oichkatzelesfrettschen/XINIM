/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
#define strchr index

/* Number of bits in a character, used by legacy UCHARAT macro. */
inline constexpr int CHARBITS = 0377;

/* Maximum number of parenthesized subexpressions supported. */
inline constexpr int NSUBEXP = 10;

/*
 * Representation for compiled regular expressions.  The startp/endp
 * arrays hold pointers to subexpression matches.  The reg* fields are
 * used internally by the matcher to speed up execution.
 */
struct regexp {
    char *startp[NSUBEXP]; ///< Pointers to start of matches.
    char *endp[NSUBEXP];   ///< Pointers to end of matches.
    char regstart;         ///< Required first character, or '\0'.
    char reganch;          ///< If true, pattern is anchored.
    char *regmust;         ///< Required substring within the match.
    int regmlen;           ///< Length of regmust substring.
    char program[1];       ///< Bytecode for the compiled pattern.
};

regexp *regcomp(const char *exp);
int regexec(regexp *prog, const char *string, bool bolflag);
void regsub(regexp *prog, const char *source, char *dest);
void regerror(const char *s);
