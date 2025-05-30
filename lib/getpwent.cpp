/* getpwent.cpp
 *
 * Simple password file access routines.
 * Converted to C90 style and updated field names.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/pwd.h"

#define PRIVATE static

/* Local buffers and state */
PRIVATE char _pw_file[] = "/etc/passwd";
PRIVATE char _pwbuf[256];
PRIVATE char _buffer[1024];
PRIVATE char *_pnt;
PRIVATE char *_buf;
PRIVATE int _pw = -1;
PRIVATE int _bufcnt;
PRIVATE struct passwd pwd;

/* Function prototypes */
int setpwent(void);
void endpwent(void);
static int getline(void);
static void skip_period(void);
struct passwd *getpwent(void);
struct passwd *getpwnam(const char *name);
struct passwd *getpwuid(int uid);

int setpwent(void) {
    if (_pw >= 0)
        lseek(_pw, 0L, 0);
    else
        _pw = open(_pw_file, 0);

    _bufcnt = 0;
    return _pw;
}

void endpwent(void) {
    if (_pw >= 0)
        close(_pw);

    _pw = -1;
    _bufcnt = 0;
}

static int getline(void) {
    if (_pw < 0 && setpwent() < 0)
        return 0;
    _buf = _pwbuf;
    do {
        if (--_bufcnt <= 0) {
            if ((_bufcnt = read(_pw, _buffer, 1024)) <= 0)
                return 0;
            else
                _pnt = _buffer;
        }
        *_buf++ = *_pnt++;
    } while (*_pnt != '\n');
    _pnt++;
    _bufcnt--;
    *_buf = 0;
    _buf = _pwbuf;
    return 1;
}

static void skip_period(void) {
    while (*_buf != ':')
        _buf++;

    *_buf++ = '\0';
}

struct passwd *getpwent(void) {
    if (getline() == 0)
        return 0;

    pwd.pw_name = _buf;
    skip_period();
    pwd.pw_passwd = _buf;
    skip_period();
    pwd.pw_uid = atoi(_buf);
    skip_period();
    pwd.pw_gid = atoi(_buf);
    skip_period();
    pwd.pw_gecos = _buf;
    skip_period();
    pwd.pw_dir = _buf;
    skip_period();
    pwd.pw_shell = _buf;

    return &pwd;
}

struct passwd *getpwnam(const char *name) {
    struct passwd *pwd;

    setpwent();
    while ((pwd = getpwent()) != NULL)
        if (!strcmp(pwd->pw_name, name))
            break;
    endpwent();
    if (pwd != NULL)
        return pwd;
    else
        return NULL;
}

struct passwd *getpwuid(int uid) {
    struct passwd *pwd;

    setpwent();
    while ((pwd = getpwent()) != NULL)
        if (pwd->pw_uid == uid)
            break;
    endpwent();
    if (pwd != NULL)
        return pwd;
    else
        return NULL;
}
