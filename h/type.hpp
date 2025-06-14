#pragma once
// Modernized for C++17

#ifndef TYPE_H
#define TYPE_H

/* Pull in the fixed-width integer typedefs. */
#include "../include/defs.hpp"             // project-wide integer definitions
#include "../include/xinim/core_types.hpp" // Use project include path
#include <cstddef>                         // for std::size_t
#include <cstdint>                         // for fixed-width integer types

// Utility functions -------------------------------------------------------*/
/*!
 * rief Return the larger of two values.
 */
template <typename T> constexpr T max(T a, T b) noexcept { return a > b ? a : b; }

/*!
 * rief Return the smaller of two values.
 */
template <typename T> constexpr T min(T a, T b) noexcept { return a < b ? a : b; }

/* Type definitions */
// Minix specific types that don't directly map or are more granular than xinim::core_types
// will remain for now. Types that have a direct xinim equivalent will be commented
// out or aliased to the xinim type.
using unshort = uint16_t;               /* must be 16-bit unsigned */
using block_nr = uint16_t;              /* block number */
inline constexpr block_nr kNoBlock = 0; /* absence of a block number */
inline constexpr block_nr kMaxBlockNr = 0177777;

using inode_nr = uint16_t;                       /* inode number */
inline constexpr inode_nr kNoEntry = 0;          /* absence of a dir entry */
inline constexpr inode_nr kMaxInodeNr = 0177777; /* highest inode number */

using zone_nr = uint16_t;             /* zone number */
inline constexpr zone_nr kNoZone = 0; /* absence of a zone number */
inline constexpr zone_nr kHighestZone = 0177777;

using bit_nr = uint16_t; /* if inode_nr & zone_nr both unshort,
                           then also unshort, else long */

using zone_type = uint32_t; /* zone size */
using mask_bits = uint16_t; /* mode bits */
using dev_nr = uint16_t;    /* major | minor device number */
inline constexpr dev_nr kNoDev = static_cast<dev_nr>(~0);

using links = uint8_t; /* number of links to an inode */
inline constexpr links kMaxLinks = 0177;

// MODERNIZED: using real_time = int64_t;
using real_time = xinim::time_t; // Maps to xinim::time_t (std::int64_t)

using file_pos =
    int32_t; /* position in, or length of, a file - Minix specific, smaller than off_t */
inline constexpr file_pos kMaxFilePos = 017777777777;
using file_pos64 = i64_t; /* 64-bit file positions - Minix specific, similar to xinim::off_t if that
                             were defined */
inline constexpr file_pos64 kMaxFilePos64 = I64_C(0x7fffffffffffffff);
using uid = uint16_t; /* user id - Minix specific (xinim::uid_t is int32_t) */
using gid = uint8_t;  /* group id - Minix specific (xinim::gid_t is int32_t) */

// Aliases to types defined in xinim::core_types.hpp
using vir_bytes = xinim::vir_bytes_t;
using phys_bytes = xinim::phys_bytes_t;
using vir_clicks = xinim::virt_addr_t;  // Representing a count/size related to virtual memory units
                                        // based on address size
using phys_clicks = xinim::phys_addr_t; // Representing a count/size related to physical memory
                                        // units based on address size

using signed_clicks = int64_t; /* same length as phys_clicks, but signed - Minix specific */
                               // No direct xinim equivalent, related to Minix clicks.

/* Types relating to messages. */
inline constexpr int M1 = 1;         /* style of message with three ints and three pointers */
inline constexpr int M3 = 3;         /* style of message with two ints and a string */
inline constexpr int M4 = 4;         /* style of message with four longs */
inline constexpr int M3_STRING = 14; /* length of M3's char array */

struct mess_1 {
    int m1i1, m1i2, m1i3;
    char *m1p1, *m1p2, *m1p3;
};
struct mess_2 {
    int m2i1, m2i2, m2i3;
    int64_t m2l1, m2l2;
    char *m2p1;
};
struct mess_3 {
    int m3i1, m3i2;
    char *m3p1;
    char m3ca1[M3_STRING];
};
struct mess_4 {
    int64_t m4l1, m4l2, m4l3, m4l4;
};
struct mess_5 {
    char m5c1, m5c2;
    int m5i1, m5i2;
    int64_t m5l1, m5l2, m5l3;
};
struct mess_6 {
    int m6i1, m6i2, m6i3;
    int64_t m6l1;
    int (*m6f1)();
};

struct message {
    int m_source; /* who sent the message */
    int m_type;   /* what kind of message is it */
    union {
        mess_1 m_m1;
        mess_2 m_m2;
        mess_3 m_m3;
        mess_4 m_m4;
        mess_5 m_m5;
        mess_6 m_m6;
    } m_u;

    /* Accessors for the union fields. */
    constexpr int &m1_i1() { return m_u.m_m1.m1i1; }
    constexpr const int &m1_i1() const { return m_u.m_m1.m1i1; }
    constexpr int &m1_i2() { return m_u.m_m1.m1i2; }
    constexpr const int &m1_i2() const { return m_u.m_m1.m1i2; }
    constexpr int &m1_i3() { return m_u.m_m1.m1i3; }
    constexpr const int &m1_i3() const { return m_u.m_m1.m1i3; }
    constexpr char *&m1_p1() { return m_u.m_m1.m1p1; }
    constexpr char *const &m1_p1() const { return m_u.m_m1.m1p1; }
    constexpr char *&m1_p2() { return m_u.m_m1.m1p2; }
    constexpr char *const &m1_p2() const { return m_u.m_m1.m1p2; }
    constexpr char *&m1_p3() { return m_u.m_m1.m1p3; }
    constexpr char *const &m1_p3() const { return m_u.m_m1.m1p3; }

