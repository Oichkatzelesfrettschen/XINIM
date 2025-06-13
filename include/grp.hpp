/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

#include "defs.hpp" // For xinim::gid_t (via xinim/core_types.hpp) and std::size_t

// Standard representation of a group database entry.
struct group {
    char          *gr_name;   /* Name of the group */
    char          *gr_passwd; /* Encrypted group password (often "x" or empty) */
    xinim::gid_t   gr_gid;     /* Numerical group identifier */
    char         **gr_mem;    /* Null-terminated array of pointers to member names */
};

// Standard group database function declarations
extern "C" {

struct group *getgrnam(const char *name) noexcept;
struct group *getgrgid(xinim::gid_t gid) noexcept;

struct group *getgrent(void) noexcept;
void setgrent(void) noexcept;
void endgrent(void) noexcept;

// POSIX standard function for setting supplementary group IDs
int setgroups(std::size_t size, const xinim::gid_t *list) noexcept;

// Other potential functions (getgroups) could be added if implemented/needed.
// int getgroups(int size, xinim::gid_t list[]) noexcept;

} // extern "C"
