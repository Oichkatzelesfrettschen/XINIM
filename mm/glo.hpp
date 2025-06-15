#pragma once
/// \file glo.hpp
/// \brief Global variables shared by the memory manager.
// Modernized for C++23

/** \name Global process management state */
///@{
extern struct mproc *mp; ///< Pointer to the current process entry.
extern int dont_reply;   ///< Non-zero to suppress replies to the caller.
extern int procs_in_use; ///< Number of process slots currently in use.
///@}

/** \name Incoming call context */
///@{
extern message mm_in;  ///< Incoming system call message.
extern message mm_out; ///< Message used to construct the reply.
extern int who;        ///< Process number of the caller.
extern int mm_call;    ///< System call identifier.
///@}

/** \name Result passing variables */
///@{
extern int err_code;  ///< Temporary storage for an error number.
extern int result2;   ///< Secondary result value.
extern char *res_ptr; ///< Pointer result returned to the caller.
///@}

extern char mm_stack[MM_STACK_BYTES]; ///< Memory manager stack storage.
