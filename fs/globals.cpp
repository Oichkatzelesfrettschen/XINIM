#include "glo.hpp"

// Define global variables for the file system
struct fproc *fp = nullptr;           // pointer to caller's fproc struct
int super_user = 0;                   // set to 1 if caller is super user
int dont_reply = 0;                   // normally 0; set to 1 to inhibit reply
int susp_count = 0;                   // number of processes suspended on pipe
int reviving = 0;                     // number of pipe processes to be revived
file_pos rdahedpos = 0;               // position to read ahead
struct inode *rdahed_inode = nullptr; // pointer to inode to read ahead
message m;                            // the input message itself
message m1;                           // the output message used for reply
int who = 0;                          // caller's proc number
int fs_call = 0;                      // system call number
char user_path[MAX_PATH];             // storage for user path name
int err_code = 0;                     // temporary storage for error number
char fstack[FS_STACK_BYTES];          // the File System's stack
