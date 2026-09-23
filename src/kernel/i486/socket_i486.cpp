/**
 * @file socket_i486.cpp
 * @brief Kernel socket layer bridging syscalls to the netstack.
 */

#include "socket_i486.hpp"
#include "netstack.hpp"
#include "tcp.hpp"

namespace xinim::i486::ksocket {
namespace {

Socket g_sockets[MAX_SOCKETS]{};
uint16_t g_next_ephemeral_port = 49152U;

Socket* get_socket(int fd) noexcept {
    // Socket fds are offset by 1000 to avoid collision with file fds
    int idx = fd - 1000;
    if (idx < 0 || static_cast<uint32_t>(idx) >= MAX_SOCKETS) {
        return nullptr;
    }
    if (!g_sockets[idx].in_use) {
        return nullptr;
    }
    return &g_sockets[idx];
}

int allocate_socket() noexcept {
    for (uint32_t i = 0U; i < MAX_SOCKETS; ++i) {
        if (!g_sockets[i].in_use) {
            g_sockets[i] = {};
            g_sockets[i].in_use = true;
            return static_cast<int>(i) + 1000;
        }
    }
    return -1;
}

void copy4(uint8_t* dst, const uint8_t* src) noexcept {
    dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2]; dst[3]=src[3];
}

uint32_t rx_available(const Socket& s) noexcept {
    return s.rx_count;
}

uint32_t rx_read(Socket& s, uint8_t* buf, uint32_t len) noexcept {
    uint32_t read = 0U;
    while (read < len && s.rx_count > 0U) {
        buf[read++] = s.rx_buf[s.rx_tail % sizeof(s.rx_buf)];
        ++s.rx_tail;
        --s.rx_count;
    }
    return read;
}

void rx_write(Socket& s, const uint8_t* data, uint32_t len) noexcept {
    for (uint32_t i = 0U; i < len && s.rx_count < sizeof(s.rx_buf); ++i) {
        s.rx_buf[s.rx_head % sizeof(s.rx_buf)] = data[i];
        ++s.rx_head;
        ++s.rx_count;
    }
}

} // namespace

int sys_socket(int domain, int type, int /*protocol*/) noexcept {
    if (domain != AF_INET) {
        return -38; // ENOSYS
    }
    if (type != SOCK_DGRAM && type != SOCK_STREAM) {
        return -22; // EINVAL
    }

    int fd = allocate_socket();
    if (fd < 0) {
        return -12; // ENOMEM
    }

    Socket* s = get_socket(fd);
    if (s == nullptr) {
        return -12;
    }
    s->type = type;
    s->state = SocketState::Closed;
    return fd;
}

int sys_bind(int sockfd, const SockAddrIn* addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9; // EBADF
    }
    if (addr == nullptr) {
        return -14; // EFAULT
    }

    s->local_port = net::ntohs(addr->port);
    copy4(s->local_addr, addr->addr);
    s->state = SocketState::Bound;
    return 0;
}

int sys_connect(int sockfd, const SockAddrIn* addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (addr == nullptr) {
        return -14;
    }

    copy4(s->remote_addr, addr->addr);
    s->remote_port = net::ntohs(addr->port);

    if (s->local_port == 0U) {
        s->local_port = g_next_ephemeral_port++;
    }

    if (s->type == SOCK_DGRAM) {
        s->state = SocketState::Connected;
        return 0;
    }

    // TCP: initiate 3-way handshake (async -- caller blocks via scheduler)
    if (s->state == SocketState::SynSent) {
        // Woken from TcpConnect wait -- check result
        const auto state = net::tcp_state(s->tcp_conn_idx);
        if (state == net::TcpState::Established) {
            s->state = SocketState::Established;
            return 0;
        }
        // Still connecting or failed
        if (state == net::TcpState::Closed) {
            s->state = SocketState::Closed;
            return -111; // ECONNREFUSED
        }
        return -115; // EINPROGRESS (still waiting)
    }
    const int tcp_conn = net::tcp_connect(s->remote_addr, s->remote_port, s->local_port);
    if (tcp_conn < 0) {
        return -12; // ENOMEM
    }
    s->tcp_conn_idx = tcp_conn;
    s->state = SocketState::SynSent;
    return -115; // EINPROGRESS: caller should block and retry
}

