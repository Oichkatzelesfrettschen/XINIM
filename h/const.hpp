#pragma once
// Modernized for C++23
#include "type.hpp" // Includes xinim::core_types.hpp transitively

// Copyright (C) 1987 by Prentice-Hall, Inc.  Permission is hereby granted to
// private individuals and educational institutions to modify and
// redistribute the binary and source programs of this system to other
// private individuals and educational institutions for educational and
// research purposes.  For corporate or commercial use, permission from
// Prentice-Hall is required.  In general, such permission will be granted,
// subject to a few conditions.

// Storage class macros used throughout the historical code base. Modern code
// should prefer explicit C++ keywords, but many translation units still rely on
// these identifiers. They are defined here to preserve compatibility.
#ifndef EXTERN
#define EXTERN extern
#endif
#ifndef PRIVATE
#define PRIVATE static
#endif
#ifndef PUBLIC
#define PUBLIC
#endif

// Boolean helpers retained for compatibility with historical code.
inline constexpr bool TRUE = true;   ///< Boolean true
inline constexpr bool FALSE = false; ///< Boolean false

// Storage class macros have been removed in favor of standard C++ keywords.

inline constexpr int HZ = 60;           // clock frequency
inline constexpr int BLOCK_SIZE = 1024; // number of bytes in a disk block
inline constexpr int MAJOR = 8;         // major device shift
inline constexpr int MINOR = 0;         // minor device mask

inline constexpr int NR_TASKS = 8;  // number of tasks in transfer vector
inline constexpr int NR_PROCS = 16; // number of slots in proc table
inline constexpr int NR_SEGS = 3;   // segments per process
inline constexpr int T = 0;         // text segment index
inline constexpr int D = 1;         // data segment index
inline constexpr int S = 2;         // stack segment index

inline constexpr std::int32_t kMaxPLong = 2147483647; // maximum positive signed 32-bit long
inline constexpr std::int32_t MAX_P_LONG = kMaxPLong; // alias for legacy code
// Note: Original was 2147483647L. Changed to std::int32_t for clarity.

// Memory is allocated in clicks.
inline constexpr int CLICK_SIZE = 0020; // allocation unit in bytes (16 in decimal)
inline constexpr int CLICK_SHIFT = 4;   // log2 of CLICK_SIZE

// Process numbers of important processes
inline constexpr int MM_PROC_NR = 0;   // memory manager process number
inline constexpr int FS_PROC_NR = 1;   // file system process number
inline constexpr int INIT_PROC_NR = 2; // init process number
inline constexpr int LOW_USER = 2;     // first user not part of OS

// Miscellaneous constants
inline constexpr int BYTE_MASK = 0377; // mask for 8 bits (Original name: BYTE)
inline constexpr int BYTE = BYTE_MASK; // alias for legacy code
inline constexpr int TO_USER = 0;      // copy from fs to user
inline constexpr int FROM_USER = 1;    // copy from user to fs
inline constexpr int READING = 0;      // copy data to user
inline constexpr int WRITING = 1;      // copy data from user
inline constexpr int ABS = -999;       // this process means absolute memory

inline constexpr int WORD_SIZE = 2; // number of bytes per word

// NIL_PTR is defined in xinim::core_types.hpp via h/type.hpp
// Using xinim::NIL_PTR or nullptr directly is preferred in new code.

inline constexpr int NO_NUM = 0x8000;         // used as numerical argument to panic
inline constexpr int MAX_PATH_LEN = 128;      // max length of path names (Original name: MAX_PATH)
inline constexpr int SIG_PUSH_BYTES = 8;      // bytes pushed by signal
inline constexpr int MAX_ISTACK_BYTES = 1024; // maximum initial stack size for EXEC

// Device numbers of root (RAM) and boot (fd0) devices.
// dev_nr is a Minix-specific type (uint16_t) from h/type.hpp
inline constexpr dev_nr ROOT_DEV = static_cast<dev_nr>(256);
inline constexpr dev_nr BOOT_DEV = static_cast<dev_nr>(512);

// Flag bits for i_mode in the inode.
// mask_bits is a Minix-specific type (uint16_t) from h/type.hpp
inline constexpr mask_bits I_TYPE = 0170000;          // inode type field
inline constexpr mask_bits I_REGULAR = 0100000;       // regular file
inline constexpr mask_bits I_BLOCK_SPECIAL = 0060000; // block special file
inline constexpr mask_bits I_DIRECTORY = 0040000;     // directory file
inline constexpr mask_bits I_CHAR_SPECIAL = 0020000;  // character special file
inline constexpr mask_bits I_SET_UID_BIT = 0004000;   // set effective uid on exec
inline constexpr mask_bits I_SET_GID_BIT = 0002000;   // set effective gid on exec
inline constexpr mask_bits ALL_MODES = 0006777;       // all bits for user, group and others
inline constexpr mask_bits RWX_MODES = 0000777;       // RWX permission bits
inline constexpr mask_bits R_BIT = 0000004;           // read protection bit
inline constexpr mask_bits W_BIT = 0000002;           // write protection bit
inline constexpr mask_bits X_BIT = 0000001;           // execute protection bit
inline constexpr mask_bits I_NOT_ALLOC = 0000000;     // inode is free

// Ensure NIL_PTR from h/type.hpp is not redefined if it was there.
// The version from xinim::core_types.hpp is preferred.
// The line `inline constexpr char *NIL_PTR = nullptr;` was in the original `h/const.hpp`
// This might conflict if h/type.hpp also defines it or includes xinim::core_types.hpp which defines
// it. For this pass, I will remove the NIL_PTR definition from h/const.hpp to rely on the one from
// xinim::core_types.hpp (via h/type.hpp). Renamed BYTE to BYTE_MASK to avoid conflict with
// potential byte type. Renamed MAX_PATH to MAX_PATH_LEN for clarity.
