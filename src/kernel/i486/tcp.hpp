#pragma once
// TCP state machine for the i486 kernel.
// Cleanroom implementation per RFC 793, RFC 1122, RFC 6298.

#include "netstack.hpp"
#include "socket_i486.hpp"

namespace xinim::i486::net {

// -- Sequence arithmetic (RFC 793 Section 3.3) ----------------------------
// Modular 32-bit comparison via signed difference handles wrap correctly.
inline bool seq_lt(uint32_t a, uint32_t b) noexcept { return static_cast<int32_t>(a - b) < 0; }
inline bool seq_leq(uint32_t a, uint32_t b) noexcept { return static_cast<int32_t>(a - b) <= 0; }
inline bool seq_gt(uint32_t a, uint32_t b) noexcept { return static_cast<int32_t>(a - b) > 0; }
inline bool seq_geq(uint32_t a, uint32_t b) noexcept { return static_cast<int32_t>(a - b) >= 0; }

// -- Connection states (RFC 793 Figure 6) ---------------------------------
enum class TcpState : uint8_t {
    Closed = 0,
    Listen = 1,
    SynSent = 2,
    SynReceived = 3,
    Established = 4,
    CloseWait = 5,
    FinWait1 = 6,
    Closing = 7,
    LastAck = 8,
    FinWait2 = 9,
    TimeWait = 10,
};

// -- Limits ---------------------------------------------------------------
constexpr uint32_t kTcpMaxConnections = 8U;
constexpr uint32_t kTcpRxBufSize = 4096U;
constexpr uint32_t kTcpTxBufSize = 4096U;
constexpr uint16_t kTcpDefaultWindow = 4096U;
constexpr uint32_t kTcpMss = 1400U;

// -- Timers (RFC 6298, RFC 1122) ------------------------------------------
// All values in 100Hz ticks.
constexpr uint32_t kTcpRtoDefault = 300U;       // 3s (RFC 6298 Section 2)
constexpr uint32_t kTcpRtoMin = 100U;           // 1s floor
constexpr uint32_t kTcpRtoMax = 6400U;          // 64s ceiling
constexpr uint32_t kTcpMsl = 3000U;             // 30s MSL
constexpr uint32_t kTcpMaxRetransmit = 12U;
constexpr uint32_t kTcpPersistInterval = 500U;   // 5s (RFC 1122 4.2.2.17)
constexpr uint32_t kTcpDupAckThreshold = 3U;     // fast retransmit (RFC 5681)
constexpr uint32_t kTcpIssIncrement = 1280U;     // ~128k/sec ISN advance (RFC 1948)

// -- Per-connection state -------------------------------------------------
struct TcpConnection {
    bool in_use;
    TcpState state;

    // Endpoints
    uint8_t local_ip[4];
    uint8_t remote_ip[4];
    uint16_t local_port;
    uint16_t remote_port;

    // Sequence tracking (RFC 793 Section 3.2)
    uint32_t snd_una;   // oldest unacknowledged
    uint32_t snd_nxt;   // next to send
    uint32_t snd_wnd;   // peer's advertised window
    uint32_t rcv_nxt;   // next expected from peer
    uint32_t rcv_wnd;   // our advertised window
    uint32_t iss;        // initial send sequence

    // Receive ring buffer
    uint8_t rx_buf[kTcpRxBufSize];
    uint32_t rx_head;
    uint32_t rx_count;

    // Send ring buffer (retransmit + Nagle staging)
    uint8_t tx_buf[kTcpTxBufSize];
    uint32_t tx_head;
    uint32_t tx_tail;
    uint32_t tx_count;
    uint32_t tx_unsent_off;  // offset to first unsent byte

    // Retransmit (RFC 6298)
    uint64_t rexmt_deadline;
    uint32_t rto;
    uint8_t rexmt_shift;
    bool needs_ack;
    bool fin_sent;

    // Persist timer (RFC 1122 Section 4.2.2.17)
    uint64_t persist_deadline;

    // Nagle (RFC 1122 Section 4.2.3.4)
    bool nagle_disabled;

    // Fast retransmit (RFC 5681 Section 3.2)
    uint32_t dup_ack_count;
    uint32_t last_ack_num;

    // TIME_WAIT (RFC 793 Section 3.5: 2*MSL)
    uint64_t timewait_deadline;

    // Accept backlog
    int parent_socket;
};

// -- Public API -----------------------------------------------------------
void tcp_initialize() noexcept;
void tcp_input(const Ipv4Header* ip, const TcpHeader* tcp, uint32_t tcp_len) noexcept;
void tcp_timer_tick(uint64_t current_tick) noexcept;

int tcp_connect(const uint8_t* dst_ip, uint16_t dst_port, uint16_t src_port) noexcept;
int tcp_listen(uint16_t port) noexcept;
int tcp_accept(int listen_conn) noexcept;
int tcp_send(int conn, const void* data, uint32_t len) noexcept;
int tcp_recv(int conn, void* buf, uint32_t len) noexcept;
int tcp_close(int conn) noexcept;

bool tcp_has_data(int conn) noexcept;
TcpState tcp_state(int conn) noexcept;
bool tcp_get_remote(int conn, uint8_t* ip, uint16_t* port) noexcept;
bool tcp_get_local(int conn, uint8_t* ip, uint16_t* port) noexcept;
void tcp_set_nodelay(int conn, bool nodelay) noexcept;

} // namespace xinim::i486::net
