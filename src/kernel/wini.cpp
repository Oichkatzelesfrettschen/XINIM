/* This file contains a driver for the IBM or DTC winchester controller.
 * It was written by Adri Koppes.
 */

#include "const.hpp"
#include "glo.hpp"
#include "panic.hpp"
#include "proc.hpp"
#include "sys/callnr.hpp"
#include "sys/com.hpp"
#include "sys/const.hpp"
#include "sys/error.hpp"
#include "sys/type.hpp"
#include "type.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

// External kernel functions not in proc.hpp
extern "C" {
unsigned char get_byte(unsigned int seg, unsigned int off) noexcept;
}

/* RAII helper ensuring critical sections use lock/unlock */
class ScopedPortLock {
public:
    ScopedPortLock() { lock(); }
    ~ScopedPortLock() { restore(); }
};

/* I/O Ports used by winchester disk task. */
inline constexpr std::uint16_t WIN_DATA{0x320};
inline constexpr std::uint16_t WIN_STATUS{0x321};
inline constexpr std::uint16_t WIN_SELECT{0x322};
inline constexpr std::uint16_t WIN_DMA{0x323};
inline constexpr std::uint16_t DMA_ADDR{0x006};
inline constexpr std::uint16_t DMA_TOP{0x082};
inline constexpr std::uint16_t DMA_COUNT{0x007};
inline constexpr std::uint16_t DMA_M2{0x00C};
inline constexpr std::uint16_t DMA_M1{0x00B};
inline constexpr std::uint16_t DMA_INIT{0x00A};

inline constexpr int WIN_RECALIBRATE{0x01};
inline constexpr int WIN_SENSE{0x03};
inline constexpr int WIN_READ{0x08};
inline constexpr int WIN_WRITE{0x0A};
inline constexpr int WIN_SPECIFY{0x0C};
inline constexpr int WIN_ECC_READ{0x0D};

inline constexpr int DMA_INT_VAL{3};
inline constexpr int NO_DMA_INT{0};
inline constexpr int CTRL_BYTE{5};

inline constexpr int DMA_READ_VAL{0x47};
inline constexpr int DMA_WRITE_VAL{0x4B};

inline constexpr int SECTOR_SIZE{512};
inline constexpr int NR_SECTORS{0x11};

inline constexpr int ERR{-1};

inline constexpr int MAX_ERRORS{4};
inline constexpr int MAX_RESULTS{4};
inline constexpr int NR_DEVICES{10};
inline constexpr int MAX_WIN_RETRY{10000};
inline constexpr int PART_TABLE{0x1C6};
inline constexpr int DEV_PER_DRIVE{5};

PRIVATE struct wini {
    int wn_opcode;
    int wn_procnr;
    int wn_drive;
    int wn_cylinder;
    int wn_sector;
    int wn_head;
    int wn_heads;
    uint64_t wn_low;
    uint64_t wn_size;
    std::size_t wn_count;
    std::size_t wn_address;
    std::array<char, MAX_RESULTS> wn_results{};
} wini[NR_DEVICES];

PRIVATE int w_need_reset = FALSE;
PRIVATE int nr_drives;
PRIVATE message w_mess;
PRIVATE std::array<int, 6> command{};
PRIVATE unsigned char buf[BLOCK_SIZE];

PRIVATE struct param {
    int nr_cyl;
    int nr_heads;
    int reduced_wr;
    int wr_precomp;
    int max_ecc;
} param0, param1;

static int w_do_rdwt(message *m_ptr) noexcept;
static void w_dma_setup(struct wini *wn) noexcept;
static int w_transfer(struct wini &wn) noexcept;
static int win_results(struct wini &wn) noexcept;
static void win_out(int val) noexcept;
static int w_reset() noexcept;
static int win_init() noexcept;
static int check_init() noexcept;
static int read_ecc() noexcept;
static int hd_wait(int bit) noexcept;
static int com_out(std::span<const int> cmd, int mode) noexcept;
static void init_params() noexcept;
static void copy_params(unsigned char *src, struct param *dest) noexcept;
static void copy_prt(int drive) noexcept;
static void sort(struct wini *wn) noexcept;
static void swap(struct wini *first, struct wini *second) noexcept;

