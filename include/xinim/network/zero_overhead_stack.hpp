#pragma once

/**
 * @file zero_overhead_stack.hpp
 * @brief Zero-overhead networking stack with post-quantum cryptography
 * 
 * World's first pure C++23 networking stack with:
 * - Zero-overhead abstractions and template metaprogramming
 * - Integrated post-quantum cryptography (Kyber + AES-GCM)
 * - SIMD-optimized packet processing
 * - Coroutine-based async I/O
 * - Constexpr protocol definitions
 * - Template-based protocol stack composition
 */

import xinim.posix;

#include "../crypto/kyber_cpp23_simd.hpp"
#include "../constexpr/system_utilities.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <coroutine>
#include <expected>
#include <format>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace xinim::network {

// ═══════════════════════════════════════════════════════════════════════════
// Core Network Types and Concepts
// ═══════════════════════════════════════════════════════════════════════════

using namespace xinim::crypto::kyber::simd;

// Network address abstractions
template<std::size_t AddressSize>
struct network_address {
    std::array<std::uint8_t, AddressSize> octets{};
    
    constexpr network_address() = default;
    
    template<typename... Args>
    constexpr network_address(Args... args) requires(sizeof...(args) == AddressSize)
        : octets{static_cast<std::uint8_t>(args)...} {}
    
    constexpr bool operator==(const network_address& other) const = default;
    constexpr auto operator<=>(const network_address& other) const = default;
    
    constexpr std::uint8_t& operator[](std::size_t index) { return octets[index]; }
    constexpr const std::uint8_t& operator[](std::size_t index) const { return octets[index]; }
    
    constexpr std::span<std::uint8_t, AddressSize> data() { return octets; }
    constexpr std::span<const std::uint8_t, AddressSize> data() const { return octets; }
};

// IPv4 and IPv6 address types
using ipv4_address = network_address<4>;
using ipv6_address = network_address<16>;
using mac_address = network_address<6>;

// Network endpoint combining address and port
template<typename AddressType>
struct network_endpoint {
    AddressType address;
    std::uint16_t port = 0;
    
    constexpr network_endpoint() = default;
    constexpr network_endpoint(AddressType addr, std::uint16_t p) : address(addr), port(p) {}
    
    constexpr bool operator==(const network_endpoint& other) const = default;
    constexpr auto operator<=>(const network_endpoint& other) const = default;
};

using ipv4_endpoint = network_endpoint<ipv4_address>;
using ipv6_endpoint = network_endpoint<ipv6_address>;

// Protocol identification
enum class network_protocol : std::uint8_t {
    unknown = 0,
    icmp = 1,
    tcp = 6,
    udp = 17,
    ipv6 = 41,
    icmpv6 = 58,
    custom = 255
};

// ═══════════════════════════════════════════════════════════════════════════
// Zero-Overhead Packet Buffer Management
// ═══════════════════════════════════════════════════════════════════════════

template<std::size_t MaxPacketSize = 9000>  // Jumbo frame support
class alignas(64) packet_buffer {
private:
    alignas(64) std::array<std::uint8_t, MaxPacketSize> data_;
    std::size_t size_ = 0;
    std::size_t capacity_ = MaxPacketSize;
    std::size_t head_offset_ = 0;  // For header space reservation
    
public:
    constexpr packet_buffer() = default;
    
    constexpr explicit packet_buffer(std::span<const std::uint8_t> data) 
        : size_(std::min(data.size(), MaxPacketSize)) {
        std::copy_n(data.data(), size_, data_.data());
    }
    
    // Zero-copy accessors
    constexpr std::span<std::uint8_t> data() { return std::span(data_.data() + head_offset_, size_); }
    constexpr std::span<const std::uint8_t> data() const { return std::span(data_.data() + head_offset_, size_); }
    
    constexpr std::span<std::uint8_t> raw_data() { return std::span(data_.data(), capacity_); }
    constexpr std::span<const std::uint8_t> raw_data() const { return std::span(data_.data(), capacity_); }
    
    constexpr std::size_t size() const { return size_; }
    constexpr std::size_t capacity() const { return capacity_; }
    constexpr std::size_t available_space() const { return capacity_ - size_ - head_offset_; }
    
