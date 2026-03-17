#pragma once

#include <stdint.h>

namespace xinim::i486::ksocket {

// Socket address families
constexpr uint16_t AF_INET = 2U;

// Socket types
constexpr int SOCK_STREAM = 1;
constexpr int SOCK_DGRAM = 2;

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
    // TCP state
    uint32_t tcp_seq;
    uint32_t tcp_ack;
    uint16_t tcp_window;
    // Receive buffer
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

// Called from netstack when UDP/TCP data arrives
void deliver_udp(const uint8_t* src_ip, uint16_t src_port,
                 uint16_t dst_port, const uint8_t* data, uint32_t length) noexcept;

} // namespace xinim::i486::ksocket
