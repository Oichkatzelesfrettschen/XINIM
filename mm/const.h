/* Constants used by the Memory Manager. */

#define ZEROBUF_SIZE	1024	/* buffer size for erasing memory */

/* Size of MM's stack depends mostly on do_exec(). */
#if ZEROBUF_SIZE > MAX_PATH
#define MM_STACK_BYTES	MAX_ISTACK_BYTES + ZEROBUF_SIZE + 384
#else
#define MM_STACK_BYTES	MAX_ISTACK_BYTES + MAX_PATH + 384
#endif

#define PAGE_SIZE        4096
#define MAX_PAGES      1048576
#define HDR_SIZE           32
#define NO_MEM (phys_clicks)0	/* returned by alloc_mem() with mem is up */


#define printf        printk
