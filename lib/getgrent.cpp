/*
 * get entry from group file
 *
 * By: Patrick van Kleef
 */

#include "../include/grp.hpp"

#define PRIVATE static

PRIVATE char _gr_file[] = "/etc/group";
PRIVATE char _grbuf[256];
PRIVATE char _buffer[1024];
PRIVATE char *_pnt;
PRIVATE char *_buf;
PRIVATE int _gfd = -1;
PRIVATE int _bufcnt;
PRIVATE struct group grp;

/* Rewind group database */
int setgrent(void) {
    if (_gfd >= 0)
        lseek(_gfd, 0L, 0);
    else
        _gfd = open(_gr_file, 0);

    _bufcnt = 0;
    return (_gfd);
}

/* Close group database */
void endgrent(void) {
    if (_gfd >= 0)
        close(_gfd);

    _gfd = -1;
    _bufcnt = 0;
}

/* Read one line from the group file */
static int getline(void) {
    if (_gfd < 0 && setgrent() < 0)
        return (0);

    _buf = _grbuf;
    do {
        if (--_bufcnt <= 0) {
            if ((_bufcnt = read(_gfd, _buffer, 1024)) <= 0)
                return (0);
            else
                _pnt = _buffer;
        }
        *_buf++ = *_pnt++;
    } while (*_pnt != '\n');
    _pnt++;
    _bufcnt--;
    *_buf = 0;
    _buf = _grbuf;
    return (1);
}

/* Skip to next ':' in buffer */
static void skip_period(void) {
    while (*_buf != ':')
        _buf++;
    *_buf++ = '\0';
}

/* Return next group entry */
struct group *getgrent(void) {
    if (getline() == 0)
        return (0);

    grp.name = _buf;
    skip_period();
    grp.passwd = _buf;
    skip_period();
    grp.gid = atoi(_buf);
    skip_period();
    return (&grp);
}

/* Get group entry by name */
struct group *getgrnam(char *name) {
    struct group *grp;

    setgrent();
    while ((grp = getgrent()) != 0)
        if (!strcmp(grp->name, name))
            break;
    endgrent();
    if (grp != 0)
        return (grp);
    else
        return (0);
}

/* Get group entry by gid */
struct group *getgrgid(int gid) {
    struct group *grp;

    setgrent();
    while ((grp = getgrent()) != 0)
        if (grp->gid == gid)
            break;
    endgrent();
    if (grp != 0)
        return (grp);
    else
        return (0);
}
