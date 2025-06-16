/* This file handles the EXEC system call.  It performs the work as follows:
 *    - see if the permissions allow the file to be executed
 *    - read the header and extract the sizes
 *    - fetch the initial args and environment from the user space
 *    - allocate the memory for the new process
 *    - copy the initial stack from MM to the process
 *    - read in the text and data segments and copy to the process
 *    - take care of setuid and setgid bits
 *    - fix up 'mproc' table
 *    - tell kernel about EXEC
 *
 *   The only entry point is do_exec.
 */

#include "../h/callnr.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/stat.h"
#include "../h/type.hpp" // Defines target types like phys_bytes, vir_bytes, vir_clicks
#include "alloc.hpp"
#include "const.hpp"
#include "glo.hpp"
#include "mproc.hpp"
#include "param.hpp"
#include "token.hpp"
#include <algorithm> // For std::min (if min is not a macro)
#include <cstddef>   // For std::size_t
#include <cstdint>   // For uint64_t, int64_t

#define MAGIC 0x04000301L /* magic number with 2 bits masked off */
#define SEP 0x00200000L   /* value for separate I & D */
#define TEXTB 2           /* location of text size in header */
#define DATAB 3           /* location of data size in header */
#define BSSB 4            /* location of bss size in header */
#define TOTB 6            /* location of total size in header */

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
/**
 * @brief Execute a new program image in the current process.
 *
 * Copies arguments from user space, loads the binary and updates
 * process credentials.
 *
 * @return OK on success or an error code.
 */
PUBLIC int do_exec() {
    /* Perform the exece(name, argv, envp) call.  The user library builds a
     * complete stack image, including pointers, args, environ, etc.  The stack
     * is copied to a buffer inside MM, and then to the new core image.
     */

    register struct mproc *rmp;
    int m, r, fd, ft;
    char mbuf[MAX_ISTACK_BYTES]; /* buffer for stack and zeroes */
    union u {
        char name_buf[MAX_PATH]; /* the name of the file to exec */
        char zb[ZEROBUF_SIZE];   /* used to zero bss */
    } u;
    char *new_sp;
    std::size_t src, dst, text_bytes, data_bytes, bss_bytes, stk_bytes,
        vsp;            // vir_bytes -> std::size_t
    uint64_t tot_bytes; /* total space for program, including gap (phys_bytes -> uint64_t) */
    std::size_t sc;     // vir_clicks -> std::size_t
    struct stat s_buf;

    /* Do some validity checks. */
    rmp = mp;
    stk_bytes = static_cast<std::size_t>(stack_bytes); // stack_bytes is int from param.hpp
    if (stk_bytes > MAX_ISTACK_BYTES)
        return (ErrorCode::ENOMEM);           /* stack too big */
    if (exec_len <= 0 || exec_len > MAX_PATH) // exec_len is int from param.hpp
        return (ErrorCode::EINVAL);

    /* Get the exec file name and see if the file is executable. */
    src = reinterpret_cast<std::size_t>(exec_name);  // exec_name is char*
    dst = reinterpret_cast<std::size_t>(u.name_buf); // u.name_buf is char[]
    // New mem_copy sig: (int, int, uintptr_t, int, int, uintptr_t, std::size_t)
    // src, dst are std::size_t holding pointer values, cast to uintptr_t. exec_len is int.
    r = mem_copy(who, D, static_cast<uintptr_t>(src), MM_PROC_NR, D, static_cast<uintptr_t>(dst),
                 static_cast<std::size_t>(exec_len));
    if (r != OK)
        return (r);                          /* file name not in user data segment */
    tell_fs(CHDIR, who, 0, 0);               /* temporarily switch to user's directory */
    fd = allowed(u.name_buf, &s_buf, X_BIT); /* is file executable? */
    tell_fs(CHDIR, 0, 1, 0);                 /* switch back to MM's own directory */
    if (fd < 0)
        return (fd); /* file was not executable */

    /* Read the file header and extract the segment sizes. */
    sc = (stk_bytes + static_cast<std::size_t>(CLICK_SIZE) - 1) >> CLICK_SHIFT;
    m = read_header(fd, &ft, &text_bytes, &data_bytes, &bss_bytes, &tot_bytes, sc);
    if (m < 0) {
        close(fd); /* something wrong with header */
        return (ErrorCode::ENOEXEC);
    }

    /* Fetch the stack from the user before destroying the old core image. */
    src = reinterpret_cast<std::size_t>(stack_ptr); // stack_ptr is char* from param.hpp
    dst = reinterpret_cast<std::size_t>(mbuf);      // mbuf is char[]
    // src, dst are std::size_t (pointer values) -> uintptr_t. stk_bytes is std::size_t.
    r = mem_copy(who, D, static_cast<uintptr_t>(src), MM_PROC_NR, D, static_cast<uintptr_t>(dst),
                 stk_bytes);
    if (r != OK) {
        close(fd); /* can't fetch stack (e.g. bad virtual addr) */
        return (ErrorCode::EACCES);
    }

    /* Allocate new memory and release old memory.  Fix map and tell kernel. */
    r = new_mem(text_bytes, data_bytes, bss_bytes, stk_bytes, tot_bytes, u.zb, ZEROBUF_SIZE);
    if (r != OK) {
        close(fd); /* insufficient core or program too big */
        return (r);
    }

    /* Patch up stack and copy it from MM to new core image. */
    // rmp->mp_seg[S].mem_vir is vir_clicks (std::size_t)
    vsp = rmp->mp_seg[S].mem_vir << CLICK_SHIFT;
    patch_ptr(mbuf, vsp);
    src = reinterpret_cast<std::size_t>(mbuf);
    // src, vsp, stk_bytes are std::size_t. src, vsp are pointer values -> uintptr_t.
    r = mem_copy(MM_PROC_NR, D, static_cast<uintptr_t>(src), who, D, static_cast<uintptr_t>(vsp),
                 stk_bytes);
    if (r != OK)
        panic("do_exec stack copy err", NO_NUM);

    /* Read in text and data segments. */
    load_seg(fd, T, text_bytes);
    load_seg(fd, D, data_bytes);
    close(fd); /* don't need exec file any more */

    /* Take care of setuid/setgid bits. */
    if (s_buf.st_mode & I_SET_UID_BIT) {
        rmp->mp_effuid = s_buf.st_uid;
        tell_fs(SETUID, who, (int)rmp->mp_realuid, (int)rmp->mp_effuid);
    }
    if (s_buf.st_mode & I_SET_GID_BIT) {
        rmp->mp_effgid = s_buf.st_gid;
        tell_fs(SETGID, who, (int)rmp->mp_realgid, (int)rmp->mp_effgid);
    }

    /* Fix up some 'mproc' fields and tell kernel that exec is done. */
    rmp->mp_catch = 0;          /* reset all caught signals */
    rmp->mp_flags &= ~SEPARATE; /* turn off SEPARATE bit */
    rmp->mp_flags |= ft;        /* turn it on for separate I & D files */
    rmp->mp_token = generate_token();
    new_sp = (char *)vsp;
    sys_exec(who, new_sp, rmp->mp_token);
    return (OK);
}

