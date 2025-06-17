/**
 * @file main.cpp
 * @brief Entry point and helper routines for the file system server.
 *
 * This module implements the main server loop that receives requests from other subsystems,
 * dispatches the appropriate handler and returns the result.  The helper functions initialize
 * global state, manage the buffer cache and load the RAM disk.
 */

#include "../h/callnr.hpp"
#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/error.hpp"
#include "../h/type.hpp"
#include "buf.hpp"
#include "const.hpp"
#include "file.hpp"
#include "fproc.hpp"
#include "glo.hpp"
#include "inode.hpp"
#include "param.hpp"
#include "super.hpp"
#include "type.hpp"
#include <cstddef>    // For std::size_t
#include <cstdint>    // For uint16_t, uint32_t, uint64_t, int64_t, int32_t, uint8_t
#include <inttypes.h> // For PRId64

#define M64K 0xFFFF0000L /* 16 bit mask for DMA check */
#define INFO 2           /* where in data_org is info from build */
#define MAX_RAM 512      /* maxium RAM disk size in blocks */

/**
 * @brief Entry point for the file system process.
 *
 * The main loop receives requests, dispatches them and sends the reply back to
 * the caller. It never terminates while the system is running.
 */
int main() {
    /* This is the main program of the file system.  The main loop consists of
     * three major activities: getting new work, processing the work, and sending
     * the reply.  This loop never terminates as long as the file system runs.
     */
    int error;
    extern int (*call_vector[NCALLS])();

    fs_init();

    /* This is the main loop that gets work, processes it, and sends replies. */
    while (TRUE) {
        get_work(); /* sets who and fs_call */

        fp = &fproc[who];                                      /* pointer to proc table struct */
        super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE); /* su? */
        dont_reply = FALSE; /* in other words, do reply is default */

        /* Call the internal function that does the work. */
        if (fs_call < 0 || fs_call >= NCALLS)
            error = ErrorCode::E_BAD_CALL;
        else
            error = (*call_vector[fs_call])();

        /* Copy the results back to the user and send reply. */
        if (dont_reply)
            continue;
        reply(who, error);
        if (rdahed_inode != NIL_INODE)
            read_ahead(); /* do block read ahead */
    }
    return 0; // main never returns but placate the compiler
}

/**
 * @brief Retrieve work from the message queue or resume a suspended process.
 */
static void get_work() {
    register struct fproc *rp;

    if (reviving != 0) {
        /* Revive a suspended process. */
        for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++)
            if (rp->fp_revived == REVIVING) {
                who = rp - fproc;
                fs_call = rp->fp_fd & BYTE;
                fd = (rp->fp_fd >> 8) & BYTE;
                buffer = rp->fp_buffer;
                nbytes = rp->fp_nbytes;
                rp->fp_suspended = NOT_SUSPENDED; /* no longer hanging*/
                rp->fp_revived = NOT_REVIVING;
                reviving--;
                return;
            }
        panic("get_work couldn't revive anyone", NO_NUM);
    }

    /* Normal case.  No one to revive. */
    if (receive(ANY, &m) != OK)
        panic("fs receive error", NO_NUM);

    who = m.m_source;
    fs_call = m.m_type;
}

/**
 * @brief Send a reply to a user process.
 *
 * The send may fail if the destination died. Failure is ignored.
 */
void reply(int whom, int result) {

    reply_type = result;
    send(whom, &m1);
}

/**
 * @brief Initialize buffers, super block and process table.
 */