extern "C" void winchester_task() noexcept {
    int r, caller, p_nr;
    init_params();
    while (TRUE) {
        ipc_receive(ANY, &w_mess);
        if (w_mess.m_source < 0) {
            printf("winchester task got message from %d ", w_mess.m_source);
            continue;
        }
        caller = w_mess.m_source;
        p_nr = proc_nr(w_mess);
        switch (w_mess.m_type) {
        case DISK_READ:
        case DISK_WRITE:
            r = w_do_rdwt(&w_mess);
            break;
        default:
            r = static_cast<int>(ErrorCode::EINVAL);
            break;
        }
        w_mess.m_type = TASK_REPLY;
        rep_proc_nr(w_mess) = p_nr;
        rep_status(w_mess) = r;
        ipc_send(caller, &w_mess);
    }
}

static int w_do_rdwt(message *m_ptr) noexcept {
    struct wini *wn;
    int r, dev, errors = 0;
    int64_t sect;
    dev = device(*m_ptr);
    if (dev < 0 || dev >= NR_DEVICES)
        return static_cast<int>(ErrorCode::EIO);
    if (count(*m_ptr) != BLOCK_SIZE)
        return static_cast<int>(ErrorCode::EINVAL);
    wn = &wini[dev];
    wn->wn_drive = dev / DEV_PER_DRIVE;
    if (wn->wn_drive >= nr_drives)
        return static_cast<int>(ErrorCode::EIO);
    wn->wn_opcode = m_ptr->m_type;
    if (position(*m_ptr) % BLOCK_SIZE != 0)
        return static_cast<int>(ErrorCode::EINVAL);
    sect = position(*m_ptr) / SECTOR_SIZE;
    if ((sect + static_cast<int64_t>(BLOCK_SIZE / SECTOR_SIZE)) > static_cast<int64_t>(wn->wn_size))
        return (EOF);
    sect += static_cast<int64_t>(wn->wn_low);
    wn->wn_cylinder = static_cast<int>(sect / (wn->wn_heads * NR_SECTORS));
    wn->wn_sector = static_cast<int>(sect % NR_SECTORS);
    wn->wn_head = static_cast<int>((sect % (wn->wn_heads * NR_SECTORS)) / NR_SECTORS);
    wn->wn_count = static_cast<std::size_t>(count(*m_ptr));
    wn->wn_address = reinterpret_cast<std::size_t>(address(*m_ptr));
    wn->wn_procnr = proc_nr(*m_ptr);
    while (errors <= MAX_ERRORS) {
        errors++;
        if (errors >= MAX_ERRORS)
            return static_cast<int>(ErrorCode::EIO);
        if (w_need_reset)
            w_reset();
        w_dma_setup(wn);
        r = w_transfer(*wn);
        if (r == OK)
            break;
    }
    return (r == OK ? BLOCK_SIZE : static_cast<int>(ErrorCode::EIO));
}

