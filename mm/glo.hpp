#pragma once
// Modernized for C++17

/* Global variables. */
extern struct mproc *mp; /* ptr to 'mproc' slot of current process */
extern int dont_reply;   /* normally 0; set to 1 to inhibit reply */
extern int procs_in_use; /* how many processes are marked as IN_USE */

/* The parameters of the call are kept here. */
extern message mm_in;  /* the incoming message itself is kept here. */
extern message mm_out; /* the reply message is built up here. */
extern int who;        /* caller's proc number */
extern int mm_call;    /* caller's proc number */

/* The following variables are used for returning results to the caller. */
extern int err_code;  /* temporary storage for error number */
extern int result2;   /* secondary result */
extern char *res_ptr; /* result, if pointer */

extern char mm_stack[MM_STACK_BYTES]; /* MM's stack */