static void fs_init() {

    buf_pool();   /* initialize buffer pool */
    load_ram();   /* Load RAM disk from root diskette. */
    load_super(); /* Load super block for root device */

    /* Initialize the 'fproc' fields for process 0 and process 2. */
    for (i = 0; i < 3; i += 2) {
        fp = &fproc[i];
        rip = get_inode(ROOT_DEV, ROOT_INODE);
        fp->fp_rootdir = rip;
        dup_inode(rip);
        fp->fp_workdir = rip;
        fp->fp_realuid = (uid)SYS_UID;
        fp->fp_effuid = (uid)SYS_UID;
        fp->fp_realgid = (gid)SYS_GID;
        fp->fp_effgid = (gid)SYS_GID;
        fp->fp_umask = ~0;
    }

    /* Certain relations must hold for the file system to work at all. */
    if (ZONE_NUM_SIZE != 2)
        panic("ZONE_NUM_SIZE != 2", NO_NUM);
    if (SUPER_SIZE > BLOCK_SIZE)
        panic("SUPER_SIZE > BLOCK_SIZE", NO_NUM);
    if (BLOCK_SIZE % INODE_SIZE != 0)
        panic("BLOCK_SIZE % INODE_SIZE != 0", NO_NUM);
    if (NR_FDS > 127)
        panic("NR_FDS > 127", NO_NUM);
    if (NR_BUFS < 6)
        panic("NR_BUFS < 6", NO_NUM);
    if (sizeof(d_inode) != 32)
        panic("inode size != 32", NO_NUM);
}

/**
 * @brief Set up the buffer cache used for block I/O.
 */
// Set up buffer pool for block I/O.
static void buf_pool() {
    /* Initialize the buffer pool.  On the IBM PC, the hardware DMA chip is
     * not able to cross 64K boundaries, so any buffer that happens to lie
     * across such a boundary is not used.  This is not very elegant, but all
     * the alternative solutions are as bad, if not worse.  The fault lies with
     * the PC hardware.
     */
    register struct buf *bp;
    std::size_t low_off, high_off; // vir_bytes -> std::size_t
    uint64_t org;                  // phys_bytes -> uint64_t
    extern uint64_t get_base();    // Assuming get_base() returns phys_clicks -> uint64_t

    bufs_in_use = 0;
    front = &buf[0];
    rear = &buf[NR_BUFS - 1];

    for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
        bp->b_blocknr = NO_BLOCK;
        bp->b_dev = kNoDev;
        bp->b_next = bp + 1;
        bp->b_prev = bp - 1;
    }
    buf[0].b_prev = NIL_BUF;
    buf[NR_BUFS - 1].b_next = NIL_BUF;

    /* Delete any buffers that span a 64K boundary. */

    for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++)
        bp->b_hash = bp->b_next;
    buf_hash[NO_BLOCK & (NR_BUF_HASH - 1)] = front;
}

/**
 * @brief Load the RAM disk from the root device.
 */