static void w_dma_setup(struct wini *wn) noexcept {
    int mode, low_addr, high_addr, top_addr, low_ct, high_ct, top_end;
    std::size_t vir, ct;
    uint64_t user_phys;
    mode = (wn->wn_opcode == DISK_READ ? DMA_READ_VAL : DMA_WRITE_VAL);
    vir = wn->wn_address;
    ct = wn->wn_count;
    user_phys = umap(proc_addr(wn->wn_procnr), D, vir, ct);
    low_addr = static_cast<int>(user_phys & 0xFF);
    high_addr = static_cast<int>((user_phys >> 8) & 0xFF);
    top_addr = static_cast<int>((user_phys >> 16) & 0xFF);
    low_ct = static_cast<int>((ct - 1) & 0xFF);
    high_ct = static_cast<int>(((ct - 1) >> 8) & 0xFF);
    if (user_phys == 0)
        kpanic("FS gave winchester disk driver bad addr");
    top_end = static_cast<int>(((user_phys + ct - 1) >> 16) & 0xFF);
    if (top_end != top_addr)
        kpanic("Trying to DMA across 64K boundary");
    {
        ScopedPortLock guard;
        port_out(DMA_M2, static_cast<unsigned>(mode));
        port_out(DMA_M1, static_cast<unsigned>(mode));
        port_out(DMA_ADDR, static_cast<unsigned>(low_addr));
        port_out(DMA_ADDR, static_cast<unsigned>(high_addr));
        port_out(DMA_TOP, static_cast<unsigned>(top_addr));
        port_out(DMA_COUNT, static_cast<unsigned>(low_ct));
        port_out(DMA_COUNT, static_cast<unsigned>(high_ct));
    }
}

static int w_transfer(struct wini &wn) noexcept {
    command[0] = (wn.wn_opcode == DISK_READ ? WIN_READ : WIN_WRITE);
    command[1] = (wn.wn_head | (wn.wn_drive << 5));
    command[2] = (((wn.wn_cylinder & 0x0300) >> 2) | wn.wn_sector);
    command[3] = (wn.wn_cylinder & 0xFF);
    command[4] = BLOCK_SIZE / SECTOR_SIZE;
    command[5] = CTRL_BYTE;
    if (com_out(command, DMA_INT_VAL) != OK)
        return (ERR);
    port_out(DMA_INIT, 3);
    ipc_receive(HARDWARE, &w_mess);
    if (win_results(wn) == OK)
        return (OK);
    if ((wn.wn_results[0] & 63) == 24)
        read_ecc();
    else
        w_need_reset = TRUE;
    return (ERR);
}

static int win_results(struct wini &wn) noexcept {
    unsigned int status;
    port_in(WIN_DATA, &status);
    port_out(WIN_DMA, 0);
    if (!(status & 2))
        return (OK);
    command[0] = WIN_SENSE;
    command[1] = (wn.wn_drive << 5);
    if (com_out(command, NO_DMA_INT) != OK)
        return (ERR);
    for (auto &res : std::span<char, MAX_RESULTS>{wn.wn_results}) {
        if (hd_wait(1) != OK)
            return (ERR);
        port_in(WIN_DATA, &status);
        res = static_cast<char>(status & 0xFF);
    }
    if (wn.wn_results[0] & 63)
        return (ERR);
    else
        return (OK);
}

static void win_out(int val) noexcept {
    if (w_need_reset)
        return;
    if (hd_wait(1) == OK)
        port_out(WIN_DATA, static_cast<unsigned>(val));
}

static int w_reset() noexcept {
    unsigned int r = 1;
    port_out(WIN_STATUS, r);
    for (int i = 0; i < 10000; i++) {
        port_in(WIN_STATUS, &r);
        if ((r & 01) == 0)
            break;
    }
    if (r & 2) {
        printf("Hard disk won't reset\n");
        return (ERR);
    }
    w_need_reset = FALSE;
    return (win_init());
}

