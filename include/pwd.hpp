#pragma once

/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

// Legacy representation of a password file entry.
struct passwd {
    char *pw_name;   // User login name
    char *pw_passwd; // Encrypted user password
    int pw_uid;      // User identifier
    int pw_gid;      // Primary group identifier
    char *pw_gecos;  // Additional user information
    char *pw_dir;    // Path to user's home directory
    char *pw_shell;  // Default shell program
};
