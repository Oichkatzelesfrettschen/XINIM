/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

// Modern C++17 header guard
#pragma once

// Use the C++17 versions of the system headers
#include "../h/callnr.hpp" // system call numbers
#include "../h/const.hpp"  // system-wide constants
#include "../h/error.hpp"  // error codes
#include "../h/type.hpp"   // basic MINIX types
#include "defs.hpp"        // project specific definitions
#include <cstddef>         // for std::size_t

extern message M;

// Indices for the message passing service queues
// Legacy macros still used throughout the code base
#define MM 0 // memory manager index
#define FS 1 // file system index

// C++17 constants for new code
inline constexpr int kMM = MM;
inline constexpr int kFS = FS;
extern int callm1(int proc, int syscallnr, int int1, int int2, int int3, char *ptr1, char *ptr2,
                  char *ptr3);
extern int callm3(int proc, int syscallnr, int int1, char *name);
extern int callx(int proc, int syscallnr);
extern int len(char *s);
extern int send(int dst, message *m_ptr);
extern int receive(int src, message *m_ptr);
extern int sendrec(int srcdest, message *m_ptr);
extern int errno;
extern int begsig(void); /* interrupts all vector here */

/* Memory allocation wrappers */
void *safe_malloc(size_t size);
void safe_free(void *ptr);
