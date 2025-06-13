/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

#include "defs.hpp" // For xinim::uid_t, xinim::gid_t (via xinim/core_types.hpp)

// Standard representation of a password database entry.
struct passwd {
    char          *pw_name;   // User login name
    char          *pw_passwd; // Encrypted user password (often "x" or empty)
    xinim::uid_t   pw_uid;     // User identifier
    xinim::gid_t   pw_gid;     // Primary group identifier
    char          *pw_gecos;  // Real name or GECOS field (user information)
    char          *pw_dir;    // Path to user's home directory
    char          *pw_shell;  // Default shell program
};

// Standard password database function declarations
extern "C" {

struct passwd *getpwnam(const char *name) noexcept;
struct passwd *getpwuid(xinim::uid_t uid) noexcept;

struct passwd *getpwent(void) noexcept;
void setpwent(void) noexcept;
void endpwent(void) noexcept;

// Other potential functions (e.g., putpwent for writing, if implemented)
// int putpwent(const struct passwd *p, FILE *stream) noexcept; // Needs FILE from stdio.hpp

} // extern "C"
