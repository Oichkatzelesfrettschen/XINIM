#pragma once

#include <stdint.h>

namespace xinim::i486::ksocket {

// Socket address families
constexpr uint16_t AF_INET = 2U;
constexpr uint16_t AF_UNIX = 1U;

// Socket types
constexpr int SOCK_STREAM = 1;
constexpr int SOCK_DGRAM = 2;

// Socket option levels
constexpr int SOL_SOCKET = 1;
constexpr int IPPROTO_TCP = 6;

// Socket options (SOL_SOCKET)
constexpr int SO_REUSEADDR = 2;
constexpr int SO_KEEPALIVE = 9;
constexpr int SO_SNDBUF = 7;
constexpr int SO_RCVBUF = 8;

// TCP options (IPPROTO_TCP)
constexpr int TCP_NODELAY = 1;

// Socket state
enum class SocketState : uint8_t {
    Closed = 0,
    Bound = 1,
    Listening = 2,
    Connected = 3,
    SynSent = 4,
    SynRecv = 5,
    Established = 6,
    CloseWait = 7,
};

struct [[gnu::packed]] SockAddrIn {
    uint16_t family;
    uint16_t port;      // Network byte order
    uint8_t  addr[4];   // Network byte order
    uint8_t  zero[8];
};

struct Socket {
    bool in_use;
    int type;           // SOCK_STREAM or SOCK_DGRAM
    SocketState state;
    uint16_t local_port;
    uint16_t remote_port;
    uint8_t local_addr[4];
    uint8_t remote_addr[4];
    // TCP connection index into tcp.cpp's g_connections table
    int tcp_conn_idx;
    // Socket option flags
    bool so_reuseaddr;
    bool so_keepalive;
    // Receive buffer (for UDP)
    uint8_t rx_buf[4096];
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t rx_count;
};

constexpr uint32_t MAX_SOCKETS = 8U;

// Syscall handlers (called from ring3.cpp)
int sys_socket(int domain, int type, int protocol) noexcept;
int sys_bind(int sockfd, const SockAddrIn* addr) noexcept;
int sys_connect(int sockfd, const SockAddrIn* addr) noexcept;
int sys_listen(int sockfd, int backlog) noexcept;
int sys_accept(int sockfd, SockAddrIn* addr) noexcept;
int sys_sendto(int sockfd, const void* buf, uint32_t len,
               const SockAddrIn* dest_addr) noexcept;
int sys_recvfrom(int sockfd, void* buf, uint32_t len,
                 SockAddrIn* src_addr) noexcept;
int sys_shutdown(int sockfd, int how) noexcept;
int sys_close(int sockfd) noexcept;
int sys_setsockopt(int sockfd, int level, int optname,
                   const void* optval, uint32_t optlen) noexcept;
int sys_getsockopt(int sockfd, int level, int optname,
                   void* optval, uint32_t* optlen) noexcept;
int sys_getsockname(int sockfd, SockAddrIn* addr) noexcept;
int sys_getpeername(int sockfd, SockAddrIn* addr) noexcept;
int sys_socketpair(int domain, int type, int protocol, int sv[2]) noexcept;

// i386 struct iovec for scatter-gather I/O
struct IoVec32 {
    uint32_t iov_base;  // user pointer
    uint32_t iov_len;
};

// i386 struct msghdr
struct MsgHdr32 {
    uint32_t msg_name;       // optional address (SockAddrIn*)
    uint32_t msg_namelen;
    uint32_t msg_iov;        // IoVec32* array
    uint32_t msg_iovlen;     // count of IoVec32 entries
    uint32_t msg_control;    // ancillary data (not supported yet)
    uint32_t msg_controllen;
    int msg_flags;
};

int sys_sendmsg(int sockfd, const void* data, uint32_t len,
                const SockAddrIn* dest_addr) noexcept;
int sys_recvmsg(int sockfd, void* buf, uint32_t len,
                SockAddrIn* src_addr) noexcept;

// Get TCP connection index for a socket (for scheduler TcpConnect wakeup).
int get_tcp_conn_idx(int sockfd) noexcept;

// Called from netstack when UDP/TCP data arrives
void deliver_udp(const uint8_t* src_ip, uint16_t src_port,
                 uint16_t dst_port, const uint8_t* data, uint32_t length) noexcept;

} // namespace xinim::i486::ksocket