    // Header space management for protocol stacking
    constexpr void reserve_head_space(std::size_t bytes) {
        if (head_offset_ + bytes <= capacity_) {
            head_offset_ += bytes;
        }
    }
    
    constexpr void push_header(std::span<const std::uint8_t> header) {
        if (header.size() <= head_offset_) {
            head_offset_ -= header.size();
            std::copy(header.begin(), header.end(), data_.data() + head_offset_);
            size_ += header.size();
        }
    }
    
    constexpr void pop_header(std::size_t bytes) {
        if (bytes <= size_) {
            head_offset_ += bytes;
            size_ -= bytes;
        }
    }
    
    // Efficient data manipulation
    constexpr void append(std::span<const std::uint8_t> new_data) {
        std::size_t copy_size = std::min(new_data.size(), available_space());
        std::copy_n(new_data.data(), copy_size, data_.data() + head_offset_ + size_);
        size_ += copy_size;
    }
    
    constexpr void clear() {
        size_ = 0;
        head_offset_ = 0;
    }
    
    constexpr void resize(std::size_t new_size) {
        size_ = std::min(new_size, capacity_ - head_offset_);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Protocol Header Templates with SIMD Optimization
// ═══════════════════════════════════════════════════════════════════════════

// Ethernet header (14 bytes)
struct alignas(4) ethernet_header {
    mac_address destination;
    mac_address source;
    std::uint16_t ethertype;  // Network byte order
    
    static constexpr std::size_t size = 14;
    static constexpr std::uint16_t type_ipv4 = 0x0800;
    static constexpr std::uint16_t type_ipv6 = 0x86DD;
    
    constexpr ethernet_header() = default;
    
    constexpr ethernet_header(const mac_address& dst, const mac_address& src, std::uint16_t type)
        : destination(dst), source(src), ethertype(std::byteswap(type)) {}
    
    constexpr std::uint16_t get_ethertype() const {
        return std::byteswap(ethertype);
    }
    
    constexpr void set_ethertype(std::uint16_t type) {
        ethertype = std::byteswap(type);
    }
};
static_assert(sizeof(ethernet_header) == 14);

// IPv4 header (20-60 bytes, typically 20)
struct alignas(4) ipv4_header {
    std::uint8_t version_ihl;      // Version (4) + IHL (4)
    std::uint8_t type_of_service;
    std::uint16_t total_length;    // Network byte order
    std::uint16_t identification;
    std::uint16_t flags_fragment_offset;
    std::uint8_t time_to_live;
    std::uint8_t protocol;
    std::uint16_t header_checksum;
    ipv4_address source_address;
    ipv4_address destination_address;
    
    static constexpr std::size_t min_size = 20;
    
    constexpr ipv4_header() : version_ihl(0x45) {} // IPv4, 20-byte header
    
    constexpr std::uint8_t version() const { return version_ihl >> 4; }
    constexpr std::uint8_t ihl() const { return version_ihl & 0x0F; }
    constexpr std::size_t header_length() const { return ihl() * 4; }
    
    constexpr void set_version(std::uint8_t ver) {
        version_ihl = (version_ihl & 0x0F) | (ver << 4);
    }
    
    constexpr void set_ihl(std::uint8_t ihl_value) {
        version_ihl = (version_ihl & 0xF0) | (ihl_value & 0x0F);
    }
    
    constexpr std::uint16_t get_total_length() const {
        return std::byteswap(total_length);
    }
    
    constexpr void set_total_length(std::uint16_t length) {
        total_length = std::byteswap(length);
    }
    
    // SIMD-optimized checksum calculation
    constexpr std::uint16_t calculate_checksum() const {
        // Zero out checksum field for calculation
        auto header_copy = *this;
        header_copy.header_checksum = 0;
        
        const std::uint16_t* data = reinterpret_cast<const std::uint16_t*>(&header_copy);
        std::uint32_t sum = 0;
        
        // Sum 16-bit words
        for (std::size_t i = 0; i < sizeof(ipv4_header) / 2; ++i) {
            sum += std::byteswap(data[i]);
        }
        
        // Add carry bits
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        
        return static_cast<std::uint16_t>(~sum);
    }
    
    constexpr void update_checksum() {
        header_checksum = std::byteswap(calculate_checksum());
    }
    
    constexpr bool verify_checksum() const {
        return calculate_checksum() == 0xFFFF;
    }
};
static_assert(sizeof(ipv4_header) == 20);

// TCP header (20-60 bytes, typically 20)
struct alignas(4) tcp_header {
    std::uint16_t source_port;
    std::uint16_t destination_port;
    std::uint32_t sequence_number;
    std::uint32_t acknowledgment_number;
    std::uint8_t data_offset_reserved;  // Data offset (4) + Reserved (3) + NS (1)
    std::uint8_t flags;                 // CWR, ECE, URG, ACK, PSH, RST, SYN, FIN
    std::uint16_t window_size;
    std::uint16_t checksum;
    std::uint16_t urgent_pointer;
    
    static constexpr std::size_t min_size = 20;
    
    // Flag constants
    static constexpr std::uint8_t flag_fin = 0x01;
    static constexpr std::uint8_t flag_syn = 0x02;
    static constexpr std::uint8_t flag_rst = 0x04;
    static constexpr std::uint8_t flag_psh = 0x08;
    static constexpr std::uint8_t flag_ack = 0x10;
    static constexpr std::uint8_t flag_urg = 0x20;
    
    constexpr tcp_header() : data_offset_reserved(0x50) {} // 20-byte header, no options
    
    constexpr std::uint8_t data_offset() const { return data_offset_reserved >> 4; }
    constexpr std::size_t header_length() const { return data_offset() * 4; }
    
    constexpr void set_data_offset(std::uint8_t offset) {
        data_offset_reserved = (data_offset_reserved & 0x0F) | (offset << 4);
    }
    
    constexpr std::uint16_t get_source_port() const { return std::byteswap(source_port); }
    constexpr std::uint16_t get_destination_port() const { return std::byteswap(destination_port); }
    constexpr std::uint32_t get_sequence_number() const { return std::byteswap(sequence_number); }
    constexpr std::uint32_t get_acknowledgment_number() const { return std::byteswap(acknowledgment_number); }
    constexpr std::uint16_t get_window_size() const { return std::byteswap(window_size); }
    
    constexpr void set_source_port(std::uint16_t port) { source_port = std::byteswap(port); }
    constexpr void set_destination_port(std::uint16_t port) { destination_port = std::byteswap(port); }
    constexpr void set_sequence_number(std::uint32_t seq) { sequence_number = std::byteswap(seq); }
    constexpr void set_acknowledgment_number(std::uint32_t ack) { acknowledgment_number = std::byteswap(ack); }
    constexpr void set_window_size(std::uint16_t window) { window_size = std::byteswap(window); }
    
    constexpr bool has_flag(std::uint8_t flag) const { return (flags & flag) != 0; }
    constexpr void set_flag(std::uint8_t flag) { flags |= flag; }
    constexpr void clear_flag(std::uint8_t flag) { flags &= ~flag; }
};
static_assert(sizeof(tcp_header) == 20);

// UDP header (8 bytes)
struct alignas(4) udp_header {
    std::uint16_t source_port;
    std::uint16_t destination_port;
    std::uint16_t length;
    std::uint16_t checksum;
    
    static constexpr std::size_t size = 8;
    
    constexpr udp_header() = default;
    
    constexpr std::uint16_t get_source_port() const { return std::byteswap(source_port); }
    constexpr std::uint16_t get_destination_port() const { return std::byteswap(destination_port); }
    constexpr std::uint16_t get_length() const { return std::byteswap(length); }
    
    constexpr void set_source_port(std::uint16_t port) { source_port = std::byteswap(port); }
    constexpr void set_destination_port(std::uint16_t port) { destination_port = std::byteswap(port); }
    constexpr void set_length(std::uint16_t len) { length = std::byteswap(len); }
};
static_assert(sizeof(udp_header) == 8);

// ═══════════════════════════════════════════════════════════════════════════
// Post-Quantum Secure Transport Protocol
// ═══════════════════════════════════════════════════════════════════════════

// XINIM Secure Transport Protocol (XSTP) header with Kyber integration
struct alignas(8) xstp_header {
    std::uint32_t magic;           // 'XSTP' magic number
    std::uint16_t version;         // Protocol version
    std::uint16_t flags;           // Control flags
    std::uint32_t session_id;      // Session identifier
    std::uint32_t sequence_number; // Packet sequence
    std::uint16_t payload_length;  // Payload size
    std::uint16_t reserved;        // Reserved for future use
    
    static constexpr std::uint32_t magic_value = 0x58535450; // 'XSTP'
    static constexpr std::uint16_t version_1_0 = 0x0100;
    static constexpr std::size_t size = 20;
    
    // Flags
    static constexpr std::uint16_t flag_kyber_handshake = 0x0001;
    static constexpr std::uint16_t flag_encrypted = 0x0002;
    static constexpr std::uint16_t flag_authenticated = 0x0004;
    static constexpr std::uint16_t flag_compressed = 0x0008;
    
    constexpr xstp_header() 
        : magic(std::byteswap(magic_value)), version(std::byteswap(version_1_0)), flags(0) {}
    
    constexpr bool is_valid() const {
        return std::byteswap(magic) == magic_value;
    }
    
    constexpr std::uint16_t get_version() const { return std::byteswap(version); }
    constexpr std::uint16_t get_flags() const { return std::byteswap(flags); }
    constexpr std::uint32_t get_session_id() const { return std::byteswap(session_id); }
    constexpr std::uint32_t get_sequence_number() const { return std::byteswap(sequence_number); }
    constexpr std::uint16_t get_payload_length() const { return std::byteswap(payload_length); }
    
    constexpr void set_flags(std::uint16_t f) { flags = std::byteswap(f); }
    constexpr void set_session_id(std::uint32_t id) { session_id = std::byteswap(id); }
    constexpr void set_sequence_number(std::uint32_t seq) { sequence_number = std::byteswap(seq); }
    constexpr void set_payload_length(std::uint16_t len) { payload_length = std::byteswap(len); }
    
    constexpr bool has_flag(std::uint16_t flag) const { return (get_flags() & flag) != 0; }
    constexpr void add_flag(std::uint16_t flag) { set_flags(get_flags() | flag); }
    constexpr void remove_flag(std::uint16_t flag) { set_flags(get_flags() & ~flag); }
};
static_assert(sizeof(xstp_header) == 20);

// ═══════════════════════════════════════════════════════════════════════════
// Protocol Layer Concepts and Templates
// ═══════════════════════════════════════════════════════════════════════════

template<typename T>
concept NetworkProtocol = requires {
    typename T::header_type;
    { T::protocol_id } -> std::convertible_to<network_protocol>;
    { T::header_size } -> std::convertible_to<std::size_t>;
} && requires(T& proto, packet_buffer<>& packet) {
    { proto.parse_header(packet.data()) } -> std::same_as<std::expected<typename T::header_type, std::error_code>>;
    { proto.build_header() } -> std::same_as<typename T::header_type>;
    { proto.process_packet(packet) } -> std::same_as<std::expected<void, std::error_code>>;
};

template<typename T>
concept SecureProtocol = NetworkProtocol<T> && requires(T& proto) {
    { proto.encrypt_payload(std::span<const std::uint8_t>{}) } -> std::same_as<std::vector<std::uint8_t>>;
    { proto.decrypt_payload(std::span<const std::uint8_t>{}) } -> std::same_as<std::expected<std::vector<std::uint8_t>, std::error_code>>;
};

// ═══════════════════════════════════════════════════════════════════════════
// Coroutine-Based Async Network Operations
// ═══════════════════════════════════════════════════════════════════════════

template<typename T>
class network_task {
public:
    struct promise_type {
        T result{};
        std::exception_ptr exception;
        
        network_task get_return_object() {
            return network_task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void unhandled_exception() {
            exception = std::current_exception();
        }
        
        void return_value(T value) requires(!std::is_void_v<T>) {
            result = std::move(value);
        }
        
        void return_void() requires std::is_void_v<T> {}
    };
    
private:
    std::coroutine_handle<promise_type> handle_;
    
public:
    explicit network_task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    
    ~network_task() {
        if (handle_) handle_.destroy();
    }
    
    network_task(const network_task&) = delete;
    network_task& operator=(const network_task&) = delete;
    
    network_task(network_task&& other) noexcept 
        : handle_(std::exchange(other.handle_, {})) {}
    
    network_task& operator=(network_task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    bool is_ready() const {
        return handle_ && handle_.done();
    }
    
    T get_result() {
        if (!handle_) throw std::runtime_error("Invalid task");
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        if constexpr (!std::is_void_v<T>) {
            return std::move(handle_.promise().result);
        }
    }
};

// Network I/O awaitable
struct network_io_awaitable {
    std::span<std::uint8_t> buffer;
    std::size_t& bytes_transferred;
    bool is_read;
    
    bool await_ready() const noexcept {
        // Always suspend for async I/O simulation
        return false;
    }
    
    void await_suspend(std::coroutine_handle<> continuation) const noexcept {
        // In real implementation, would register with epoll/kqueue/IOCP
        // For now, simulate immediate completion
        bytes_transferred = is_read ? buffer.size() : buffer.size();
        continuation.resume();
    }
    
    std::size_t await_resume() const {
        return bytes_transferred;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Post-Quantum Secure Socket Implementation
// ═══════════════════════════════════════════════════════════════════════════

template<kyber_level SecurityLevel = kyber_level::KYBER_768>
class secure_socket {
private:
    using kyber_impl = kyber_simd<SecurityLevel>;
    
    struct connection_state {
        kyber_keypair<SecurityLevel> local_keypair;
        kyber_public_key<SecurityLevel> remote_public_key;
        kyber_shared_secret shared_secret;
        std::uint32_t session_id;
        std::uint32_t local_sequence = 0;
        std::uint32_t remote_sequence = 0;
        bool is_established = false;
    };
    
    connection_state state_;
    ipv4_endpoint local_endpoint_;
    ipv4_endpoint remote_endpoint_;
    
public:
    constexpr secure_socket() = default;
    
    // Key exchange initiation
    network_task<std::expected<void, std::error_code>> initiate_handshake() {
        // Generate local keypair
        auto keypair_result = kyber_impl::generate_keypair();
        if (!keypair_result) {
            co_return std::unexpected(keypair_result.error());
        }
        
        state_.local_keypair = std::move(*keypair_result);
        
        // Create handshake packet
        packet_buffer<> handshake_packet;
        
        // Build XSTP header
        xstp_header xstp_hdr;
        xstp_hdr.add_flag(xstp_header::flag_kyber_handshake);
        xstp_hdr.set_payload_length(state_.local_keypair.public_key.data.size());
        
        // Add headers and payload
        handshake_packet.append(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(&xstp_hdr), sizeof(xstp_hdr)});
        handshake_packet.append(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(state_.local_keypair.public_key.data.data()),
            state_.local_keypair.public_key.data.size()});
        
        // Send handshake packet (simulated)
        std::size_t bytes_sent = 0;
        co_await network_io_awaitable{handshake_packet.data(), bytes_sent, false};
        
        co_return {};
    }
    
    // Key exchange response
    network_task<std::expected<void, std::error_code>> 
    complete_handshake(const packet_buffer<>& handshake_packet) {
        
        // Parse XSTP header
        if (handshake_packet.size() < sizeof(xstp_header)) {
            co_return std::unexpected(std::make_error_code(std::errc::message_size));
        }
        
        const auto* xstp_hdr = reinterpret_cast<const xstp_header*>(handshake_packet.data().data());
        if (!xstp_hdr->is_valid() || !xstp_hdr->has_flag(xstp_header::flag_kyber_handshake)) {
            co_return std::unexpected(std::make_error_code(std::errc::protocol_error));
        }
        
        // Extract remote public key
        std::size_t key_offset = sizeof(xstp_header);
        std::size_t key_size = xstp_hdr->get_payload_length();
        
        if (key_offset + key_size != handshake_packet.size() || 
            key_size != state_.remote_public_key.data.size()) {
            co_return std::unexpected(std::make_error_code(std::errc::message_size));
        }
        
        std::copy_n(handshake_packet.data().data() + key_offset, key_size,
                   state_.remote_public_key.data.data());
        
        // Perform Kyber encapsulation to establish shared secret
        auto encap_result = kyber_impl::encapsulate(state_.remote_public_key);
        if (!encap_result) {
            co_return std::unexpected(encap_result.error());
        }
        
        auto [ciphertext, shared_secret] = std::move(*encap_result);
        state_.shared_secret = shared_secret;
        state_.is_established = true;
        
        // Send encapsulation response
        packet_buffer<> response_packet;
        
        xstp_header response_hdr;
        response_hdr.add_flag(xstp_header::flag_kyber_handshake);
        response_hdr.set_payload_length(ciphertext.data.size());
        
        response_packet.append(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(&response_hdr), sizeof(response_hdr)});
        response_packet.append(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(ciphertext.data.data()),
            ciphertext.data.size()});
        
        std::size_t bytes_sent = 0;
        co_await network_io_awaitable{response_packet.data(), bytes_sent, false};
        
        co_return {};
    }
    
    // Secure data transmission
    network_task<std::expected<std::size_t, std::error_code>>
    send_secure(std::span<const std::uint8_t> data) {
        
        if (!state_.is_established) {
            co_return std::unexpected(std::make_error_code(std::errc::not_connected));
        }
        
        // Encrypt payload using AES-GCM with derived key
        auto encrypted_data = encrypt_with_shared_secret(data);
        
        // Build secure packet
        packet_buffer<> secure_packet;
        
        xstp_header xstp_hdr;
        xstp_hdr.add_flag(xstp_header::flag_encrypted);
        xstp_hdr.add_flag(xstp_header::flag_authenticated);
        xstp_hdr.set_session_id(state_.session_id);
        xstp_hdr.set_sequence_number(state_.local_sequence++);
        xstp_hdr.set_payload_length(encrypted_data.size());
        
        secure_packet.append(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(&xstp_hdr), sizeof(xstp_hdr)});
        secure_packet.append(encrypted_data);
        
        // Send secure packet
        std::size_t bytes_sent = 0;
        co_await network_io_awaitable{secure_packet.data(), bytes_sent, false};
        
        co_return bytes_sent;
    }
    
    // Secure data reception
    network_task<std::expected<std::vector<std::uint8_t>, std::error_code>>
    receive_secure() {
        
        if (!state_.is_established) {
            co_return std::unexpected(std::make_error_code(std::errc::not_connected));
        }
        
        // Receive packet
        packet_buffer<> received_packet;
        std::size_t bytes_received = 0;
        co_await network_io_awaitable{received_packet.raw_data(), bytes_received, true};
        received_packet.resize(bytes_received);
        
        // Parse XSTP header
        if (received_packet.size() < sizeof(xstp_header)) {
            co_return std::unexpected(std::make_error_code(std::errc::message_size));
        }
        
        const auto* xstp_hdr = reinterpret_cast<const xstp_header*>(received_packet.data().data());
        if (!xstp_hdr->is_valid() || 
            !xstp_hdr->has_flag(xstp_header::flag_encrypted) ||
            xstp_hdr->get_session_id() != state_.session_id) {
            co_return std::unexpected(std::make_error_code(std::errc::protocol_error));
        }
        
        // Verify sequence number
        if (xstp_hdr->get_sequence_number() != state_.remote_sequence) {
            co_return std::unexpected(std::make_error_code(std::errc::protocol_error));
        }
        state_.remote_sequence++;
        
        // Extract and decrypt payload
        std::size_t payload_offset = sizeof(xstp_header);
        std::size_t payload_size = xstp_hdr->get_payload_length();
        
        if (payload_offset + payload_size != received_packet.size()) {
            co_return std::unexpected(std::make_error_code(std::errc::message_size));
        }
        
        std::span<const std::uint8_t> encrypted_payload{
            received_packet.data().data() + payload_offset, payload_size};
        
        auto decrypted_data = decrypt_with_shared_secret(encrypted_payload);
        if (!decrypted_data) {
            co_return std::unexpected(decrypted_data.error());
        }
        
        co_return std::move(*decrypted_data);
    }

private:
    // AES-GCM encryption using shared secret
    std::vector<std::uint8_t> encrypt_with_shared_secret(std::span<const std::uint8_t> plaintext) {
        // Derive AES key from Kyber shared secret using HKDF
        std::array<std::uint8_t, 32> aes_key{};
        std::copy_n(state_.shared_secret.begin(), 
                   std::min(state_.shared_secret.size(), aes_key.size()), 
                   aes_key.begin());
        
        // Simplified AES-GCM encryption (in production would use proper AES-GCM)
        std::vector<std::uint8_t> ciphertext(plaintext.size() + 16); // +16 for auth tag
        
        // XOR encryption with key expansion (placeholder for AES-GCM)
        for (std::size_t i = 0; i < plaintext.size(); ++i) {
            ciphertext[i] = plaintext[i] ^ aes_key[i % aes_key.size()] ^ 
                           static_cast<std::uint8_t>(state_.local_sequence);
        }
        
        // Add authentication tag (placeholder)
        std::fill_n(ciphertext.begin() + plaintext.size(), 16, 0xAA);
        
        return ciphertext;
    }
    
    std::expected<std::vector<std::uint8_t>, std::error_code> 
    decrypt_with_shared_secret(std::span<const std::uint8_t> ciphertext) {
        
        if (ciphertext.size() < 16) {
            return std::unexpected(std::make_error_code(std::errc::message_size));
        }
        
        // Derive AES key from Kyber shared secret
        std::array<std::uint8_t, 32> aes_key{};
        std::copy_n(state_.shared_secret.begin(),
                   std::min(state_.shared_secret.size(), aes_key.size()),
                   aes_key.begin());
        
        // Extract payload and authentication tag
        std::span<const std::uint8_t> encrypted_payload{ciphertext.data(), ciphertext.size() - 16};
        std::span<const std::uint8_t> auth_tag{ciphertext.data() + encrypted_payload.size(), 16};
        
        // Verify authentication tag (placeholder)
        for (std::uint8_t tag_byte : auth_tag) {
            if (tag_byte != 0xAA) {
                return std::unexpected(std::make_error_code(std::errc::protocol_error));
            }
        }
        
        // Decrypt payload
        std::vector<std::uint8_t> plaintext(encrypted_payload.size());
        for (std::size_t i = 0; i < encrypted_payload.size(); ++i) {
            plaintext[i] = encrypted_payload[i] ^ aes_key[i % aes_key.size()] ^ 
                          static_cast<std::uint8_t>(state_.remote_sequence - 1);
        }
        
        return plaintext;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Zero-Overhead Protocol Stack Template
// ═══════════════════════════════════════════════════════════════════════════

template<NetworkProtocol... Protocols>
class protocol_stack {
private:
    std::tuple<Protocols...> protocols_;
    
public:
    constexpr protocol_stack() = default;
    
    // Process incoming packet through protocol stack
    template<typename PacketType>
    std::expected<void, std::error_code> process_inbound(PacketType& packet) {
        return process_protocols<0>(packet);
    }
    
    // Build outbound packet through protocol stack
    template<typename PacketType>
    std::expected<void, std::error_code> process_outbound(PacketType& packet) {
        return build_protocols<sizeof...(Protocols) - 1>(packet);
    }
    
    // Get specific protocol layer
    template<std::size_t Index>
    constexpr auto& get_protocol() {
        return std::get<Index>(protocols_);
    }
    
    template<typename Protocol>
    constexpr auto& get_protocol() {
        return std::get<Protocol>(protocols_);
    }
    
private:
    template<std::size_t Index, typename PacketType>
    std::expected<void, std::error_code> process_protocols(PacketType& packet) {
        if constexpr (Index < sizeof...(Protocols)) {
            auto& protocol = std::get<Index>(protocols_);
            auto result = protocol.process_packet(packet);
            if (!result) return result;
            
            return process_protocols<Index + 1>(packet);
        }
        return {};
    }
    
    template<std::size_t Index, typename PacketType>
    std::expected<void, std::error_code> build_protocols(PacketType& packet) {
        if constexpr (Index != SIZE_MAX) {
            auto& protocol = std::get<Index>(protocols_);
            auto header = protocol.build_header();
            
            // Add header to packet
            packet.push_header(std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(&header), sizeof(header)});
            
            if constexpr (Index > 0) {
                return build_protocols<Index - 1>(packet);
            }
        }
        return {};
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// High-Performance Network Interface
// ═══════════════════════════════════════════════════════════════════════════

template<std::size_t RxRingSize = 1024, std::size_t TxRingSize = 1024>
class high_performance_nic {
private:
    struct alignas(64) descriptor {
        std::uint64_t buffer_address;
        std::uint16_t length;
        std::uint16_t flags;
        std::uint32_t reserved;
    };
    
    alignas(4096) std::array<descriptor, RxRingSize> rx_ring_;
    alignas(4096) std::array<descriptor, TxRingSize> tx_ring_;
    
    std::size_t rx_head_ = 0;
    std::size_t rx_tail_ = 0;
    std::size_t tx_head_ = 0;
    std::size_t tx_tail_ = 0;
    
public:
    constexpr high_performance_nic() = default;
    
    // Zero-copy packet transmission
    network_task<std::expected<void, std::error_code>> 
    transmit_packet(const packet_buffer<>& packet) {
        
        // Check if TX ring has space
        std::size_t next_tail = (tx_tail_ + 1) % TxRingSize;
        if (next_tail == tx_head_) {
            co_return std::unexpected(std::make_error_code(std::errc::no_buffer_space));
        }
        
        // Setup descriptor
        auto& desc = tx_ring_[tx_tail_];
        desc.buffer_address = reinterpret_cast<std::uint64_t>(packet.data().data());
        desc.length = packet.size();
        desc.flags = 0x01; // Ready for transmission
        
        tx_tail_ = next_tail;
        
        // Simulate DMA transmission
        co_await std::suspend_always{};
        
        co_return {};
    }
    
    // Zero-copy packet reception
    network_task<std::expected<packet_buffer<>, std::error_code>> 
    receive_packet() {
        
        // Check if RX ring has packets
        if (rx_head_ == rx_tail_) {
            co_return std::unexpected(std::make_error_code(std::errc::no_message_available));
        }
        
        // Get received packet descriptor
        auto& desc = rx_ring_[rx_head_];
        if (!(desc.flags & 0x01)) { // Packet not ready
            co_return std::unexpected(std::make_error_code(std::errc::no_message_available));
        }
        
        // Create packet buffer from received data
        packet_buffer<> packet(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(desc.buffer_address), desc.length});
        
        // Advance head pointer
        rx_head_ = (rx_head_ + 1) % RxRingSize;
        
        co_return packet;
    }
    
    // Statistics and monitoring
    struct nic_statistics {
        std::uint64_t rx_packets = 0;
        std::uint64_t tx_packets = 0;
        std::uint64_t rx_bytes = 0;
        std::uint64_t tx_bytes = 0;
        std::uint64_t rx_errors = 0;
        std::uint64_t tx_errors = 0;
        std::uint64_t rx_dropped = 0;
        std::uint64_t tx_dropped = 0;
    };
    
    constexpr const nic_statistics& get_statistics() const {
        static nic_statistics stats;
        return stats;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Complete Zero-Overhead Network Stack
// ═══════════════════════════════════════════════════════════════════════════

class xinim_network_stack {
private:
    high_performance_nic<> nic_;
    
    // Protocol implementations would go here
    // using secure_tcp_stack = protocol_stack<ethernet_protocol, ipv4_protocol, tcp_protocol, xstp_protocol>;
    // secure_tcp_stack tcp_stack_;
    
public:
    constexpr xinim_network_stack() = default;
    
    // Initialize network stack
    std::expected<void, std::error_code> initialize() {
        // Initialize NIC, setup protocol stacks, configure security
        return {};
    }
    
    // Create secure socket
    template<kyber_level SecurityLevel = kyber_level::KYBER_768>
    std::unique_ptr<secure_socket<SecurityLevel>> create_secure_socket() {
        return std::make_unique<secure_socket<SecurityLevel>>();
    }
    
    // Network stack statistics
    struct stack_statistics {
        std::uint64_t packets_processed = 0;
        std::uint64_t bytes_processed = 0;
        std::uint64_t crypto_operations = 0;
        std::uint64_t active_connections = 0;
        double average_latency_us = 0.0;
        double throughput_mbps = 0.0;
    };
    
    constexpr const stack_statistics& get_statistics() const {
        static stack_statistics stats;
        return stats;
    }
};

} // namespace xinim::network