// Load the RAM disk from the root device.
static void load_ram() {
    /* The root diskette contains a block-by-block image of the root file system
     * starting at 0.  Go get it and copy it to the RAM disk.
     */

    register struct buf *bp, *bp1;
    uint32_t count;   // Was int, count of blocks
    int64_t k_loaded; // Was long, for printf
    struct super_block *sp;
    uint16_t i;                                                        // block_nr -> uint16_t
    uint64_t ram_clicks, init_org, init_text_clicks, init_data_clicks; // phys_clicks -> uint64_t
    extern uint64_t data_org[INFO + 2]; // phys_clicks[] -> uint64_t[]
    extern struct buf *get_block();

    /* Get size of INIT by reading block on diskette where 'build' put it. */
    init_org = data_org[INFO];
    init_text_clicks = data_org[INFO + 1];
    init_data_clicks = data_org[INFO + 2];

    /* Get size of RAM disk by reading root file system's super block */
    bp = get_block(BOOT_DEV, SUPER_BLOCK, NORMAL); /* get RAM super block */
    copy(super_block, bp->b_data, sizeof(struct super_block));
    sp = &super_block[0];
    if (sp->s_magic != SUPER_MAGIC)
        panic("Diskette in drive 0 is not root file system", NO_NUM);
    // sp->s_nzones is uint16_t, s_log_zone_size is uint8_t. Result fits uint32_t.
    count = static_cast<uint32_t>(sp->s_nzones) << sp->s_log_zone_size; /* # blocks on root dev */
    if (count > MAX_RAM)                                                // MAX_RAM is int
        panic("RAM disk is too big. # blocks = ", static_cast<int>(count)); // panic takes int
    // count is uint32_t, BLOCK_SIZE/CLICK_SIZE are int. Result fits uint64_t.
    ram_clicks = static_cast<uint64_t>(count) * (BLOCK_SIZE / CLICK_SIZE);
    put_block(bp, BlockType::FullData);

    /* Tell MM the origin and size of INIT, and the amount of memory used for the
     * system plus RAM disk combined, so it can remove all of it from the map.
     */
    m1.m_type = BRK2;
    // Message fields m1_i1, m1_i2, m1_i3 are int. init_text_clicks etc are uint64_t.
    // This is a narrowing conversion; assumes these click counts fit in int.
    m1.m1_i1() = static_cast<int>(init_text_clicks);
    m1.m1_i2() = static_cast<int>(init_data_clicks);
    m1.m1_i3() = static_cast<int>(init_org + init_text_clicks + init_data_clicks + ram_clicks);
    // m1_p1 is char*. init_org is uint64_t. Requires uintptr_t intermediate for safety.
    m1.m1_p1() = reinterpret_cast<char *>(static_cast<uintptr_t>(init_org));
    if (sendrec(MM_PROC_NR, &m1) != OK)
        panic("FS Can't report to MM", NO_NUM);

    /* Tell RAM driver where RAM disk is and how big it is. */
    m1.m_type = DISK_IOCTL;
    device(m1) = RAM_DEV; // RAM_DEV is dev_nr (uint16_t), device(m1) is int.
    // position(m1) is int64_t (was long). init_org etc are uint64_t. Sum is uint64_t.
    position(m1) = static_cast<int64_t>(init_org + init_text_clicks + init_data_clicks);
    position(m1) = position(m1) << CLICK_SHIFT; // CLICK_SHIFT is int.
    count(m1) = static_cast<int>(count);        // count(m1) is int, count is uint32_t. Narrowing.
    if (sendrec(MEM, &m1) != OK)
        panic("Can't report size to MEM", NO_NUM);

    /* Copy the blocks one at a time from the root diskette to the RAM */
    printf("Loading RAM disk from root diskette.      Loaded:   0K ");
    for (i = 0; i < count; i++) { // i is uint16_t, count is uint32_t
        bp = get_block(BOOT_DEV, static_cast<uint16_t>(i),
                       NORMAL); // get_block takes block_nr (uint16_t)
        bp1 = get_block(ROOT_DEV, i, NO_READ);
        copy(bp1->b_data, bp->b_data, BLOCK_SIZE);
        bp1->b_dirt = DIRTY;
        put_block(bp, BlockType::IMap);
        put_block(bp1, BlockType::IMap);
        // i is uint16_t, BLOCK_SIZE is int. Result of mult fits int64_t.
        k_loaded = (static_cast<int64_t>(i) * BLOCK_SIZE) / 1024L; /* K loaded so far */
        if (k_loaded % 5 == 0)
            printf("\b\b\b\b\b%3" PRId64 "K %c", k_loaded, 0);
    }

    printf("\rRAM disk loaded.  Please remove root diskette.           \n\n");
}

/**
 * @brief Load the super block for the root device.
 */
// Load the super block for the root device.
static void load_super() {
    register struct super_block *sp;
    register struct inode *rip;
    extern struct inode *get_inode();

    /* Initialize the super_block table. */

    for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
        sp->s_dev = kNoDev;

    /* Read in super_block for the root file system. */
    sp = &super_block[0];
    sp->s_dev = ROOT_DEV;
    rw_super(sp, READING);
    rip = get_inode(ROOT_DEV, ROOT_INODE); /* inode for root dir */

    /* Check super_block for consistency (is it the right diskette?). */
    if ((rip->i_mode & I_TYPE) != I_DIRECTORY || rip->i_nlinks < 3 || sp->s_magic != SUPER_MAGIC)
        panic("Root file system corrupted.  Possibly wrong diskette.", NO_NUM);

    sp->s_imount = rip;
    dup_inode(rip);
    sp->s_isup = rip;
    sp->s_rd_only = 0;
    if (load_bit_maps(ROOT_DEV) != OK)
        panic("init: can't load root bit maps", NO_NUM);
}
