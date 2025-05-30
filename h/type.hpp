#pragma once
// Modernized for C++17

#ifndef TYPE_H
#define TYPE_H

/* Pull in the fixed-width integer typedefs. */
#include "../include/defs.hpp" // project-wide integer definitions
#include <cstddef>             // for std::size_t

/* Utility functions -------------------------------------------------------*/
/*!
 * \brief Return the larger of two values.
 */
template <typename T> constexpr T max(T a, T b) { return a > b ? a : b; }

/*!
 * \brief Return the smaller of two values.
 */
template <typename T> constexpr T min(T a, T b) { return a < b ? a : b; }

/* Type definitions */
typedef unsigned short unshort;         /* must be 16-bit unsigned */
typedef unshort block_nr;               /* block number */
inline constexpr block_nr kNoBlock = 0; /* absence of a block number */
inline constexpr block_nr kMaxBlockNr = 0177777;

typedef unshort inode_nr;                        /* inode number */
inline constexpr inode_nr kNoEntry = 0;          /* absence of a dir entry */
inline constexpr inode_nr kMaxInodeNr = 0177777; /* highest inode number */

typedef unshort zone_nr;              /* zone number */
inline constexpr zone_nr kNoZone = 0; /* absence of a zone number */
inline constexpr zone_nr kHighestZone = 0177777;

typedef unshort bit_nr; /* if inode_nr & zone_nr both unshort,
                           then also unshort, else long */

typedef long zone_type;    /* zone size */
typedef unshort mask_bits; /* mode bits */
typedef unshort dev_nr;    /* major | minor device number */
inline constexpr dev_nr kNoDev = static_cast<dev_nr>(~0);

typedef char links; /* number of links to an inode */
inline constexpr links kMaxLinks = 0177;

typedef long real_time; /* real time in seconds since Jan 1, 1980 */
typedef long file_pos;  /* position in, or length of, a file */
inline constexpr long kMaxFilePos = 017777777777L;
typedef i64_t file_pos64; /* 64-bit file positions */
inline constexpr file_pos64 kMaxFilePos64 = I64_C(0x7fffffffffffffff);
typedef short int uid; /* user id */
typedef char gid;      /* group id */

typedef unsigned vir_bytes;   /* virtual addresses and lengths in bytes */
typedef unsigned vir_clicks;  /* virtual addresses and lengths in clicks */
typedef long phys_bytes;      /* physical addresses and lengths in bytes */
typedef unsigned phys_clicks; /* physical addresses and lengths in clicks */
typedef int signed_clicks;    /* same length as phys_clicks, but signed */

/* Types relating to messages. */
inline constexpr int M1 = 1;         /* style of message with three ints and three pointers */
inline constexpr int M3 = 3;         /* style of message with two ints and a string */
inline constexpr int M4 = 4;         /* style of message with four longs */
inline constexpr int M3_STRING = 14; /* length of M3's char array */

typedef struct {
    int m1i1, m1i2, m1i3;
    char *m1p1, *m1p2, *m1p3;
} mess_1;
typedef struct {
    int m2i1, m2i2, m2i3;
    long m2l1, m2l2;
    char *m2p1;
} mess_2;
typedef struct {
    int m3i1, m3i2;
    char *m3p1;
    char m3ca1[M3_STRING];
} mess_3;
typedef struct {
    long m4l1, m4l2, m4l3, m4l4;
} mess_4;
typedef struct {
    char m5c1, m5c2;
    int m5i1, m5i2;
    long m5l1, m5l2, m5l3;
} mess_5;
typedef struct {
    int m6i1, m6i2, m6i3;
    long m6l1;
    int (*m6f1)();
} mess_6;

typedef struct {
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
    constexpr long &m2_l1() { return m_u.m_m2.m2l1; }
    constexpr const long &m2_l1() const { return m_u.m_m2.m2l1; }
    constexpr long &m2_l2() { return m_u.m_m2.m2l2; }
    constexpr const long &m2_l2() const { return m_u.m_m2.m2l2; }
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

    constexpr long &m4_l1() { return m_u.m_m4.m4l1; }
    constexpr const long &m4_l1() const { return m_u.m_m4.m4l1; }
    constexpr long &m4_l2() { return m_u.m_m4.m4l2; }
    constexpr const long &m4_l2() const { return m_u.m_m4.m4l2; }
    constexpr long &m4_l3() { return m_u.m_m4.m4l3; }
    constexpr const long &m4_l3() const { return m_u.m_m4.m4l3; }
    constexpr long &m4_l4() { return m_u.m_m4.m4l4; }
    constexpr const long &m4_l4() const { return m_u.m_m4.m4l4; }

    constexpr char &m5_c1() { return m_u.m_m5.m5c1; }
    constexpr const char &m5_c1() const { return m_u.m_m5.m5c1; }
    constexpr char &m5_c2() { return m_u.m_m5.m5c2; }
    constexpr const char &m5_c2() const { return m_u.m_m5.m5c2; }
    constexpr int &m5_i1() { return m_u.m_m5.m5i1; }
    constexpr const int &m5_i1() const { return m_u.m_m5.m5i1; }
    constexpr int &m5_i2() { return m_u.m_m5.m5i2; }
    constexpr const int &m5_i2() const { return m_u.m_m5.m5i2; }
    constexpr long &m5_l1() { return m_u.m_m5.m5l1; }
    constexpr const long &m5_l1() const { return m_u.m_m5.m5l1; }
    constexpr long &m5_l2() { return m_u.m_m5.m5l2; }
    constexpr const long &m5_l2() const { return m_u.m_m5.m5l2; }
    constexpr long &m5_l3() { return m_u.m_m5.m5l3; }
    constexpr const long &m5_l3() const { return m_u.m_m5.m5l3; }

    constexpr int &m6_i1() { return m_u.m_m6.m6i1; }
    constexpr const int &m6_i1() const { return m_u.m_m6.m6i1; }
    constexpr int &m6_i2() { return m_u.m_m6.m6i2; }
    constexpr const int &m6_i2() const { return m_u.m_m6.m6i2; }
    constexpr int &m6_i3() { return m_u.m_m6.m6i3; }
    constexpr const int &m6_i3() const { return m_u.m_m6.m6i3; }
    constexpr long &m6_l1() { return m_u.m_m6.m6l1; }
    constexpr const long &m6_l1() const { return m_u.m_m6.m6l1; }
    constexpr int (*&m6_f1())() { return m_u.m_m6.m6f1; }
    constexpr int (*const &m6_f1() const)() { return m_u.m_m6.m6f1; }
} message;

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
