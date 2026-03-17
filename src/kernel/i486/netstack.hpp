#pragma once

#include <stdint.h>

namespace xinim::i486::net {

// Network byte order helpers
inline uint16_t htons(uint16_t h) noexcept { return static_cast<uint16_t>((h >> 8) | (h << 8)); }
inline uint16_t ntohs(uint16_t n) noexcept { return htons(n); }
inline uint32_t htonl(uint32_t h) noexcept {
    return ((h >> 24) & 0xFF) | ((h >> 8) & 0xFF00) |
           ((h << 8) & 0xFF0000) | ((h << 24) & 0xFF000000U);
}
inline uint32_t ntohl(uint32_t n) noexcept { return htonl(n); }

// Protocol numbers
constexpr uint16_t ETHERTYPE_ARP  = 0x0806U;
constexpr uint16_t ETHERTYPE_IPV4 = 0x0800U;
constexpr uint8_t  IP_PROTO_ICMP  = 1U;
constexpr uint8_t  IP_PROTO_TCP   = 6U;
constexpr uint8_t  IP_PROTO_UDP   = 17U;

// Ethernet header
struct [[gnu::packed]] EthernetHeader {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;
};

// ARP packet
struct [[gnu::packed]] ArpPacket {
    uint16_t hw_type;     // 1 = Ethernet
    uint16_t proto_type;  // 0x0800 = IPv4
    uint8_t  hw_len;      // 6
    uint8_t  proto_len;   // 4
    uint16_t opcode;      // 1 = request, 2 = reply
    uint8_t  sender_mac[6];
    uint8_t  sender_ip[4];
    uint8_t  target_mac[6];
    uint8_t  target_ip[4];
};

// IPv4 header
struct [[gnu::packed]] Ipv4Header {
    uint8_t  version_ihl;  // (4 << 4) | 5
    uint8_t  tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
};

// UDP header
struct [[gnu::packed]] UdpHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
};

// ICMP header
struct [[gnu::packed]] IcmpHeader {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
};

// TCP header
struct [[gnu::packed]] TcpHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset; // (offset << 4) | reserved
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
};

constexpr uint8_t TCP_FIN = 0x01U;
constexpr uint8_t TCP_SYN = 0x02U;
constexpr uint8_t TCP_RST = 0x04U;
constexpr uint8_t TCP_PSH = 0x08U;
constexpr uint8_t TCP_ACK = 0x10U;

// Network configuration (set by DHCP or static)
struct NetConfig {
    uint8_t  ip[4];        // Our IP address
    uint8_t  gateway[4];   // Default gateway
    uint8_t  netmask[4];   // Subnet mask
    uint8_t  dns[4];       // DNS server
    uint8_t  mac[6];       // Our MAC (from virtio-net)
    bool     configured;   // True after DHCP or static config
};

// Initialize the network stack (call after virtio-net is up)
void initialize() noexcept;

// Process one received Ethernet frame
void process_rx_frame(const uint8_t* frame, uint32_t length) noexcept;

// Poll for received frames and process them
void poll() noexcept;

// Send a raw Ethernet frame
bool send_frame(const uint8_t* frame, uint32_t length) noexcept;

// High-level operations
bool send_udp(const uint8_t* dst_ip, uint16_t dst_port,
              uint16_t src_port, const uint8_t* data, uint32_t length) noexcept;

// DHCP
bool dhcp_discover() noexcept;
bool is_configured() noexcept;
const NetConfig& config() noexcept;

// DNS
bool dns_resolve(const char* hostname, uint8_t* out_ip) noexcept;

} // namespace xinim::i486::net