int sys_listen(int sockfd, int /*backlog*/) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (s->type != SOCK_STREAM) {
        return -95; // EOPNOTSUPP
    }
    const int tcp_conn = net::tcp_listen(s->local_port);
    if (tcp_conn < 0) {
        return -12;
    }
    s->tcp_conn_idx = tcp_conn;
    s->state = SocketState::Listening;
    return 0;
}

int sys_accept(int sockfd, SockAddrIn* addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (s->state != SocketState::Listening) {
        return -22;
    }

    net::poll();
    const int tcp_child = net::tcp_accept(s->tcp_conn_idx);
    if (tcp_child < 0) {
        return -11; // EAGAIN
    }

    int new_fd = allocate_socket();
    if (new_fd < 0) {
        return -12;
    }

    Socket* child = get_socket(new_fd);
    if (child == nullptr) {
        return -12;
    }
    child->type = SOCK_STREAM;
    child->state = SocketState::Established;
    child->local_port = s->local_port;
    child->tcp_conn_idx = tcp_child;
    // D.3: Fill remote addr from TCP connection
    uint8_t remote_ip[4]{};
    uint16_t remote_port = 0U;
    net::tcp_get_remote(tcp_child, remote_ip, &remote_port);
    copy4(child->remote_addr, remote_ip);
    child->remote_port = remote_port;
    if (addr != nullptr) {
        *addr = {};
        addr->family = AF_INET;
        copy4(addr->addr, remote_ip);
        addr->port = net::htons(remote_port);
    }
    return new_fd;
}

int sys_sendto(int sockfd, const void* buf, uint32_t len,
               const SockAddrIn* dest_addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (buf == nullptr) {
        return -14;
    }

    uint8_t dst_ip[4];
    uint16_t dst_port;

    if (dest_addr != nullptr) {
        copy4(dst_ip, dest_addr->addr);
        dst_port = net::ntohs(dest_addr->port);
    } else if (s->state == SocketState::Connected) {
        copy4(dst_ip, s->remote_addr);
        dst_port = s->remote_port;
    } else {
        return -89; // EDESTADDRREQ
    }

    if (s->local_port == 0U) {
        s->local_port = g_next_ephemeral_port++;
    }

    if (s->type == SOCK_DGRAM) {
        if (net::send_udp(dst_ip, dst_port, s->local_port,
                          static_cast<const uint8_t*>(buf), len)) {
            return static_cast<int>(len);
        }
        return -5; // EIO
    }

    // TCP send
    if (s->type == SOCK_STREAM && s->state == SocketState::Established) {
        const int tcp_conn = s->tcp_conn_idx;
        return net::tcp_send(tcp_conn, buf, len);
    }
    return -38;
}

int sys_recvfrom(int sockfd, void* buf, uint32_t len,
                 SockAddrIn* /*src_addr*/) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (buf == nullptr) {
        return -14;
    }

    // Poll netstack for new frames
    net::poll();

    if (s->type == SOCK_STREAM && s->state == SocketState::Established) {
        const int tcp_conn = s->tcp_conn_idx;
        return net::tcp_recv(tcp_conn, buf, len);
    }

    if (rx_available(*s) == 0U) {
        return -11; // EAGAIN (would block)
    }

    uint32_t read = rx_read(*s, static_cast<uint8_t*>(buf), len);
    return static_cast<int>(read);
}

int sys_shutdown(int sockfd, int /*how*/) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (s->type == SOCK_STREAM && s->state == SocketState::Established) {
        net::tcp_close(s->tcp_conn_idx);
    }
    s->state = SocketState::Closed;
    return 0;
}

int sys_close(int sockfd) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (s->type == SOCK_STREAM &&
        (s->state == SocketState::Established || s->state == SocketState::CloseWait)) {
        net::tcp_close(s->tcp_conn_idx);
    }
    s->in_use = false;
    return 0;
}