/*===========================================================================*
 *				read_header				     *
 *===========================================================================*/
/**
 * @brief Read the executable header.
 *
 * Parses the text, data, bss and total sizes from the binary.
 *
 * @param fd         File descriptor of the executable.
 * @param ft         Output flag for separate I&D.
 * @param text_bytes Size of the text segment.
 * @param data_bytes Size of the data segment.
 * @param bss_bytes  Size of the bss segment.
 * @param tot_bytes  Total bytes required.
 * @param sc         Initial stack clicks.
 * @return OK or error code from size_ok().
 */
PRIVATE int read_header(int fd, int *ft, std::size_t *text_bytes, std::size_t *data_bytes,
                        std::size_t *bss_bytes, uint64_t *tot_bytes, std::size_t sc)
// fd, ft are int
// text_bytes, data_bytes, bss_bytes are std::size_t* (vir_bytes*)
// tot_bytes is uint64_t* (phys_bytes*)
// sc is std::size_t (vir_clicks)
{
    /* Read the header and extract the text, data, bss and total sizes from it. */

    int m, ct;
    std::size_t tc, dc, s_vir, dvir;   // vir_clicks -> std::size_t
    uint64_t totc;                     // phys_clicks -> uint64_t
    long buf[HDR_SIZE / sizeof(long)]; // HDR_SIZE is from mm/const.hpp (std::size_t)

    /* Read the header and check the magic number.  The standard MINIX header
     * consists of 8 longs, as follows:
     *	0: 0x04100301L (combined I & D space) or 0x04200301L (separate I & D)
     *	1: 0x00000020L
     *	2: size of text segments in bytes
     *	3: size of initialized data segment in bytes
     *	4: size of bss in bytes
     *	5: 0x00000000L
     *	6: total memory allocated to program (text, data and stack, combined)
     *	7: 0x00000000L
     * The longs are represented low-order byte first and high-order byte last.
     * The first byte of the header is always 0x01, followed by 0x03.
     * The header is followed directly by the text and data segments, whose sizes
     * are given in the header.
     */

    if (read(fd, buf, HDR_SIZE) != HDR_SIZE)
        return (ErrorCode::ENOEXEC);
    if ((buf[0] & 0xFF0FFFFFL) != MAGIC)
        return (ErrorCode::ENOEXEC);
    *ft = (buf[0] & SEP ? SEPARATE : 0); /* separate I & D or not */

    /* Get text and data sizes. */
    *text_bytes = static_cast<std::size_t>(buf[TEXTB]); /* text size in bytes */
    *data_bytes = static_cast<std::size_t>(buf[DATAB]); /* data size in bytes */
    if (*ft != SEPARATE) {
        /* If I & D space is not separated, it is all considered data. Text=0 */
        *data_bytes += *text_bytes;
        *text_bytes = 0;
    }

    /* Get bss and total sizes. */
    *bss_bytes = static_cast<std::size_t>(buf[BSSB]); /* bss size in bytes */
    *tot_bytes = static_cast<uint64_t>(buf[TOTB]);    /* total bytes to allocate for program */
    if (*tot_bytes == 0)
        return (ErrorCode::ENOEXEC);

    /* Check to see if segment sizes are feasible. */
    tc = (*text_bytes + static_cast<std::size_t>(CLICK_SHIFT) - 1) >> CLICK_SHIFT;
    dc = (*data_bytes + *bss_bytes + static_cast<std::size_t>(CLICK_SHIFT) - 1) >> CLICK_SHIFT;
    // *tot_bytes is uint64_t, CLICK_SIZE is int.
    totc = (*tot_bytes + static_cast<uint64_t>(CLICK_SIZE) - 1) >> CLICK_SHIFT;
    if (dc >= totc) // dc is size_t, totc is uint64_t. This comparison may need a cast for safety if
                    // totc can be very large. Assuming dc (virtual clicks) will not exceed totc
                    // (physical clicks scaled) in a valid scenario.
        return (ErrorCode::ENOEXEC);   /* stack must be at least 1 click */
    dvir = (*ft == SEPARATE ? 0 : tc); // tc is size_t, dvir is size_t
    // s_vir (size_t) = dvir (size_t) + (totc (uint64_t) - sc (size_t))
    // This calculation needs care. If totc is much larger than sc and their difference doesn't fit
    // size_t (if size_t is 32-bit). Assuming virtual address space (size_t) is sufficient for these
    // calculations.
    s_vir = dvir + static_cast<std::size_t>(totc - sc);
    m = size_ok(*ft, tc, dc, sc, dvir, s_vir);
    ct = static_cast<int>(buf[1] & BYTE); /* header length */
    if (ct > HDR_SIZE)
        read(fd, buf, ct - HDR_SIZE); /* skip unused hdr */
    return (m);
}

