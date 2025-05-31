/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* File System global variables */
extern struct fproc *fp;           /* pointer to caller's fproc struct */
extern int super_user;             /* 1 if caller is super_user, else 0 */
extern int dont_reply;             /* normally 0; set to 1 to inhibit reply */
extern int susp_count;             /* number of procs suspended on pipe */
extern int reviving;               /* number of pipe processes to be revived */
extern file_pos rdahedpos;         /* position to read ahead */
extern struct inode *rdahed_inode; /* pointer to inode to read ahead */

/* The parameters of the call are kept here. */
extern message m;                /* the input message itself */
extern message m1;               /* the output message used for reply */
extern int who;                  /* caller's proc number */
extern int fs_call;              /* system call number */
extern char user_path[MAX_PATH]; /* storage for user path name */

/* The following variables are used for returning results to the caller. */
extern int err_code; /* temporary storage for error number */

extern char fstack[FS_STACK_BYTES]; /* the File System's stack. */
