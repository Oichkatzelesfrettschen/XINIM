#include "syscalls.h"

.text
.global gettimeofday
#ifdef __PIE__
.hidden gettimeofday
#endif
.type gettimeofday,@function
gettimeofday:
	mov $__NR_gettimeofday, %al
	jmp __unified_syscall
.Lhere:
	.size	 gettimeofday,.Lhere-gettimeofday