/*===========================================================================*
 *				new_mem					     *
 *===========================================================================*/
/**
 * @brief Allocate a new memory map for the process.
 *
 * Replaces the old mapping and zeroes bss, gap and stack.
 *
 * @param text_bytes Size of text segment.
 * @param data_bytes Size of data segment.
 * @param bss_bytes  Size of bss segment.
 * @param stk_bytes  Size of stack.
 * @param tot_bytes  Total bytes required.
 * @param bf         Temporary zero buffer.
 * @param zs         Size of the zero buffer.
 * @return OK on success or an error code.
 */
PRIVATE int new_mem(std::size_t text_bytes, std::size_t data_bytes, std::size_t bss_bytes,
                    std::size_t stk_bytes, uint64_t tot_bytes, char bf[ZEROBUF_SIZE], int zs)
// text_bytes, data_bytes, bss_bytes, stk_bytes are std::size_t (vir_bytes)
// tot_bytes is uint64_t (phys_bytes)
// bf, zs are char[], int
{
    /* Allocate new memory and release the old memory.  Change the map and report
     * the new map to the kernel.  Zero the new core image's bss, gap and stack.
     */

    register struct mproc *rmp;
    char *rzp;
    std::size_t vzb;                                    // vir_bytes -> std::size_t
    std::size_t text_clicks, data_clicks, stack_clicks; // vir_clicks -> std::size_t
    int64_t gap_clicks;                                 // Potentially signed
    uint64_t tot_clicks;                                // phys_clicks based -> uint64_t
    uint64_t new_base, old_clicks;                      // phys_clicks -> uint64_t
    uint64_t bytes, base, count, bss_offset;            // phys_bytes -> uint64_t
    // extern phys_clicks alloc_mem(); // alloc_mem now returns uint64_t
    // extern phys_clicks max_hole();  // max_hole now returns uint64_t

    /* Acquire the new memory.  Each of the 4 parts: text, (data+bss), gap,
     * and stack occupies an integral number of clicks, starting at click
     * boundary.  The data and bss parts are run together with no space.
     */

    text_clicks = (text_bytes + static_cast<std::size_t>(CLICK_SIZE) - 1) >> CLICK_SHIFT;
    data_clicks =
        (data_bytes + bss_bytes + static_cast<std::size_t>(CLICK_SIZE) - 1) >> CLICK_SHIFT;
    stack_clicks = (stk_bytes + static_cast<std::size_t>(CLICK_SIZE) - 1) >> CLICK_SHIFT;
    tot_clicks = (tot_bytes + static_cast<uint64_t>(CLICK_SIZE) - 1) >> CLICK_SHIFT;

    // Gap calculation: tot_clicks is physical, data/stack clicks are virtual (but scaled like
    // physical). This calculation assumes they are directly comparable after scaling.
    gap_clicks = static_cast<int64_t>(tot_clicks) - static_cast<int64_t>(data_clicks) -
                 static_cast<int64_t>(stack_clicks);
    if (gap_clicks < 0)
        return (ErrorCode::ENOMEM);

    /* Check to see if there is a hole big enough.  If so, we can risk first
     * releasing the old core image before allocating the new one, since we
     * know it will succeed.  If there is not enough, return failure.
     */
    // text_clicks (size_t) + tot_clicks (uint64_t) -> result is uint64_t
    // max_hole() returns uint64_t (phys_clicks)
    if (static_cast<uint64_t>(text_clicks) + tot_clicks > max_hole())
        return (ErrorCode::EAGAIN);

    /* There is enough memory for the new core image.  Release the old one. */
    rmp = mp;
    // rmp->mp_seg members mem_vir, mem_len are vir_clicks (std::size_t)
    // rmp->mp_seg[T].mem_phys is phys_clicks (uint64_t)
    old_clicks = static_cast<uint64_t>(rmp->mp_seg[S].mem_vir + rmp->mp_seg[S].mem_len);
    if (rmp->mp_flags & SEPARATE)
        old_clicks += static_cast<uint64_t>(rmp->mp_seg[T].mem_len);
    free_mem(rmp->mp_seg[T].mem_phys, old_clicks); /* free the memory */

    /* We have now passed the point of no return.  The old core image has been
     * forever lost.  The call must go through now.  Set up and report new map.
     */
    // alloc_mem takes and returns phys_clicks (uint64_t)
    new_base = alloc_mem(static_cast<uint64_t>(text_clicks) + tot_clicks); /* new core image */
    if (new_base == NO_MEM)
        panic("MM hole list is inconsistent", NO_NUM);
    rmp->mp_seg[T].mem_vir = 0;
    rmp->mp_seg[T].mem_len = text_clicks;
    rmp->mp_seg[T].mem_phys = new_base;
    rmp->mp_seg[D].mem_vir = 0;
    rmp->mp_seg[D].mem_len = data_clicks;
    rmp->mp_seg[D].mem_phys = new_base + text_clicks;
    rmp->mp_seg[S].mem_vir = rmp->mp_seg[D].mem_vir + data_clicks + gap_clicks;
    rmp->mp_seg[S].mem_len = stack_clicks;
    // All these are size_t or uint64_t, ensure consistent types for arithmetic before assignment
    rmp->mp_seg[S].mem_phys = rmp->mp_seg[D].mem_phys + static_cast<uint64_t>(data_clicks) +
                              static_cast<uint64_t>(gap_clicks);
    sys_newmap(who, rmp->mp_seg); /* report new map to the kernel */

    /* Zero the bss, gap, and stack segment. Start just above text.  */
    for (rzp = &bf[0]; rzp < &bf[zs]; rzp++)
        *rzp = 0; /* clear buffer */
    // data_clicks, stack_clicks are std::size_t, gap_clicks is int64_t
    bytes = static_cast<uint64_t>(static_cast<int64_t>(data_clicks) + gap_clicks +
                                  static_cast<int64_t>(stack_clicks))
            << CLICK_SHIFT;
    vzb = reinterpret_cast<std::size_t>(bf);
    // rmp->mp_seg[T].mem_phys is uint64_t (phys_clicks)
    // rmp->mp_seg[T].mem_len is std::size_t (vir_clicks)
    base = (rmp->mp_seg[T].mem_phys + static_cast<uint64_t>(rmp->mp_seg[T].mem_len)) << CLICK_SHIFT;
    bss_offset = (data_bytes >> CLICK_SHIFT) << CLICK_SHIFT; // data_bytes is std::size_t
    base += bss_offset;
    // bytes is uint64_t, bss_offset is std::size_t.
    bytes -= static_cast<uint64_t>(bss_offset);

    while (bytes > 0) {
        // bytes is uint64_t, zs is int.
        count = std::min(bytes, static_cast<uint64_t>(zs));
        // vzb (std::size_t, pointer value) -> uintptr_t
        // base (uint64_t, physical address) -> uintptr_t (assuming it fits)
        // count (uint64_t, byte count) -> std::size_t (assuming it fits)
        if (mem_copy(MM_PROC_NR, D, static_cast<uintptr_t>(vzb), ABS, 0,
                     static_cast<uintptr_t>(base), static_cast<std::size_t>(count)) != OK)
            panic("new_mem can't zero", NO_NUM);
        base += count;
        bytes -= count;
    }
    return (OK);
}

