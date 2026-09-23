/**
 * @file netstack.cpp
 * @brief Minimal IPv4/ARP/UDP/DHCP/DNS/ICMP stack for i486.
 *
 * Self-contained network stack sitting on top of the virtio-net driver.
 * No external dependencies (no lwIP, no hosted libc).
 * Designed for QEMU SLIRP networking (10.0.2.x subnet).
 */

#include "netstack.hpp"
#include "virtio_net_i486.hpp"
#include "socket_i486.hpp"
#include "console.hpp"

namespace xinim::i486::net {
namespace {

// --- Globals ---
NetConfig g_config{};
uint16_t g_ip_id = 1U;

// ARP cache (simple: 8 entries)
struct ArpEntry { uint8_t ip[4]; uint8_t mac[6]; bool valid; };
ArpEntry g_arp_cache[8]{};

// --- Helpers ---

void copy4(uint8_t* dst, const uint8_t* src) noexcept {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
}

void copy6(uint8_t* dst, const uint8_t* src) noexcept {
    for (int i = 0; i < 6; ++i) {
        dst[i] = src[i];
    }
}

bool eq4(const uint8_t* a, const uint8_t* b) noexcept {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

void set4(uint8_t* dst, uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept {
    dst[0]=a; dst[1]=b; dst[2]=c; dst[3]=d;
}

} // namespace (close anonymous for ip_checksum, reopened below)

uint16_t ip_checksum(const void* data, uint32_t length) noexcept {
    const auto* p = static_cast<const uint8_t*>(data);
    uint32_t sum = 0U;
    for (uint32_t i = 0U; i + 1U < length; i += 2U) {
        sum += (static_cast<uint32_t>(p[i]) << 8U) | p[i + 1U];
    }
    if (length & 1U) {
        sum += static_cast<uint32_t>(p[length - 1U]) << 8U;
    }
    while (sum >> 16U) {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }
    return static_cast<uint16_t>(~sum);
}

namespace {

// --- DHCP packet structure (needed before handle_ipv4) ---

struct [[gnu::packed]] DhcpPacket {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic_cookie;
};

constexpr uint32_t DHCP_MAGIC = 0x63825363U;
uint32_t g_dhcp_xid = 0x12345678U;

// --- ARP ---

void arp_cache_add(const uint8_t* ip, const uint8_t* mac) noexcept {
    for (auto& e : g_arp_cache) {
        if (e.valid && eq4(e.ip, ip)) { copy6(e.mac, mac); return; }
    }
    for (auto& e : g_arp_cache) {
        if (!e.valid) { copy4(e.ip, ip); copy6(e.mac, mac); e.valid = true; return; }
    }
    // Cache full, overwrite first
    copy4(g_arp_cache[0].ip, ip); copy6(g_arp_cache[0].mac, mac);
}

bool arp_cache_lookup(const uint8_t* ip, uint8_t* mac_out) noexcept {
    for (const auto& e : g_arp_cache) {
        if (e.valid && eq4(e.ip, ip)) { copy6(mac_out, e.mac); return true; }
    }
    return false;
}

void send_arp_request(const uint8_t* target_ip) noexcept {
    uint8_t frame[42]{};
    auto* eth = reinterpret_cast<EthernetHeader*>(frame);
    auto* arp = reinterpret_cast<ArpPacket*>(frame + sizeof(EthernetHeader));

    // Broadcast
    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = 0xFF;
    }
    copy6(eth->src, g_config.mac);
    eth->ethertype = htons(ETHERTYPE_ARP);

    arp->hw_type = htons(1U);
    arp->proto_type = htons(0x0800U);
    arp->hw_len = 6U;
    arp->proto_len = 4U;
    arp->opcode = htons(1U); // Request
    copy6(arp->sender_mac, g_config.mac);
    copy4(arp->sender_ip, g_config.ip);
    for (int i = 0; i < 6; ++i) {
        arp->target_mac[i] = 0;
    }
    copy4(arp->target_ip, target_ip);

    send_frame(frame, 42U);
}

void handle_arp(const uint8_t* frame, uint32_t length) noexcept {
    if (length < sizeof(EthernetHeader) + sizeof(ArpPacket)) {
        return;
    }
    const auto* arp = reinterpret_cast<const ArpPacket*>(frame + sizeof(EthernetHeader));

    if (ntohs(arp->opcode) == 2U) {
        // ARP reply: cache it
        arp_cache_add(arp->sender_ip, arp->sender_mac);
    } else if (ntohs(arp->opcode) == 1U && eq4(arp->target_ip, g_config.ip)) {
        // ARP request for us: reply
        uint8_t reply[42]{};
        auto* reth = reinterpret_cast<EthernetHeader*>(reply);
        auto* rarp = reinterpret_cast<ArpPacket*>(reply + sizeof(EthernetHeader));

        copy6(reth->dst, arp->sender_mac);
        copy6(reth->src, g_config.mac);
        reth->ethertype = htons(ETHERTYPE_ARP);

        rarp->hw_type = htons(1U);
        rarp->proto_type = htons(0x0800U);
        rarp->hw_len = 6U;
        rarp->proto_len = 4U;
        rarp->opcode = htons(2U);
        copy6(rarp->sender_mac, g_config.mac);
        copy4(rarp->sender_ip, g_config.ip);
        copy6(rarp->target_mac, arp->sender_mac);
        copy4(rarp->target_ip, arp->sender_ip);

        send_frame(reply, 42U);
    }
}

// --- IPv4 ---

} // namespace (close anonymous for send_ip_packet, reopened below)

bool send_ip_packet(const uint8_t* dst_ip, uint8_t protocol,
                    const uint8_t* payload, uint32_t payload_len) noexcept {
    if (!g_config.configured) {
        return false;
    }

    // Determine next-hop MAC via ARP
    uint8_t next_hop[4];
    // If destination is on same subnet, use it directly; else use gateway
    bool same_subnet = true;
    for (int i = 0; i < 4; ++i) {
        if ((dst_ip[i] & g_config.netmask[i]) != (g_config.ip[i] & g_config.netmask[i])) {
            same_subnet = false;
            break;
        }
    }
    copy4(next_hop, same_subnet ? dst_ip : g_config.gateway);

    uint8_t dst_mac[6];
    if (!arp_cache_lookup(next_hop, dst_mac)) {
        // Send ARP request and hope it resolves by next attempt
        send_arp_request(next_hop);
        return false;
    }

    uint32_t total = sizeof(EthernetHeader) + sizeof(Ipv4Header) + payload_len;
    if (total > 1514U) {
        return false;
    }

    uint8_t frame[1514]{};
    auto* eth = reinterpret_cast<EthernetHeader*>(frame);
    auto* ip = reinterpret_cast<Ipv4Header*>(frame + sizeof(EthernetHeader));

    copy6(eth->dst, dst_mac);
    copy6(eth->src, g_config.mac);
    eth->ethertype = htons(ETHERTYPE_IPV4);

    ip->version_ihl = 0x45U; // IPv4, IHL=5
    ip->tos = 0U;
    ip->total_length = htons(static_cast<uint16_t>(sizeof(Ipv4Header) + payload_len));
    ip->identification = htons(g_ip_id++);
    ip->flags_fragment = 0U;
    ip->ttl = 64U;
    ip->protocol = protocol;
    ip->checksum = 0U;
    copy4(ip->src_ip, g_config.ip);
    copy4(ip->dst_ip, dst_ip);
    ip->checksum = htons(ip_checksum(ip, sizeof(Ipv4Header)));

    auto* payload_dst = frame + sizeof(EthernetHeader) + sizeof(Ipv4Header);
    for (uint32_t i = 0U; i < payload_len; ++i) {
        payload_dst[i] = payload[i];
    }

    return send_frame(frame, total);
}

namespace {

void handle_icmp(const Ipv4Header* ip, const uint8_t* payload, uint32_t length) noexcept {
    if (length < sizeof(IcmpHeader)) {
        return;
    }
    const auto* icmp = reinterpret_cast<const IcmpHeader*>(payload);

    if (icmp->type == 8U) {
        // Echo request -> send echo reply
        uint8_t reply_payload[128];
        if (length > sizeof(reply_payload)) {
            return;
        }
        for (uint32_t i = 0U; i < length; ++i) {
            reply_payload[i] = payload[i];
        }
        auto* ricmp = reinterpret_cast<IcmpHeader*>(reply_payload);
        ricmp->type = 0U; // Echo reply
        ricmp->checksum = 0U;
        ricmp->checksum = htons(ip_checksum(reply_payload, length));
        send_ip_packet(ip->src_ip, IP_PROTO_ICMP, reply_payload, length);
    }
}

void handle_ipv4(const uint8_t* frame, uint32_t length) noexcept {
    if (length < sizeof(EthernetHeader) + sizeof(Ipv4Header)) {
        return;
    }
    const auto* ip = reinterpret_cast<const Ipv4Header*>(frame + sizeof(EthernetHeader));

    uint32_t ihl = (ip->version_ihl & 0x0FU) * 4U;
    uint32_t ip_total = ntohs(ip->total_length);
    if (ip_total < ihl) {
        return;
    }

    const uint8_t* payload = frame + sizeof(EthernetHeader) + ihl;
    uint32_t payload_len = ip_total - ihl;

    // Cache sender's ARP entry
    const auto* eth = reinterpret_cast<const EthernetHeader*>(frame);
    arp_cache_add(ip->src_ip, const_cast<uint8_t*>(eth->src));

    switch (ip->protocol) {
    case IP_PROTO_ICMP:
        handle_icmp(ip, payload, payload_len);
        break;
    case IP_PROTO_UDP: {
        if (payload_len < sizeof(UdpHeader)) {
            break;
        }
        const auto* udp = reinterpret_cast<const UdpHeader*>(payload);
        uint16_t src_port = ntohs(udp->src_port);
        uint16_t dst_port = ntohs(udp->dst_port);
        uint32_t udp_data_len = ntohs(udp->length);
        if (udp_data_len < sizeof(UdpHeader)) {
            break;
        }
        udp_data_len -= sizeof(UdpHeader);
        const uint8_t* udp_data = payload + sizeof(UdpHeader);

        // DHCP response (from port 67 to port 68)
        if (src_port == 67U && dst_port == 68U && udp_data_len >= sizeof(DhcpPacket)) {
            const auto* dhcp = reinterpret_cast<const DhcpPacket*>(udp_data);
            if (ntohl(dhcp->magic_cookie) == DHCP_MAGIC && dhcp->op == 2U) {
                // DHCP Offer or ACK: extract assigned IP
                copy4(g_config.ip, dhcp->yiaddr);
                // Parse options for subnet, gateway, DNS
                const uint8_t* opt = udp_data + sizeof(DhcpPacket);
                const uint8_t* opt_end = udp_data + udp_data_len;
                while (opt < opt_end && *opt != 255U) {
                    uint8_t opt_type = *opt++;
                    if (opt >= opt_end) {
                        break;
                    }
                    uint8_t opt_len = *opt++;
                    if (opt + opt_len > opt_end) {
                        break;
                    }
                    if (opt_type == 1U && opt_len == 4U) {
                        copy4(g_config.netmask, opt);
                    }
                    if (opt_type == 3U && opt_len >= 4U) {
                        copy4(g_config.gateway, opt);
                    }
                    if (opt_type == 6U && opt_len >= 4U) {
                        copy4(g_config.dns, opt);
                    }
                    opt += opt_len;
                }
                g_config.configured = true;
                console::write_string("DHCP: acquired ");
                for (int i = 0; i < 4; ++i) {
                    if (i > 0) {
                        console::write_char('.');
                    }
                    console::write_dec32(g_config.ip[i]);
                }
                console::newline();
            }
        }

        // Dispatch to socket layer
        ksocket::deliver_udp(ip->src_ip, src_port, dst_port, udp_data, udp_data_len);
        break;
    }
    case IP_PROTO_TCP: {
        const uint32_t tcp_offset = static_cast<uint32_t>((ip->version_ihl & 0x0FU) * 4U);
        const uint8_t* tcp_data = reinterpret_cast<const uint8_t*>(ip) + tcp_offset;
        const uint32_t tcp_len = ntohs(ip->total_length) - tcp_offset;
        deliver_tcp_segment(ip, tcp_data, tcp_len);
        break;
    }
    default:
        break;
    }
}

// --- DHCP ---

bool send_dhcp_discover_impl() noexcept {
    uint8_t udp_payload[300]{};
    auto* dhcp = reinterpret_cast<DhcpPacket*>(udp_payload);

    dhcp->op = 1U;
    dhcp->htype = 1U;
    dhcp->hlen = 6U;
    dhcp->xid = htonl(g_dhcp_xid);
    dhcp->flags = htons(0x8000U); // Broadcast
    copy6(dhcp->chaddr, g_config.mac);
    dhcp->magic_cookie = htonl(DHCP_MAGIC);

    // Options: DHCP Message Type = Discover (1)
    uint8_t* opt = udp_payload + sizeof(DhcpPacket);
    *opt++ = 53U; *opt++ = 1U; *opt++ = 1U; // DHCP Discover
    *opt++ = 55U; *opt++ = 4U; // Parameter Request List
    *opt++ = 1U;  // Subnet Mask
    *opt++ = 3U;  // Router
    *opt++ = 6U;  // DNS
    *opt++ = 15U; // Domain Name
    *opt++ = 255U; // End

    uint32_t dhcp_len = static_cast<uint32_t>(opt - udp_payload);

    // Build UDP header + DHCP payload
    uint8_t udp_frame[512]{};
    auto* udp = reinterpret_cast<UdpHeader*>(udp_frame);
    udp->src_port = htons(68U);  // DHCP client
    udp->dst_port = htons(67U);  // DHCP server
    udp->length = htons(static_cast<uint16_t>(sizeof(UdpHeader) + dhcp_len));
    udp->checksum = 0U; // Optional for UDP over IPv4

    for (uint32_t i = 0U; i < dhcp_len; ++i) {
        udp_frame[sizeof(UdpHeader) + i] = udp_payload[i];
    }

    // Send as broadcast IP (255.255.255.255) with source 0.0.0.0
    // We need to build the full frame manually since we don't have an IP yet
    uint32_t total = sizeof(EthernetHeader) + sizeof(Ipv4Header) +
                     sizeof(UdpHeader) + dhcp_len;
    if (total > 1514U) {
        return false;
    }

    uint8_t frame[1514]{};
    auto* eth = reinterpret_cast<EthernetHeader*>(frame);
    auto* ip = reinterpret_cast<Ipv4Header*>(frame + sizeof(EthernetHeader));

    for (int i = 0; i < 6; ++i) {
        eth->dst[i] = 0xFFU;
    }
    copy6(eth->src, g_config.mac);
    eth->ethertype = htons(ETHERTYPE_IPV4);

    ip->version_ihl = 0x45U;
    ip->total_length = htons(static_cast<uint16_t>(sizeof(Ipv4Header) + sizeof(UdpHeader) + dhcp_len));
    ip->identification = htons(g_ip_id++);
    ip->ttl = 64U;
    ip->protocol = IP_PROTO_UDP;
    set4(ip->src_ip, 0, 0, 0, 0);
    set4(ip->dst_ip, 255, 255, 255, 255);
    ip->checksum = htons(ip_checksum(ip, sizeof(Ipv4Header)));

    auto* udp_dst = frame + sizeof(EthernetHeader) + sizeof(Ipv4Header);
    for (uint32_t i = 0U; i < sizeof(UdpHeader) + dhcp_len; ++i) {
        udp_dst[i] = udp_frame[i];
    }

    return send_frame(frame, total);
}

} // namespace

// --- Public API ---

void initialize() noexcept {
    if (!virtio_net::is_initialized()) {
        return;
    }

    const uint8_t* mac = virtio_net::mac_address();
    copy6(g_config.mac, mac);
    g_config.configured = false;

    console::write_string("netstack: initialized, MAC from virtio-net");
    console::newline();
}

void process_rx_frame(const uint8_t* frame, uint32_t length) noexcept {
    if (length < sizeof(EthernetHeader)) {
        return;
    }
    const auto* eth = reinterpret_cast<const EthernetHeader*>(frame);
    uint16_t type = ntohs(eth->ethertype);

    switch (type) {
    case ETHERTYPE_ARP:
        handle_arp(frame, length);
        break;
    case ETHERTYPE_IPV4:
        handle_ipv4(frame, length);
        break;
    default:
        break;
    }
}

void poll() noexcept {
    uint8_t rx_buf[1536];
    uint32_t rx_len = 0U;
    while (virtio_net::recv(rx_buf, sizeof(rx_buf), &rx_len)) {
        process_rx_frame(rx_buf, rx_len);
    }
}

bool send_frame(const uint8_t* frame, uint32_t length) noexcept {
    return virtio_net::send(frame, length);
}

bool send_udp(const uint8_t* dst_ip, uint16_t dst_port,
              uint16_t src_port, const uint8_t* data, uint32_t length) noexcept {
    uint8_t payload[1400]{};
    if (sizeof(UdpHeader) + length > sizeof(payload)) {
        return false;
    }

    auto* udp = reinterpret_cast<UdpHeader*>(payload);
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons(static_cast<uint16_t>(sizeof(UdpHeader) + length));
    udp->checksum = 0U;

    for (uint32_t i = 0U; i < length; ++i) {
        payload[sizeof(UdpHeader) + i] = data[i];
    }

    return send_ip_packet(dst_ip, IP_PROTO_UDP, payload,
                          static_cast<uint32_t>(sizeof(UdpHeader)) + length);
}

bool dhcp_discover() noexcept {
    return send_dhcp_discover_impl();
}

bool is_configured() noexcept { return g_config.configured; }
const NetConfig& config() noexcept { return g_config; }

bool dns_resolve(const char* hostname, uint8_t* out_ip) noexcept {
    if (!g_config.configured || hostname == nullptr || out_ip == nullptr) {
        return false;
    }

    // Build DNS query packet
    uint8_t query[512]{};
    uint32_t qlen = 0U;

    // DNS header: ID=0x1234, flags=0x0100 (standard query, recursion desired)
    query[qlen++] = 0x12U; query[qlen++] = 0x34U; // ID
    query[qlen++] = 0x01U; query[qlen++] = 0x00U; // Flags: RD=1
    query[qlen++] = 0x00U; query[qlen++] = 0x01U; // QDCOUNT=1
    query[qlen++] = 0x00U; query[qlen++] = 0x00U; // ANCOUNT=0
    query[qlen++] = 0x00U; query[qlen++] = 0x00U; // NSCOUNT=0
    query[qlen++] = 0x00U; query[qlen++] = 0x00U; // ARCOUNT=0

    // Encode hostname as DNS labels
    const char* p = hostname;
    while (*p) {
        const char* dot = p;
        while (*dot && *dot != '.') {
            ++dot;
        }
        uint8_t label_len = static_cast<uint8_t>(dot - p);
        query[qlen++] = label_len;
        for (uint8_t i = 0U; i < label_len; ++i) {
            query[qlen++] = static_cast<uint8_t>(p[i]);
        }
        p = (*dot) ? dot + 1 : dot;
    }
    query[qlen++] = 0U; // Root label

    // QTYPE=A (1), QCLASS=IN (1)
    query[qlen++] = 0x00U; query[qlen++] = 0x01U;
    query[qlen++] = 0x00U; query[qlen++] = 0x01U;

    // Send via UDP to DNS server port 53
    return send_udp(g_config.dns, 53U, 1234U, query, qlen);
}

} // namespace xinim::i486::net
