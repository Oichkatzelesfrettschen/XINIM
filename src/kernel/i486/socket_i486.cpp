/**
 * @file socket_i486.cpp
 * @brief Kernel socket layer bridging syscalls to the netstack.
 */

#include "socket_i486.hpp"
#include "netstack.hpp"

namespace xinim::i486::ksocket {
namespace {

Socket g_sockets[MAX_SOCKETS]{};
uint16_t g_next_ephemeral_port = 49152U;

Socket* get_socket(int fd) noexcept {
    // Socket fds are offset by 1000 to avoid collision with file fds
    int idx = fd - 1000;
    if (idx < 0 || static_cast<uint32_t>(idx) >= MAX_SOCKETS) return nullptr;
    if (!g_sockets[idx].in_use) return nullptr;
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
    if (domain != AF_INET) return -38; // ENOSYS
    if (type != SOCK_DGRAM && type != SOCK_STREAM) return -22; // EINVAL

    int fd = allocate_socket();
    if (fd < 0) return -12; // ENOMEM

    Socket* s = get_socket(fd);
    if (s == nullptr) return -12;
    s->type = type;
    s->state = SocketState::Closed;
    return fd;
}

int sys_bind(int sockfd, const SockAddrIn* addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) return -9; // EBADF
    if (addr == nullptr) return -14; // EFAULT

    s->local_port = net::ntohs(addr->port);
    copy4(s->local_addr, addr->addr);
    s->state = SocketState::Bound;
    return 0;
}

int sys_connect(int sockfd, const SockAddrIn* addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) return -9;
    if (addr == nullptr) return -14;

    copy4(s->remote_addr, addr->addr);
    s->remote_port = net::ntohs(addr->port);

    if (s->local_port == 0U) {
        s->local_port = g_next_ephemeral_port++;
    }

    if (s->type == SOCK_DGRAM) {
        s->state = SocketState::Connected;
        return 0;
    }

    // TCP: send SYN (simplified -- would need full state machine)
    s->state = SocketState::Connected;
    return 0;
}

int sys_listen(int sockfd, int /*backlog*/) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) return -9;
    s->state = SocketState::Listening;
    return 0;
}

int sys_accept(int /*sockfd*/, SockAddrIn* /*addr*/) noexcept {
    // Simplified: not yet implemented
    return -38; // ENOSYS
}

int sys_sendto(int sockfd, const void* buf, uint32_t len,
               const SockAddrIn* dest_addr) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) return -9;
    if (buf == nullptr) return -14;

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

    // TCP send would go here
    return -38;
}

int sys_recvfrom(int sockfd, void* buf, uint32_t len,
                 SockAddrIn* /*src_addr*/) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) return -9;
    if (buf == nullptr) return -14;

    // Poll netstack for new frames
    net::poll();

    if (rx_available(*s) == 0U) {
        return -11; // EAGAIN (would block)
    }

    uint32_t read = rx_read(*s, static_cast<uint8_t*>(buf), len);
    return static_cast<int>(read);
}

int sys_shutdown(int sockfd, int /*how*/) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) return -9;
    s->state = SocketState::Closed;
    return 0;
}

int sys_close(int sockfd) noexcept {
    Socket* s = get_socket(sockfd);
    if (s == nullptr) return -9;
    s->in_use = false;
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