static int win_init() noexcept {
    command[0] = WIN_SPECIFY;
    command[1] = 0;
    if (com_out(command, NO_DMA_INT) != OK)
        return (ERR);
    {
        ScopedPortLock guard;
        win_out(param0.nr_cyl >> 8);
        win_out(param0.nr_cyl & 0xFF);
        win_out(param0.nr_heads);
        win_out(param0.reduced_wr >> 8);
        win_out(param0.reduced_wr & 0xFF);
        win_out(param0.wr_precomp >> 8);
        win_out(param0.wr_precomp & 0xFF);
        win_out(param0.max_ecc);
    }
    if (check_init() != OK) {
        w_need_reset = TRUE;
        return (ERR);
    }
    if (nr_drives > 1) {
        command[1] = (1 << 5);
        if (com_out(command, NO_DMA_INT) != OK)
            return (ERR);
        {
            ScopedPortLock guard;
            win_out(param1.nr_cyl >> 8);
            win_out(param1.nr_cyl & 0xFF);
            win_out(param1.nr_heads);
            win_out(param1.reduced_wr >> 8);
            win_out(param1.reduced_wr & 0xFF);
            win_out(param1.wr_precomp >> 8);
            win_out(param1.wr_precomp & 0xFF);
            win_out(param1.max_ecc);
        }
        if (check_init() != OK) {
            w_need_reset = TRUE;
            return (ERR);
        }
    }
    for (int i = 0; i < nr_drives; i++) {
        command[0] = WIN_RECALIBRATE;
        command[1] = i << 5;
        command[5] = CTRL_BYTE;
        if (com_out(command, 2) != OK)
            return (ERR); // 2 is INT
        ipc_receive(HARDWARE, &w_mess);
        if (win_results(wini[i * DEV_PER_DRIVE]) != OK) {
            w_need_reset = TRUE;
            return (ERR);
        }
    }
    return (OK);
}

static int check_init() noexcept {
    unsigned int r;
    if (hd_wait(2) == OK) {
        port_in(WIN_DATA, &r);
        if (r & 2)
            return (ERR);
        else
            return (OK);
    }
    return (ERR);
}

static int read_ecc() noexcept {
    unsigned int r;
    command[0] = WIN_ECC_READ;
    if (com_out(command, NO_DMA_INT) == OK && hd_wait(1) == OK) {
        port_in(WIN_DATA, &r);
        if (hd_wait(1) == OK) {
            port_in(WIN_DATA, &r);
            if (r & 1)
                w_need_reset = TRUE;
        }
    }
    return (ERR);
}

static int hd_wait(int bit) noexcept {
    int i = 0;
    unsigned int r;
    do {
        port_in(WIN_STATUS, &r);
        r &= static_cast<unsigned>(bit);
    } while ((i++ < MAX_WIN_RETRY) && !r);
    if (i >= MAX_WIN_RETRY) {
        w_need_reset = TRUE;
        return (ERR);
    } else
        return (OK);
}

static int com_out(std::span<const int> cmd, int mode) noexcept {
    unsigned int r;
    port_out(WIN_SELECT, static_cast<unsigned>(mode));
    port_out(WIN_DMA, static_cast<unsigned>(mode));
    int i = 0;
    for (i = 0; i < MAX_WIN_RETRY; i++) {
        port_in(WIN_STATUS, &r);
        if ((r & 0x0F) == 0x0D)
            break;
    }
    if (i == MAX_WIN_RETRY) {
        w_need_reset = TRUE;
        return (ERR);
    }
    {
        ScopedPortLock guard;
        for (const auto val : cmd)
            port_out(WIN_DATA, static_cast<unsigned>(val));
    }
    port_in(WIN_STATUS, &r);
    if (r & 1) {
        w_need_reset = TRUE;
        return (ERR);
    }
    return (OK);
}

