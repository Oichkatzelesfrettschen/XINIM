#include "tcp.hpp"
#include "console.hpp"

namespace xinim::i486::net {
namespace {

TcpConnection g_connections[kTcpMaxConnections]{};
// ISN: timer-driven per RFC 793/1948.
// Incremented by kTcpIssIncrement every timer tick, plus a per-connection delta.
uint32_t g_tcp_iss = 0x10000U;
uint64_t g_tcp_timer_ticks = 0U;

void copy4(uint8_t* dst, const uint8_t* src) noexcept {
    dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2]; dst[3]=src[3];
}

bool eq4(const uint8_t* a, const uint8_t* b) noexcept {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

TcpConnection* get_conn(int idx) noexcept {
    if (idx < 0 || static_cast<uint32_t>(idx) >= kTcpMaxConnections) {
        return nullptr;
    }
    if (!g_connections[idx].in_use) {
        return nullptr;
    }
    return &g_connections[idx];
}

int alloc_conn() noexcept {
    for (uint32_t i = 0; i < kTcpMaxConnections; ++i) {
        if (!g_connections[i].in_use) {
            g_connections[i] = {};
            g_connections[i].in_use = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

// TCP pseudo-header checksum for IPv4
uint16_t tcp_checksum(const uint8_t* src_ip, const uint8_t* dst_ip,
                      const uint8_t* tcp_segment, uint32_t tcp_len) noexcept {
    uint32_t sum = 0U;
    // Pseudo-header: src_ip(4) + dst_ip(4) + zero(1) + proto(1) + tcp_len(2)
    sum += (static_cast<uint32_t>(src_ip[0]) << 8U) | src_ip[1];
    sum += (static_cast<uint32_t>(src_ip[2]) << 8U) | src_ip[3];
    sum += (static_cast<uint32_t>(dst_ip[0]) << 8U) | dst_ip[1];
    sum += (static_cast<uint32_t>(dst_ip[2]) << 8U) | dst_ip[3];
    sum += IP_PROTO_TCP;
    sum += tcp_len;
    // TCP segment
    for (uint32_t i = 0U; i + 1U < tcp_len; i += 2U) {
        sum += (static_cast<uint32_t>(tcp_segment[i]) << 8U) | tcp_segment[i + 1U];
    }
    if (tcp_len & 1U) {
        sum += static_cast<uint32_t>(tcp_segment[tcp_len - 1U]) << 8U;
    }
    while (sum >> 16U) {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }
    return static_cast<uint16_t>(~sum);
}

bool send_tcp_segment(TcpConnection* conn, uint8_t flags,
                      const uint8_t* data, uint32_t data_len,
                      uint32_t seq) noexcept {
    const uint32_t tcp_hdr_len = 20U; // No options
    const uint32_t total = tcp_hdr_len + data_len;
    if (total > 1460U) {
        return false; // MSS limit
    }

    uint8_t segment[1480U]{};
    auto* tcp = reinterpret_cast<TcpHeader*>(segment);
    tcp->src_port = htons(conn->local_port);
    tcp->dst_port = htons(conn->remote_port);
    tcp->seq_num = htonl(seq);
    tcp->ack_num = htonl(conn->rcv_nxt);
    tcp->data_offset = static_cast<uint8_t>((tcp_hdr_len / 4U) << 4U);
    tcp->flags = flags;
    tcp->window = htons(static_cast<uint16_t>(conn->rcv_wnd));
    tcp->checksum = 0U;
    tcp->urgent_ptr = 0U;

    if (data_len > 0U && data != nullptr) {
        for (uint32_t i = 0U; i < data_len; ++i) {
            segment[tcp_hdr_len + i] = data[i];
        }
    }

    const uint8_t* local_ip = conn->local_ip;
    if (local_ip[0] == 0U && local_ip[1] == 0U && local_ip[2] == 0U && local_ip[3] == 0U) {
        local_ip = config().ip;
    }
    tcp->checksum = htons(tcp_checksum(local_ip, conn->remote_ip, segment, total));

    conn->needs_ack = false;
    return send_ip_packet(conn->remote_ip, IP_PROTO_TCP, segment, total);
}

// Clear or reset retransmit timer after ACK advances snd_una.
// Per RFC 6298 Section 5.5: clear if all data acknowledged, reset to base if more expected.
void ack_update_timer(TcpConnection* conn) noexcept {
    if (conn->snd_una == conn->snd_nxt) {
        // All data acknowledged -- clear retransmit timer
        conn->rexmt_deadline = 0U;
        conn->rexmt_shift = 0U;
        conn->rto = kTcpRtoDefault;
    } else {
        // More ACKs expected -- reset to base RTO
        conn->rexmt_shift = 0U;
        conn->rto = kTcpRtoDefault;
        conn->rexmt_deadline = g_tcp_timer_ticks + conn->rto;
    }
}

// Find a connection matching the incoming segment
TcpConnection* find_connection(const uint8_t* remote_ip,
                                uint16_t remote_port,
                                uint16_t local_port) noexcept {
    // Exact match first
    for (auto& c : g_connections) {
        if (c.in_use && c.state != TcpState::Listen &&
            c.local_port == local_port &&
            c.remote_port == remote_port &&
            eq4(c.remote_ip, remote_ip)) {
            return &c;
        }
    }
    // Listening socket match
    for (auto& c : g_connections) {
        if (c.in_use && c.state == TcpState::Listen &&
            c.local_port == local_port) {
            return &c;
        }
    }
    return nullptr;
}

uint32_t rx_push(TcpConnection* conn, const uint8_t* data, uint32_t len) noexcept {
    uint32_t stored = 0U;
    for (uint32_t i = 0U; i < len && conn->rx_count < kTcpRxBufSize; ++i) {
        conn->rx_buf[(conn->rx_head + conn->rx_count) % kTcpRxBufSize] = data[i];
        ++conn->rx_count;
        ++stored;
    }
    // B.1: Update advertised window to reflect actual free space
    conn->rcv_wnd = kTcpRxBufSize - conn->rx_count;
    return stored;
}

// Copy unacked data from tx_buf for retransmission (B.5)
uint32_t tx_read_unacked(TcpConnection* conn, uint8_t* out, uint32_t max_len) noexcept {
    // Data between snd_una and snd_nxt lives in tx_buf
    const uint32_t unacked = conn->snd_nxt - conn->snd_una;
    if (unacked == 0U) {
        return 0U;
    }
    uint32_t to_copy = unacked;
    if (to_copy > max_len) {
        to_copy = max_len;
    }
    if (to_copy > conn->tx_count) {
        to_copy = conn->tx_count;
    }
    // tx_unsent_off marks where unsent data begins; acked data was consumed
    // The retransmit data starts at tx_head (oldest buffered)
    for (uint32_t i = 0U; i < to_copy; ++i) {
        out[i] = conn->tx_buf[(conn->tx_head + i) % kTcpTxBufSize];
    }
    return to_copy;
}

// Buffer data in tx_buf for Nagle/retransmit (B.3/B.5)
uint32_t tx_buffer(TcpConnection* conn, const uint8_t* data, uint32_t len) noexcept {
    uint32_t buffered = 0U;
    while (buffered < len && conn->tx_count < kTcpTxBufSize) {
        conn->tx_buf[conn->tx_tail % kTcpTxBufSize] = data[buffered];
        ++conn->tx_tail;
        ++conn->tx_count;
        ++buffered;
    }
    return buffered;
}

// Consume acknowledged data from tx_buf
void tx_consume_acked(TcpConnection* conn, uint32_t bytes) noexcept {
    if (bytes > conn->tx_count) {
        bytes = conn->tx_count;
    }
    conn->tx_head += bytes;
    conn->tx_count -= bytes;
    if (conn->tx_unsent_off > bytes) {
        conn->tx_unsent_off -= bytes;
    } else {
        conn->tx_unsent_off = 0U;
    }
}

void handle_syn(TcpConnection* conn, const Ipv4Header* ip,
                const TcpHeader* tcp) noexcept {
    if (conn->state == TcpState::Listen) {
        // Passive open: create new connection for this SYN
        int idx = alloc_conn();
        if (idx < 0) {
            return; // No free connections
        }
        TcpConnection* child = &g_connections[idx];
        copy4(child->remote_ip, ip->src_ip);
        copy4(child->local_ip, ip->dst_ip);
        child->remote_port = ntohs(tcp->src_port);
        child->local_port = conn->local_port;
        child->iss = g_tcp_iss += kTcpIssIncrement;
        child->snd_una = child->iss;
        child->snd_nxt = child->iss + 1U;
        child->rcv_nxt = ntohl(tcp->seq_num) + 1U;
        child->rcv_wnd = kTcpDefaultWindow;
        child->snd_wnd = ntohs(tcp->window);
        child->state = TcpState::SynReceived;
        child->parent_socket = -1;
        child->rto = kTcpRtoDefault;
        child->rexmt_shift = 0U;
        // Send SYN+ACK and arm retransmit timer
        send_tcp_segment(child, TCP_SYN | TCP_ACK, nullptr, 0U, child->iss);
        child->rexmt_deadline = g_tcp_timer_ticks + child->rto;
        return;
    }

    if (conn->state == TcpState::SynSent) {
        // Simultaneous open: received SYN while waiting for SYN+ACK
        conn->rcv_nxt = ntohl(tcp->seq_num) + 1U;
        conn->snd_wnd = ntohs(tcp->window);
        if ((tcp->flags & TCP_ACK) != 0U &&
            ntohl(tcp->ack_num) == conn->iss + 1U) {
            conn->snd_una = ntohl(tcp->ack_num);
            conn->state = TcpState::Established;
            send_tcp_segment(conn, TCP_ACK, nullptr, 0U, conn->snd_nxt);
        } else if ((tcp->flags & TCP_ACK) == 0U) {
            conn->state = TcpState::SynReceived;
            send_tcp_segment(conn, TCP_SYN | TCP_ACK, nullptr, 0U, conn->iss);
        }
    }
}

} // namespace

void tcp_initialize() noexcept {
    for (auto& c : g_connections) {
        c = {};
    }
}

void tcp_input(const Ipv4Header* ip, const TcpHeader* tcp, uint32_t tcp_len) noexcept {
    if (tcp_len < 20U) {
        return;
    }

    const uint16_t src_port = ntohs(tcp->src_port);
    const uint16_t dst_port = ntohs(tcp->dst_port);
    const uint32_t seq = ntohl(tcp->seq_num);
    const uint32_t ack = ntohl(tcp->ack_num);
    const uint8_t flags = tcp->flags;
    const uint32_t hdr_len = static_cast<uint32_t>((tcp->data_offset >> 4U) * 4U);
    const uint32_t data_len = (tcp_len > hdr_len) ? (tcp_len - hdr_len) : 0U;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(tcp) + hdr_len;

    TcpConnection* conn = find_connection(ip->src_ip, src_port, dst_port);
    if (conn == nullptr) {
        // No matching connection -- send RST
        return;
    }

    // RST handling: RFC 793 s3.4 -- only accept if seq is in receive window
    if ((flags & TCP_RST) != 0U) {
        if (conn->state == TcpState::Listen) {
            return; // Ignore RST in LISTEN
        }
        if (seq >= conn->rcv_nxt && seq < conn->rcv_nxt + conn->rcv_wnd) {
            conn->state = TcpState::Closed;
            conn->in_use = false;
        }
        return;
    }

    switch (conn->state) {
    case TcpState::Listen:
    case TcpState::SynSent:
        if ((flags & TCP_SYN) != 0U) {
            handle_syn(conn, ip, tcp);
        }
        break;

    case TcpState::SynReceived:
        if ((flags & TCP_ACK) != 0U && ack == conn->snd_nxt) {
            conn->snd_una = ack;
            conn->state = TcpState::Established;
        }
        break;

    case TcpState::Established:
        // Process ACK
        if ((flags & TCP_ACK) != 0U) {
            const uint32_t old_snd_una = conn->snd_una;
            if (seq_gt(ack, old_snd_una) && seq_leq(ack, conn->snd_nxt)) {
                // New ACK: advances snd_una
                const uint32_t acked_bytes = ack - old_snd_una;
                conn->snd_una = ack;
                tx_consume_acked(conn, acked_bytes);
                conn->dup_ack_count = 0U;
                conn->last_ack_num = ack;
            } else if (ack == old_snd_una && data_len == 0U &&
                       ntohs(tcp->window) == static_cast<uint16_t>(conn->snd_wnd)) {
                // B.4: Duplicate ACK (same ack, no data, same window)
                ++conn->dup_ack_count;
                if (conn->dup_ack_count == kTcpDupAckThreshold) {
                    // Fast retransmit per RFC 5681 Section 3.2
                    uint8_t rexmt_data[kTcpMss]{};
                    const uint32_t rexmt_len = tx_read_unacked(conn, rexmt_data, kTcpMss);
                    if (rexmt_len > 0U) {
                        send_tcp_segment(conn, TCP_ACK, rexmt_data, rexmt_len, conn->snd_una);
                    }
                    conn->dup_ack_count = 0U;
                }
            }
            conn->snd_wnd = ntohs(tcp->window);
            ack_update_timer(conn);
            // B.2: Clear persist timer when window opens
            if (conn->snd_wnd > 0U) {
                conn->persist_deadline = 0U;
            }
        }
        // Process data
        if (data_len > 0U && seq == conn->rcv_nxt) {
            const uint32_t accepted = rx_push(conn, data, data_len);
            conn->rcv_nxt += accepted;
            conn->needs_ack = true;
        }
        // Process FIN
        if ((flags & TCP_FIN) != 0U) {
            conn->rcv_nxt = conn->rcv_nxt + 1U; // FIN consumes one sequence number
            conn->state = TcpState::CloseWait;
            send_tcp_segment(conn, TCP_ACK, nullptr, 0U, conn->snd_nxt);
        } else if (conn->needs_ack) {
            // Send ACK for received data
            send_tcp_segment(conn, TCP_ACK, nullptr, 0U, conn->snd_nxt);
        }
        break;

    case TcpState::FinWait1:
        if ((flags & TCP_ACK) != 0U) {
            conn->snd_una = ack;
        }
        // Process data before FIN
        if (data_len > 0U && seq == conn->rcv_nxt) {
            const uint32_t accepted = rx_push(conn, data, data_len);
            conn->rcv_nxt += accepted;
            send_tcp_segment(conn, TCP_ACK, nullptr, 0U, conn->snd_nxt);
        }
        if ((flags & TCP_ACK) != 0U && (flags & TCP_FIN) != 0U) {
            conn->rcv_nxt += 1U; // FIN consumes one seq
            conn->state = TcpState::TimeWait;
            conn->timewait_deadline = g_tcp_timer_ticks + 2U * static_cast<uint64_t>(kTcpMsl);
            conn->rexmt_deadline = 0U;
            send_tcp_segment(conn, TCP_ACK, nullptr, 0U, conn->snd_nxt);
        } else if ((flags & TCP_ACK) != 0U) {
            conn->state = TcpState::FinWait2;
        }
        break;

    case TcpState::FinWait2:
        // Process data BEFORE FIN to avoid rcv_nxt advancing past data
        if (data_len > 0U && seq == conn->rcv_nxt) {
            const uint32_t accepted = rx_push(conn, data, data_len);
            conn->rcv_nxt += accepted;
            send_tcp_segment(conn, TCP_ACK, nullptr, 0U, conn->snd_nxt);
        }
        if ((flags & TCP_FIN) != 0U) {
            conn->rcv_nxt += 1U; // FIN consumes one seq
            conn->state = TcpState::TimeWait;
            conn->timewait_deadline = g_tcp_timer_ticks + 2U * static_cast<uint64_t>(kTcpMsl);
            conn->rexmt_deadline = 0U;
            send_tcp_segment(conn, TCP_ACK, nullptr, 0U, conn->snd_nxt);
        }
        break;

    case TcpState::CloseWait:
        // Waiting for application to close
        if ((flags & TCP_ACK) != 0U) {
            conn->snd_una = ack;
        }
        break;

    case TcpState::LastAck:
        if ((flags & TCP_ACK) != 0U) {
            conn->state = TcpState::Closed;
            conn->in_use = false;
        }
        break;

    case TcpState::Closing:
        if ((flags & TCP_ACK) != 0U) {
            conn->state = TcpState::TimeWait;
            conn->timewait_deadline = g_tcp_timer_ticks + 2U * static_cast<uint64_t>(kTcpMsl);
            conn->rexmt_deadline = 0U;
        }
        break;

    case TcpState::TimeWait:
        // Ignore segments in TIME_WAIT
    case TcpState::Closed:
        break;
    }
}

int tcp_connect(const uint8_t* dst_ip, uint16_t dst_port, uint16_t src_port) noexcept {
    int idx = alloc_conn();
    if (idx < 0) {
        return -1;
    }

    TcpConnection* conn = &g_connections[idx];
    copy4(conn->remote_ip, dst_ip);
    copy4(conn->local_ip, config().ip);
    conn->remote_port = dst_port;
    conn->local_port = src_port;
    conn->iss = g_tcp_iss += kTcpIssIncrement;
    conn->snd_una = conn->iss;
    conn->snd_nxt = conn->iss + 1U;
    conn->rcv_wnd = kTcpDefaultWindow;
    conn->rto = kTcpRtoDefault;
    conn->rexmt_shift = 0U;
    conn->state = TcpState::SynSent;

    // Send SYN and arm retransmit timer
    send_tcp_segment(conn, TCP_SYN, nullptr, 0U, conn->iss);
    conn->rexmt_deadline = g_tcp_timer_ticks + conn->rto;
    return idx;
}

int tcp_listen(uint16_t port) noexcept {
    int idx = alloc_conn();
    if (idx < 0) {
        return -1;
    }

    TcpConnection* conn = &g_connections[idx];
    copy4(conn->local_ip, config().ip);
    conn->local_port = port;
    conn->state = TcpState::Listen;
    conn->rcv_wnd = kTcpDefaultWindow;
    return idx;
}

int tcp_accept(int listen_idx) noexcept {
    // Find a SynReceived -> Established connection with matching local port
    TcpConnection* listener = get_conn(listen_idx);
    if (listener == nullptr || listener->state != TcpState::Listen) {
        return -1;
    }

    for (uint32_t i = 0; i < kTcpMaxConnections; ++i) {
        if (g_connections[i].in_use &&
            g_connections[i].state == TcpState::Established &&
            g_connections[i].local_port == listener->local_port &&
            static_cast<int>(i) != listen_idx) {
            return static_cast<int>(i);
        }
    }
    return -1; // No pending connection
}

int tcp_send(int idx, const void* data, uint32_t len) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn == nullptr) {
        return -1;
    }
    if (conn->state != TcpState::Established &&
        conn->state != TcpState::CloseWait) {
        return -1;
    }

    const auto* src = static_cast<const uint8_t*>(data);

    // Buffer data in tx_buf for retransmit tracking (B.5)
    const uint32_t buffered = tx_buffer(conn, src, len);
    if (buffered == 0U) {
        return 0;
    }

    // Determine how much unsent data we have
    const uint32_t unsent = conn->tx_count - conn->tx_unsent_off;
    const bool has_unacked = seq_lt(conn->snd_una, conn->snd_nxt);

    // B.3: Nagle algorithm -- if unacked data and new data < MSS, hold
    if (!conn->nagle_disabled && has_unacked && unsent < kTcpMss) {
        // Data is buffered; it will be flushed when ACK arrives or MSS is reached
        return static_cast<int>(buffered);
    }

    // Send buffered data in MSS-sized chunks
    while (conn->tx_unsent_off < conn->tx_count) {
        uint32_t chunk = conn->tx_count - conn->tx_unsent_off;
        if (chunk > kTcpMss) {
            chunk = kTcpMss;
        }
        if (chunk > conn->snd_wnd) {
            chunk = static_cast<uint32_t>(conn->snd_wnd);
        }
        if (chunk == 0U) {
            // B.2: Zero window -- arm persist timer
            if (conn->persist_deadline == 0U) {
                conn->persist_deadline = g_tcp_timer_ticks + kTcpPersistInterval;
            }
            break;
        }

        // Read chunk from tx_buf at unsent offset
        uint8_t seg_data[kTcpMss]{};
        for (uint32_t i = 0U; i < chunk; ++i) {
            seg_data[i] = conn->tx_buf[(conn->tx_head + conn->tx_unsent_off + i) % kTcpTxBufSize];
        }

        if (!send_tcp_segment(conn, TCP_ACK | TCP_PSH, seg_data, chunk, conn->snd_nxt)) {
            break;
        }
        conn->snd_nxt += chunk;
        conn->tx_unsent_off += chunk;

        // Arm retransmit timer if not already running
        if (conn->rexmt_deadline == 0U) {
            conn->rexmt_deadline = g_tcp_timer_ticks + conn->rto;
        }
    }

    return static_cast<int>(buffered);
}

int tcp_recv(int idx, void* buf, uint32_t len) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn == nullptr) {
        return -1;
    }