    constexpr int &m2_i1() { return m_u.m_m2.m2i1; }
    constexpr const int &m2_i1() const { return m_u.m_m2.m2i1; }
    constexpr int &m2_i2() { return m_u.m_m2.m2i2; }
    constexpr const int &m2_i2() const { return m_u.m_m2.m2i2; }
    constexpr int &m2_i3() { return m_u.m_m2.m2i3; }
    constexpr const int &m2_i3() const { return m_u.m_m2.m2i3; }
    constexpr int64_t &m2_l1() { return m_u.m_m2.m2l1; }
    constexpr const int64_t &m2_l1() const { return m_u.m_m2.m2l1; }
    constexpr int64_t &m2_l2() { return m_u.m_m2.m2l2; }
    constexpr const int64_t &m2_l2() const { return m_u.m_m2.m2l2; }
    constexpr char *&m2_p1() { return m_u.m_m2.m2p1; }
    constexpr char *const &m2_p1() const { return m_u.m_m2.m2p1; }

    constexpr int &m3_i1() { return m_u.m_m3.m3i1; }
    constexpr const int &m3_i1() const { return m_u.m_m3.m3i1; }
    constexpr int &m3_i2() { return m_u.m_m3.m3i2; }
    constexpr const int &m3_i2() const { return m_u.m_m3.m3i2; }
    constexpr char *&m3_p1() { return m_u.m_m3.m3p1; }
    constexpr char *const &m3_p1() const { return m_u.m_m3.m3p1; }
    constexpr char *m3_ca1() { return m_u.m_m3.m3ca1; }
    constexpr const char *m3_ca1() const { return m_u.m_m3.m3ca1; }

    constexpr int64_t &m4_l1() { return m_u.m_m4.m4l1; }
    constexpr const int64_t &m4_l1() const { return m_u.m_m4.m4l1; }
    constexpr int64_t &m4_l2() { return m_u.m_m4.m4l2; }
    constexpr const int64_t &m4_l2() const { return m_u.m_m4.m4l2; }
    constexpr int64_t &m4_l3() { return m_u.m_m4.m4l3; }
    constexpr const int64_t &m4_l3() const { return m_u.m_m4.m4l3; }
    constexpr int64_t &m4_l4() { return m_u.m_m4.m4l4; }
    constexpr const int64_t &m4_l4() const { return m_u.m_m4.m4l4; }

    constexpr char &m5_c1() { return m_u.m_m5.m5c1; }
    constexpr const char &m5_c1() const { return m_u.m_m5.m5c1; }
    constexpr char &m5_c2() { return m_u.m_m5.m5c2; }
    constexpr const char &m5_c2() const { return m_u.m_m5.m5c2; }
    constexpr int &m5_i1() { return m_u.m_m5.m5i1; }
    constexpr const int &m5_i1() const { return m_u.m_m5.m5i1; }
    constexpr int &m5_i2() { return m_u.m_m5.m5i2; }
    constexpr const int &m5_i2() const { return m_u.m_m5.m5i2; }
    constexpr int64_t &m5_l1() { return m_u.m_m5.m5l1; }
    constexpr const int64_t &m5_l1() const { return m_u.m_m5.m5l1; }
    constexpr int64_t &m5_l2() { return m_u.m_m5.m5l2; }
    constexpr const int64_t &m5_l2() const { return m_u.m_m5.m5l2; }
    constexpr int64_t &m5_l3() { return m_u.m_m5.m5l3; }
    constexpr const int64_t &m5_l3() const { return m_u.m_m5.m5l3; }

    constexpr int &m6_i1() { return m_u.m_m6.m6i1; }
    constexpr const int &m6_i1() const { return m_u.m_m6.m6i1; }
    constexpr int &m6_i2() { return m_u.m_m6.m6i2; }
    constexpr const int &m6_i2() const { return m_u.m_m6.m6i2; }
    constexpr int &m6_i3() { return m_u.m_m6.m6i3; }
    constexpr const int &m6_i3() const { return m_u.m_m6.m6i3; }
    constexpr int64_t &m6_l1() { return m_u.m_m6.m6l1; }
    constexpr const int64_t &m6_l1() const { return m_u.m_m6.m6l1; }
    constexpr int (*&m6_f1())() { return m_u.m_m6.m6f1; }             // Modern C-style: int(*)()
    constexpr int (*const &m6_f1() const)() { return m_u.m_m6.m6f1; } // Modern C-style: int(*)()
};

inline constexpr std::size_t kMessSize = sizeof(message);
inline constexpr message *kNilMess = nullptr;

struct mem_map {
    vir_clicks mem_vir;   /* virtual address */
    phys_clicks mem_phys; /* physical address */
    vir_clicks mem_len;   /* length */
};

struct copy_info { /* used by sys_copy(src, dst, bytes) */
    int cp_src_proc;
    int cp_src_space;
    vir_bytes cp_src_vir;
    int cp_dst_proc;
    int cp_dst_space;
    vir_bytes cp_dst_vir;
    vir_bytes cp_bytes;
};

#endif /* TYPE_H */