int get_tcp_conn_idx(int sockfd) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -1;
    }
    return s->tcp_conn_idx;
}

int sys_setsockopt(int sockfd, int level, int optname,
                   const void* optval, uint32_t optlen) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9; // EBADF
    }

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_REUSEADDR:
            if (optval != nullptr && optlen >= 4U) {
                s->so_reuseaddr = (*static_cast<const int*>(optval) != 0);
            }
            return 0;
        case SO_KEEPALIVE:
            if (optval != nullptr && optlen >= 4U) {
                s->so_keepalive = (*static_cast<const int*>(optval) != 0);
            }
            return 0;
        default:
            return 0; // Fixed buffer sizes; accept unknown options
        }
    }

    if (level == IPPROTO_TCP) {
        if (optname == TCP_NODELAY) {
            if (optval != nullptr && optlen >= 4U && s->type == SOCK_STREAM) {
                const bool nodelay = (*static_cast<const int*>(optval) != 0);
                net::tcp_set_nodelay(s->tcp_conn_idx, nodelay);
            }
            return 0;
        }
        return 0; // Permissive
    }

    return 0; // Permissive for unknown levels
}

int sys_getsockopt(int sockfd, int level, int optname,
                   void* optval, uint32_t* optlen) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (optval == nullptr || optlen == nullptr || *optlen < 4U) {
        return -14;
    }

    auto* val = static_cast<int*>(optval);
    *optlen = 4U;

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_REUSEADDR: *val = s->so_reuseaddr ? 1 : 0; return 0;
        case SO_KEEPALIVE: *val = s->so_keepalive ? 1 : 0; return 0;
        case SO_SNDBUF:
        case SO_RCVBUF: *val = 4096; return 0;
        default: *val = 0; return 0;
        }
    }
    *val = 0;
    return 0;
}

int sys_getsockname(int sockfd, SockAddrIn* addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (addr == nullptr) {
        return -14;
    }
    *addr = {};
    addr->family = AF_INET;
    copy4(addr->addr, s->local_addr);
    addr->port = net::htons(s->local_port);
    return 0;
}

int sys_getpeername(int sockfd, SockAddrIn* addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) {
        return -9;
    }
    if (addr == nullptr) {
        return -14;
    }
    if (s->state != SocketState::Connected && s->state != SocketState::Established) {
        return -107; // ENOTCONN
    }
    *addr = {};
    addr->family = AF_INET;
    copy4(addr->addr, s->remote_addr);
    addr->port = net::htons(s->remote_port);
    return 0;
}

int sys_socketpair(int domain, int /*type*/, int /*protocol*/, int sv[2]) noexcept {
    if (domain != AF_UNIX) {
        return -97; // EAFNOSUPPORT
    }
    if (sv == nullptr) {
        return -14;
    }

    int fd0 = allocate_socket();
    if (fd0 < 0) {
        return -12;
    }
    int fd1 = allocate_socket();
    if (fd1 < 0) {
        get_socket(fd0)->in_use = false;
        return -12;
    }

    Socket* s0 = get_socket(fd0);
    Socket* s1 = get_socket(fd1);
    s0->type = SOCK_STREAM;
    s0->state = SocketState::Connected;
    s1->type = SOCK_STREAM;
    s1->state = SocketState::Connected;
    // Cross-link: each socket's "tcp_conn_idx" points to the peer's fd
    // for socketpair we use negative indices as a sentinel
    s0->tcp_conn_idx = -(fd1 + 1);
    s1->tcp_conn_idx = -(fd0 + 1);

    sv[0] = fd0;
    sv[1] = fd1;
    return 0;
}

void deliver_udp(const uint8_t* src_ip, uint16_t src_port,
                 uint16_t dst_port, const uint8_t* data, uint32_t length) noexcept {
    (void)src_ip;
    (void)src_port;
    // Find socket bound to dst_port
    for (auto& s : g_sockets) {
        if (s.in_use && s.type == SOCK_DGRAM && s.local_port == dst_port) {
            rx_write(s, data, length);
            return;
        }
    }
}

} // namespace xinim::i486::ksocket