    if (conn->rx_count == 0U) {
        // Check for connection closed / EOF
        if (conn->state == TcpState::CloseWait ||
            conn->state == TcpState::Closed ||
            conn->state == TcpState::TimeWait) {
            return 0; // EOF
        }
        return -11; // EAGAIN
    }

    auto* dst = static_cast<uint8_t*>(buf);
    uint32_t read = 0U;
    while (read < len && conn->rx_count > 0U) {
        dst[read] = conn->rx_buf[conn->rx_head % kTcpRxBufSize];
        ++conn->rx_head;
        --conn->rx_count;
        ++read;
    }
    // B.1: Update advertised window after consuming data
    conn->rcv_wnd = kTcpRxBufSize - conn->rx_count;
    return static_cast<int>(read);
}

int tcp_close(int idx) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn == nullptr) {
        return -1;
    }

    switch (conn->state) {
    case TcpState::Established:
        send_tcp_segment(conn, TCP_FIN | TCP_ACK, nullptr, 0U, conn->snd_nxt);
        ++conn->snd_nxt;
        conn->state = TcpState::FinWait1;
        break;
    case TcpState::CloseWait:
        send_tcp_segment(conn, TCP_FIN | TCP_ACK, nullptr, 0U, conn->snd_nxt);
        ++conn->snd_nxt;
        conn->state = TcpState::LastAck;
        break;
    case TcpState::Listen:
    case TcpState::SynSent:
        conn->state = TcpState::Closed;
        conn->in_use = false;
        break;
    default:
        break;
    }
    return 0;
}