/*===========================================================================*
 *				patch_ptr				     *
 *===========================================================================*/
/**
 * @brief Relocate pointers on the stack image.
 *
 * Converts relative argument and environment pointers to absolute
 * addresses based on the new stack base.
 *
 * @param stack Stack image buffer.
 * @param base  Base address of the stack in user space.
 */
PRIVATE void patch_ptr(char stack[MAX_ISTACK_BYTES], std::size_t base)
// base is std::size_t (vir_bytes)
{
    /* When doing an exec(name, argv, envp) call, the user builds up a stack
     * image with arg and env pointers relative to the start of the stack.  Now
     * these pointers must be relocated, since the stack is not positioned at
     * address 0 in the user's address space.
     */

    char **ap, flag;
    std::size_t v; // vir_bytes -> std::size_t

    flag = 0;            /* counts number of 0-pointers seen */
    ap = (char **)stack; /* points initially to 'nargs' */
    ap++;                /* now points to argv[0] */
    while (flag < 2) {
        if (ap >= (char **)&stack[MAX_ISTACK_BYTES])
            return; /* too bad */
        if (*ap != NIL_PTR) {
            v = reinterpret_cast<std::size_t>(*ap); /* v is relative pointer */
            v += base;                              /* relocate it */
            *ap = reinterpret_cast<char *>(v);      /* put it back */
        } else {
            flag++;
        }
        ap++;
    }
}

