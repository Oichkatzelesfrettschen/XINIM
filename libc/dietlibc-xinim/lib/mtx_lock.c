#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <threads.h>
#include <sys/futex.h>
#include <errno.h>

int mtx_lock(mtx_t* mutex) {
  return mtx_timedlock(mutex,0);
}