bool tcp_has_data(int idx) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn == nullptr) {
        return false;
    }
    return conn->rx_count > 0U;
}

TcpState tcp_state(int idx) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn == nullptr) {
        return TcpState::Closed;
    }
    return conn->state;
}

void tcp_timer_tick(uint64_t current_tick) noexcept {
    // Advance ISN clock (RFC 1948: monotonic ISN increment)
    g_tcp_iss += kTcpIssIncrement;
    g_tcp_timer_ticks = current_tick;

    for (auto& c : g_connections) {
        if (!c.in_use) {
            continue;
        }

        // TIME_WAIT expiry (2*MSL per RFC 793 Section 3.5)
        if (c.state == TcpState::TimeWait) {
            if (c.timewait_deadline != 0U && current_tick >= c.timewait_deadline) {
                c.state = TcpState::Closed;
                c.in_use = false;
            }
            continue;
        }

        // B.2: Persist timer -- zero-window probe (RFC 1122 Section 4.2.2.17)
        if (c.persist_deadline != 0U && current_tick >= c.persist_deadline) {
            if (c.snd_wnd == 0U && c.tx_count > 0U &&
                (c.state == TcpState::Established || c.state == TcpState::CloseWait)) {
                // Send 1-byte window probe from unsent data
                uint8_t probe = c.tx_buf[(c.tx_head + c.tx_unsent_off) % kTcpTxBufSize];
                send_tcp_segment(&c, TCP_ACK, &probe, 1U, c.snd_nxt);
            }
            c.persist_deadline = current_tick + kTcpPersistInterval;
        }

        // Retransmit timer
        if (c.rexmt_deadline != 0U && current_tick >= c.rexmt_deadline) {
            if (c.rexmt_shift >= kTcpMaxRetransmit) {
                // Too many retransmits -- drop connection
                c.state = TcpState::Closed;
                c.in_use = false;
                continue;
            }
            // Retransmit: re-send from snd_una
            if (c.state == TcpState::SynSent) {
                send_tcp_segment(&c, TCP_SYN, nullptr, 0U, c.iss);
            } else if (c.state == TcpState::SynReceived) {
                send_tcp_segment(&c, TCP_SYN | TCP_ACK, nullptr, 0U, c.iss);
            } else if (seq_lt(c.snd_una, c.snd_nxt)) {
                // B.5: Retransmit actual data from snd_una
                uint8_t rexmt_data[kTcpMss]{};
                const uint32_t rexmt_len = tx_read_unacked(&c, rexmt_data, kTcpMss);
                if (rexmt_len > 0U) {
                    send_tcp_segment(&c, TCP_ACK, rexmt_data, rexmt_len, c.snd_una);
                } else {
                    send_tcp_segment(&c, TCP_ACK, nullptr, 0U, c.snd_una);
                }
            }
            // Exponential backoff
            ++c.rexmt_shift;
            c.rto = c.rto * 2U;
            if (c.rto > kTcpRtoMax) {
                c.rto = kTcpRtoMax;
            }
            c.rexmt_deadline = current_tick + c.rto;
        }

        // B.3: Flush Nagle-held data when all prior data is ACKed
        if (c.snd_una == c.snd_nxt && c.tx_unsent_off < c.tx_count &&
            (c.state == TcpState::Established || c.state == TcpState::CloseWait)) {
            uint32_t unsent = c.tx_count - c.tx_unsent_off;
            while (unsent > 0U) {
                uint32_t chunk = unsent;
                if (chunk > kTcpMss) {
                    chunk = kTcpMss;
                }
                if (chunk > c.snd_wnd) {
                    chunk = static_cast<uint32_t>(c.snd_wnd);
                }
                if (chunk == 0U) {
                    break;
                }
                uint8_t seg_data[kTcpMss]{};
                for (uint32_t i = 0U; i < chunk; ++i) {
                    seg_data[i] = c.tx_buf[(c.tx_head + c.tx_unsent_off + i) % kTcpTxBufSize];
                }
                if (!send_tcp_segment(&c, TCP_ACK | TCP_PSH, seg_data, chunk, c.snd_nxt)) {
                    break;
                }
                c.snd_nxt += chunk;
                c.tx_unsent_off += chunk;
                unsent -= chunk;
                if (c.rexmt_deadline == 0U) {
                    c.rexmt_deadline = current_tick + c.rto;
                }
            }
        }
    }
}

bool tcp_get_remote(int idx, uint8_t* ip, uint16_t* port) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn == nullptr) {
        return false;
    }
    if (ip != nullptr) {
        copy4(ip, conn->remote_ip);
    }
    if (port != nullptr) {
        *port = conn->remote_port;
    }
    return true;
}

bool tcp_get_local(int idx, uint8_t* ip, uint16_t* port) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn == nullptr) {
        return false;
    }
    if (ip != nullptr) {
        copy4(ip, conn->local_ip);
    }
    if (port != nullptr) {
        *port = conn->local_port;
    }
    return true;
}

void tcp_set_nodelay(int idx, bool nodelay) noexcept {
    TcpConnection* conn = get_conn(idx);
    if (conn != nullptr) {
        conn->nagle_disabled = nodelay;
    }
}

// Called from netstack IP handler
void deliver_tcp_segment(const Ipv4Header* ip,
                         const uint8_t* tcp_data, uint32_t tcp_len) noexcept {
    if (tcp_len < sizeof(TcpHeader)) {
        return;
    }
    const auto* tcp = reinterpret_cast<const TcpHeader*>(tcp_data);
    tcp_input(ip, tcp, tcp_len);
}

} // namespace xinim::i486::net