/*===========================================================================*
 *				load_seg				     *
 *===========================================================================*/
/**
 * @brief Load a segment from the executable into memory.
 *
 * @param fd        File descriptor of the executable.
 * @param seg       Segment index (T or D).
 * @param seg_bytes Number of bytes to read.
 */
PRIVATE void load_seg(int fd, int seg, std::size_t seg_bytes)
// fd, seg are int
// seg_bytes is std::size_t (vir_bytes)
{
    /* Read in text or data from the exec file and copy to the new core image.
     * This procedure is a little bit tricky.  The logical way to load a segment
     * would be to read it block by block and copy each block to the user space
     * one at a time.  This is too slow, so we do something dirty here, namely
     * send the user space and virtual address to the file system in the upper
     * 10 bits of the file descriptor, and pass it the user virtual address
     * instead of a MM address.  The file system copies the whole segment
     * directly to user space, bypassing MM completely.
     */

    int new_fd;
    // char *ubuf_ptr; // Not strictly needed as type, but for clarity.
    std::size_t bytes_to_read;

    if (seg_bytes == 0)
        return;                            /* text size for combined I & D is 0 */
    new_fd = (who << 8) | (seg << 6) | fd; // who is int
    // mp->mp_seg[seg].mem_vir is vir_clicks (std::size_t)
    char *ubuf_ptr = reinterpret_cast<char *>(mp->mp_seg[seg].mem_vir << CLICK_SHIFT);
    bytes_to_read = seg_bytes; // seg_bytes is std::size_t
    // read() system call 3rd arg is size_t.
    read(new_fd, ubuf_ptr, bytes_to_read);
}