static void init_params() noexcept {
    unsigned int segment, offset;
    int type_0, type_1;
    uint64_t addr;
    extern int vec_table[];
    unsigned int i_val;
    port_in(WIN_SELECT, &i_val);
    type_0 = (i_val >> 2) & 3;
    type_1 = i_val & 3;
    offset = static_cast<unsigned>(vec_table[2 * 0x41]);
    segment = static_cast<unsigned>(vec_table[2 * 0x41 + 1]);
    addr = (static_cast<uint64_t>(segment) << 4) + offset;

    uint64_t phys_buf = umap(proc_addr(WINCHESTER), D, reinterpret_cast<std::size_t>(buf),
                             static_cast<std::size_t>(64));
    phys_copy(reinterpret_cast<void *>(static_cast<uintptr_t>(phys_buf)),
              reinterpret_cast<const void *>(static_cast<uintptr_t>(addr)), 64ULL);

    copy_params((&buf[type_0 * 16]), &param0);
    copy_params((&buf[type_1 * 16]), &param1);

    uint64_t phys_buf_1 = umap(proc_addr(WINCHESTER), D, reinterpret_cast<std::size_t>(buf),
                               static_cast<std::size_t>(1));
    phys_copy(reinterpret_cast<void *>(static_cast<uintptr_t>(phys_buf_1)),
              reinterpret_cast<const void *>(0x475ULL), 1ULL);

    nr_drives = static_cast<int>(*buf);
    for (int i = 0; i < 5; i++)
        wini[i].wn_heads = param0.nr_heads;
    wini[0].wn_low = wini[5].wn_low = 0L;
    wini[0].wn_size = static_cast<uint64_t>(param0.nr_cyl) *
                      static_cast<uint64_t>(param0.nr_heads) * static_cast<uint64_t>(NR_SECTORS);
    for (int i = 5; i < 10; i++)
        wini[i].wn_heads = param1.nr_heads;
    wini[5].wn_size = static_cast<uint64_t>(param1.nr_cyl) *
                      static_cast<uint64_t>(param1.nr_heads) * static_cast<uint64_t>(NR_SECTORS);
    if ((nr_drives > 0) && (win_init() != OK))
        nr_drives = 0;
    for (int i = 0; i < nr_drives; i++) {
        device(w_mess) = i * 5;
        position(w_mess) = 0LL;
        count(w_mess) = BLOCK_SIZE;
        address(w_mess) = reinterpret_cast<char *>(buf);
        proc_nr(w_mess) = WINCHESTER;
        w_mess.m_type = DISK_READ;
        if (w_do_rdwt(&w_mess) != BLOCK_SIZE)
            kpanic("Can't read partition table");
        copy_prt(i * 5);
    }
}

static void copy_params(unsigned char *src, struct param *dest) noexcept {
    dest->nr_cyl = *reinterpret_cast<int *>(src);
    dest->nr_heads = static_cast<int>(src[2]);
    dest->reduced_wr = *reinterpret_cast<int *>(&src[3]);
    dest->wr_precomp = *reinterpret_cast<int *>(&src[5]);
    dest->max_ecc = static_cast<int>(src[7]);
}

static void copy_prt(int drive) noexcept {
    struct wini *wn;
    uint64_t temp_v;
    int64_t adj;
    for (int i = 0; i < 4; i++) {
        adj = 0;
        wn = &wini[i + drive + 1];
        int off = PART_TABLE + i * 0x10;
        memcpy(&temp_v, &buf[off], 4);
        wn->wn_low = temp_v;
        if ((wn->wn_low % (BLOCK_SIZE / SECTOR_SIZE)) != 0) {
            adj = static_cast<int64_t>(wn->wn_low);
            wn->wn_low = (wn->wn_low / (BLOCK_SIZE / SECTOR_SIZE) + 1) * (BLOCK_SIZE / SECTOR_SIZE);
            adj = static_cast<int64_t>(wn->wn_low) - adj;
        }
        memcpy(&temp_v, &buf[off + 4], 4);
        wn->wn_size = temp_v - static_cast<uint64_t>(adj);
    }
    sort(&wini[drive + 1]);
}

static void sort(struct wini *wn) noexcept {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 3; j++)
            if ((wn[j].wn_low == 0) && (wn[j + 1].wn_low != 0))
                swap(&wn[j], &wn[j + 1]);
            else if (wn[j].wn_low > wn[j + 1].wn_low && wn[j + 1].wn_low != 0)
                swap(&wn[j], &wn[j + 1]);
}

static void swap(struct wini *first, struct wini *second) noexcept {
    struct wini tmp = *first;
    *first = *second;
    *second = tmp;
}
