/**
 * @file utility.cpp
 * @brief Miscellaneous helper routines for the file system server.
 *
 * The functions in this module provide time retrieval, string utilities and
 * helpers for fetching user space path names. They replace older C variants
 * with C++23 implementations.
 */

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
#include <algorithm>

static int panicking = 0; /**< prevents recursive panics during sync */
static message clock_mess{};

/**
 * @brief Return the current real time from the clock task.
 * @return Seconds since the Unix epoch.
 */
[[nodiscard]] real_time clock_time() {
    clock_mess.m_type = GET_TIME;
    int k = sendrec(CLOCK, &clock_mess);
    if (k != OK) {
        panic("clock_time err", k);
    }

    auto *sp = get_super(ROOT_DEV);
    sp->s_time = new_time(clock_mess);
    if (!sp->s_rd_only) {
        sp->s_dirt = DIRTY;
    }

    return static_cast<real_time>(new_time(clock_mess));
}
/**
 * @brief Compare two strings of length n.
 * @param rsp1 First string.
 * @param rsp2 Second string.
 * @param n Number of characters to compare.
 * @return 1 if the strings are identical, 0 otherwise.
 */
[[nodiscard]] int cmp_string(const char *rsp1, const char *rsp2, int n) {
    for (int i = 0; i < n; ++i) {
        if (rsp1[i] != rsp2[i]) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Copy a byte sequence.
 * @param dest Destination buffer.
 * @param source Source buffer.
 * @param bytes Number of bytes to move.
 */
inline void copy(void *dest, const void *source, int bytes) {
    if (bytes <= 0) {
        return;
    }
    std::memmove(dest, source, static_cast<size_t>(bytes));
}

/**
 * @brief Fetch a path name from user space.
 * @param path User space pointer to the path.
 * @param len Length of the path including the null terminator.
 * @param flag When set to M3, the path may reside in the incoming message.
 * @return OK on success or an error code.
 */
[[nodiscard]] int fetch_name(const char *path, int len, int flag) {
    if (flag == M3 && len <= M3_STRING) {
        std::copy_n(pathname, len, user_path);
        return OK;
    }
    if (len > MAX_PATH) {
        err_code = ErrorCode::E_LONG_STRING;
        return ERROR;
    }
    vir_bytes vpath = reinterpret_cast<vir_bytes>(path);
    err_code = rw_user(D, who, vpath, static_cast<vir_bytes>(len), user_path, FROM_USER);
    return err_code;
}

/**
 * @brief Handler for unsupported system calls.
 * @return Always returns ErrorCode::EINVAL.
 */
[[nodiscard]] int no_sys() { return static_cast<int>(ErrorCode::EINVAL); }

/**
 * @brief Panic handler that syncs all buffers and halts the system.
 * @param format Message
 * format string.
 * @param num Optional numeric argument printed with the message.
 */
void panic(const char *format, int num) {
    if (panicking) {
        return;
    }
    panicking = TRUE;
    printf("File system panic: %s ", format);
    if (num != NO_NUM) {
        printf("%d", num);
    }
    printf("\n");
    do_sync();
    sys_abort();
}
