/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

#pragma once

// Shared stat structure used by user space. The layout is provided by the
// common header to keep it consistent with the kernel version.
#include "shared/stat_struct.hpp" // Defines struct stat using xinim types
#include "../xinim/core_types.hpp" // For xinim::mode_t, xinim::dev_t

/* File mode constants. These use xinim::mode_t to match struct stat::st_mode. */
inline constexpr xinim::mode_t S_IFMT = 0170000;  /* type of file */
inline constexpr xinim::mode_t S_IFDIR = 0040000; /* directory */
inline constexpr xinim::mode_t S_IFCHR = 0020000; /* character special */
inline constexpr xinim::mode_t S_IFBLK = 0060000; /* block special */
inline constexpr xinim::mode_t S_IFREG = 0100000; /* regular */
// Note: S_IFLNK (symbolic link) might be missing if not supported by Minix's FS directly in this way
// For XINIM, if symlinks are treated as regular files with a special marker,
// or handled differently, this might not be needed. Assuming basic file types for now.

inline constexpr xinim::mode_t S_ISUID = 04000;   /* set user id on execution */
inline constexpr xinim::mode_t S_ISGID = 02000;   /* set group id on execution */
inline constexpr xinim::mode_t S_ISVTX = 01000;   /* save swapped text even after use (sticky bit) */

/* Permission bits */
inline constexpr xinim::mode_t S_IRWXU = 00700; /* mask for file owner permissions */
inline constexpr xinim::mode_t S_IRUSR = 00400; /* owner has read permission */
inline constexpr xinim::mode_t S_IWUSR = 00200; /* owner has write permission */
inline constexpr xinim::mode_t S_IXUSR = 00100; /* owner has execute permission */

inline constexpr xinim::mode_t S_IRWXG = 00070; /* mask for group permissions */
inline constexpr xinim::mode_t S_IRGRP = 00040; /* group has read permission */
inline constexpr xinim::mode_t S_IWGRP = 00020; /* group has write permission */
inline constexpr xinim::mode_t S_IXGRP = 00010; /* group has execute permission */

inline constexpr xinim::mode_t S_IRWXO = 00007; /* mask for others permissions */
inline constexpr xinim::mode_t S_IROTH = 00004; /* others have read permission */
inline constexpr xinim::mode_t S_IWOTH = 00002; /* others have write permission */
inline constexpr xinim::mode_t S_IXOTH = 00001; /* others have execute permission */

// The constants S_IREAD, S_IWRITE, S_IEXEC from the original file were owner permission bits.
// Replaced them with the more standard POSIX names (S_IRUSR, S_IWUSR, S_IXUSR) and added group/other.
// If Minix specifically used S_IREAD as 00400 for generic read checks beyond owner, that logic would need review.
// For a stat header, POSIX names are generally preferred.

// Standard stat-related function declarations
extern "C" {

int stat(const char *pathname, struct stat *statbuf) noexcept;
int fstat(int fd, struct stat *statbuf) noexcept;
int lstat(const char *pathname, struct stat *statbuf) noexcept; // POSIX standard, include if supported

int mkdir(const char *pathname, xinim::mode_t mode) noexcept;

int chmod(const char *pathname, xinim::mode_t mode) noexcept;
int fchmod(int fd, xinim::mode_t mode) noexcept;

// mknod is often here, but sometimes in its own header or sys/types.h
// For XINIM, its declaration was already found in lib/mknod.cpp's includes (likely lib.hpp)
// but good to have a consistent declaration.
int mknod(const char *pathname, xinim::mode_t mode, xinim::dev_t dev) noexcept;

// umask is also related but often in stdlib.h or sys/types.h
// int umask(xinim::mode_t mask) noexcept; // Declaration found in lib/umask.cpp (via lib.hpp)

} // extern "C"
