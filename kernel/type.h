/* The 'pc_psw' struct is machine dependent.  It must contain the information
 * pushed onto the stack by an interrupt, in the same format as the hardware
 * creates and expects.  It is used for storing the interrupt status after a
 * trap or interrupt, as well as for causing interrupts for signals.
 */

#include <stdint.h>
struct pc_psw {
  uint64_t rip;                 /* instruction pointer */
  uint64_t rflags;              /* rflags register */
};

struct sig_info {
  int signo;
  struct pc_psw sigpcpsw;
};
