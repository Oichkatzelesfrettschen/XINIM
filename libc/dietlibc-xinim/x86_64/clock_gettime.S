#include "syscalls.h"

.text
.global clock_gettime
#ifdef __PIE__
.hidden clock_gettime
#endif
.type clock_gettime,@function
clock_gettime:
	mov $__NR_clock_gettime, %al
	jmp __unified_syscall

.Lhere:
	.size	 clock_gettime,.Lhere-clock_gettime
