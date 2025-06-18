/*
 * get entry from group file
 *
 * By: Patrick van Kleef
 */

#include "../include/grp.hpp" // group structure definition
#include <cstdlib>            // atoi
#include <cstring>            // strcmp
#include <fcntl.h>            // open
#include <unistd.h>           // lseek, read, close

// Path to the system group file
static char _gr_file[] = "/etc/group";
// Temporary line buffer for a single group entry
static char _grbuf[256];
// Raw read buffer for streaming data
static char _buffer[1024];
// Pointer within ``_buffer``
static char *_pnt;
// Pointer to the current location in ``_grbuf``
static char *_buf;
// File descriptor for ``_gr_file`` or -1 when closed
static int _gfd = -1;
// Remaining bytes in ``_buffer``
static int _bufcnt;
// Current parsed group structure returned by the API
static struct group grp;

// Reset the group database to the beginning.
int setgrent() {
    if (_gfd >= 0)
        lseek(_gfd, 0L, SEEK_SET);
    else
        _gfd = open(_gr_file, O_RDONLY);

    _bufcnt = 0;
    return _gfd;
}

// Close the group database file.
void endgrent() {
    if (_gfd >= 0)
        close(_gfd);

    _gfd = -1;
    _bufcnt = 0;
}

// Read a single line from the group file into ``_grbuf``.
static int getline() {
    if (_gfd < 0 && setgrent() < 0)
        return 0;

    _buf = _grbuf;
    do {
        if (--_bufcnt <= 0) {
            _bufcnt = read(_gfd, _buffer, sizeof(_buffer));
            if (_bufcnt <= 0)
                return 0;
            _pnt = _buffer;
        }
        *_buf++ = *_pnt++;
    } while (*_pnt != '\n');
    _pnt++;
    _bufcnt--;
    *_buf = '\0';
    _buf = _grbuf;
    return 1;
}

/* Skip to next ':' in buffer */
// Advance ``_buf`` to the next ':' character and terminate the current token.
static void skip_period() {
    while (*_buf != ':')
        ++_buf;
    *_buf++ = '\0';
}

/* Return next group entry */
// Parse the next group entry from the file.
struct group *getgrent() {
    if (getline() == 0)
        return nullptr;

    grp.name = _buf;
    skip_period();
    grp.passwd = _buf;
    skip_period();
    grp.gid = atoi(_buf);
    skip_period();
    return &grp;
}

/* Get group entry by name */
// Locate a group entry by its name.
struct group *getgrnam(char *name) {
    struct group *grp_ptr;

    setgrent();
    while ((grp_ptr = getgrent()) != nullptr) {
        if (std::strcmp(grp_ptr->name, name) == 0)
            break;
    }
    endgrent();
    return grp_ptr;
}

/* Get group entry by gid */
// Locate a group entry by its numeric identifier.
struct group *getgrgid(int gid) {
    struct group *grp_ptr;

    setgrent();
    while ((grp_ptr = getgrent()) != nullptr) {
        if (grp_ptr->gid == gid)
            break;
    }
    endgrent();
    return grp_ptr;
}
