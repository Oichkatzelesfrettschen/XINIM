// wget -- HTTP/1.0 file downloader (subset)
// Cleanroom C++23 implementation for XINIM i486.
// Supports: http://host:port/path [-O output] [-q quiet]
// Uses XINIM kernel socket syscalls directly (dietlibc i486 lacks send/recv wrappers).
// Syscall numbers per include/xinim/sys/syscalls.h.

#include <fcntl.h>
#include <unistd.h>

namespace {

// ============================================================
// I/O helpers
// ============================================================

void write_all(int fd, const char* buf, int len) noexcept {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w; len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) noexcept {
    int n = 0; while (s[n]) ++n;
    write_all(fd, s, n);
}

int str_len(const char* s) noexcept { int n = 0; while (s[n]) ++n; return n; }

void str_copy(char* dst, const char* src, int cap) noexcept {
    int i = 0;
    while (i < cap - 1 && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

bool str_eq(const char* a, const char* b) noexcept {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

bool str_starts(const char* s, const char* prefix) noexcept {
    while (*prefix) { if (*s != *prefix) return false; ++s; ++prefix; }
    return true;
}

void write_uint(int fd, unsigned long v) noexcept {
    char tmp[24]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    else { while (v > 0) { tmp[n++] = static_cast<char>('0' + v % 10); v /= 10; } }
    char out[24];
    for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    write_all(fd, out, n);
}

// Host-to-network byte order for port (big-endian)
constexpr unsigned short htons16(unsigned short h) noexcept {
    return static_cast<unsigned short>((h >> 8) | (h << 8));
}

// ============================================================
// XINIM i486 socket syscall wrappers (int $0x80)
// Syscall numbers from include/xinim/sys/syscalls.h.
// ============================================================

// Generic 5-argument syscall trampoline.
static unsigned int sys5(unsigned int nr, unsigned int a, unsigned int b,
                         unsigned int c, unsigned int d, unsigned int e) noexcept {
    unsigned int r;
    __asm__ volatile(
        "int $0x80"
        : "=a"(r)
        : "a"(nr), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e)
        : "memory", "cc");
    return r;
}

// AF_INET=2, SOCK_STREAM=1
struct SockAddrIn {
    unsigned short sa_family; // AF_INET = 2
    unsigned short sin_port;  // network byte order
    unsigned char  sin_addr[4];
    unsigned char  _pad[8];
};

static int xsocket(int domain, int type, int proto) noexcept {
    return static_cast<int>(sys5(81U, static_cast<unsigned>(domain),
                                 static_cast<unsigned>(type),
                                 static_cast<unsigned>(proto), 0U, 0U));
}

static int xconnect(int fd, const SockAddrIn* addr) noexcept {
    return static_cast<int>(sys5(85U, static_cast<unsigned>(fd),
                                 static_cast<unsigned>(reinterpret_cast<unsigned long>(addr)),
                                 static_cast<unsigned>(sizeof(SockAddrIn)), 0U, 0U));
}

// SYS_sendto = 86: sendto(fd, buf, len, flags, null, 0) acts as send()
static int xsend(int fd, const void* buf, unsigned len) noexcept {
    return static_cast<int>(sys5(86U, static_cast<unsigned>(fd),
                                 static_cast<unsigned>(reinterpret_cast<unsigned long>(buf)),
                                 len, 0U, 0U));
}

// SYS_recvfrom = 87: recvfrom(fd, buf, len, flags, null, null) acts as recv()
static int xrecv(int fd, void* buf, unsigned len) noexcept {
    return static_cast<int>(sys5(87U, static_cast<unsigned>(fd),
                                 static_cast<unsigned>(reinterpret_cast<unsigned long>(buf)),
                                 len, 0U, 0U));
}

// ============================================================
// URL parser
// ============================================================

struct Url {
    char host[128];
    char path[256];
    unsigned short port;
    bool ok;
};

Url parse_url(const char* url) noexcept {
    Url u{}; u.port = 80U; u.ok = false;

    if (str_starts(url, "http://")) url += 7;
    else if (str_starts(url, "https://")) {
        write_str(2, "wget: HTTPS not supported; use HTTP\n");
        return u;
    }

    // Find end of host:port section
    const char* slash = url;
    while (*slash && *slash != '/') ++slash;
    const char* colon = url;
    while (colon < slash && *colon != ':') ++colon;

    int host_len;
    if (colon < slash) {
        host_len = static_cast<int>(colon - url);
        if (host_len >= 127) return u;
        for (int i = 0; i < host_len; ++i) u.host[i] = url[i];
        u.host[host_len] = '\0';
        u.port = 0U;
        const char* pp = colon + 1;
        while (pp < slash && *pp >= '0' && *pp <= '9')
            u.port = static_cast<unsigned short>(u.port * 10U + static_cast<unsigned>(*pp++ - '0'));
    } else {
        host_len = static_cast<int>(slash - url);
        if (host_len >= 127) return u;
        for (int i = 0; i < host_len; ++i) u.host[i] = url[i];
        u.host[host_len] = '\0';
    }

    if (*slash == '\0') { u.path[0] = '/'; u.path[1] = '\0'; }
    else { str_copy(u.path, slash, 256); }

    u.ok = true;
    return u;
}

// ============================================================
// IPv4 dotted-decimal parser (no DNS on bare-metal)
// ============================================================

bool parse_ipv4(const char* s, unsigned char out[4]) noexcept {
    for (int i = 0; i < 4; ++i) {
        if (*s < '0' || *s > '9') return false;
        unsigned v = 0U;
        while (*s >= '0' && *s <= '9') { v = v * 10U + static_cast<unsigned>(*s - '0'); ++s; }
        if (v > 255U) return false;
        out[i] = static_cast<unsigned char>(v);
        if (i < 3) { if (*s != '.') return false; ++s; }
    }
    return true;
}

// ============================================================
// HTTP header helpers
// ============================================================

// Find end of HTTP headers (\r\n\r\n or \n\n), return body offset or -1.
int find_body(const char* buf, int len) noexcept {
    for (int i = 0; i < len - 3; ++i) {
        if (buf[i]=='\r' && buf[i+1]=='\n' && buf[i+2]=='\r' && buf[i+3]=='\n') return i + 4;
    }
    for (int i = 0; i < len - 1; ++i) {
        if (buf[i]=='\n' && buf[i+1]=='\n') return i + 2;
    }
    return -1;
}

// Parse HTTP status code from response line.
int parse_status(const char* buf, int len) noexcept {
    if (len < 12 || !str_starts(buf, "HTTP/")) return 0;
    const char* p = buf + 5;
    while (*p && *p != ' ') ++p;
    if (*p == ' ') ++p;
    int code = 0;
    while (*p >= '0' && *p <= '9') { code = code * 10 + (*p - '0'); ++p; }
    return code;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: wget http://host[:port]/path [-O output] [-q]\n");
        return 1;
    }

    const char* url_str = nullptr;
    const char* output_file = nullptr;
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        if (str_eq(argv[i], "-O") && i + 1 < argc) { output_file = argv[++i]; }
        else if (str_eq(argv[i], "-q") || str_eq(argv[i], "--quiet")) { quiet = true; }
        else if (argv[i][0] != '-') { url_str = argv[i]; }
    }

    if (url_str == nullptr) { write_str(2, "wget: no URL specified\n"); return 1; }

    Url url = parse_url(url_str);
    if (!url.ok) return 1;

    unsigned char ip[4];
    if (!parse_ipv4(url.host, ip)) {
        write_str(2, "wget: cannot resolve '"); write_str(2, url.host);
        write_str(2, "' -- only IPv4 addresses supported (no DNS)\n");
        return 1;
    }

    if (!quiet) {
        write_str(2, "Connecting to "); write_str(2, url.host);
        write_str(2, ":"); write_uint(2, url.port); write_str(2, "...\n");
    }

    int sock = xsocket(2, 1, 0); // AF_INET=2, SOCK_STREAM=1
    if (sock < 0) { write_str(2, "wget: socket() failed\n"); return 1; }

    SockAddrIn addr{};
    addr.sa_family = 2U; // AF_INET
    addr.sin_port  = htons16(url.port);
    addr.sin_addr[0] = ip[0]; addr.sin_addr[1] = ip[1];
    addr.sin_addr[2] = ip[2]; addr.sin_addr[3] = ip[3];

    if (xconnect(sock, &addr) < 0) {
        write_str(2, "wget: connect failed\n");
        close(sock); return 1;
    }
    if (!quiet) write_str(2, "Connected.\n");

    // Build HTTP/1.0 GET request
    char request[1024]; int rlen = 0;
    auto req_append = [&](const char* s) noexcept {
        int n = str_len(s);
        if (rlen + n < 1020) { for (int i = 0; i < n; ++i) request[rlen++] = s[i]; }
    };
    req_append("GET "); req_append(url.path); req_append(" HTTP/1.0\r\n");
    req_append("Host: "); req_append(url.host); req_append("\r\n");
    req_append("User-Agent: wget-xinim/1.0\r\n");
    req_append("Connection: close\r\n\r\n");

    // Send request
    int sent = 0;
    while (sent < rlen) {
        int w = xsend(sock, request + sent, static_cast<unsigned>(rlen - sent));
        if (w <= 0) { write_str(2, "wget: send failed\n"); close(sock); return 1; }
        sent += w;
    }

    // Open output file
    int outfd;
    if (output_file != nullptr) {
        outfd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outfd < 0) {
            write_str(2, "wget: cannot create: "); write_str(2, output_file); write_str(2, "\n");
            close(sock); return 1;
        }
    } else {
        outfd = 1; // stdout
    }

    // Receive headers + body
    char hdr_buf[4096]; int hdr_len = 0;
    int body_off = -1;
    int status_code = 0;
    unsigned long total_bytes = 0UL;

    // Drain until headers complete
    while (body_off < 0) {
        char buf[1024];
        int n = xrecv(sock, buf, sizeof(buf));
        if (n <= 0) break;

        // Accumulate in header buffer
        int to_copy = n < (4095 - hdr_len) ? n : (4095 - hdr_len);
        for (int i = 0; i < to_copy; ++i) hdr_buf[hdr_len + i] = buf[i];
        hdr_len += to_copy; hdr_buf[hdr_len] = '\0';

        body_off = find_body(hdr_buf, hdr_len);
        if (body_off >= 0) {
            status_code = parse_status(hdr_buf, hdr_len);
            if (!quiet) {
                write_str(2, "HTTP status: ");
                write_uint(2, static_cast<unsigned long>(status_code));
                write_str(2, "\n");
            }
            // Write body portion that arrived with headers
            int body_in_hdr = hdr_len - body_off;
            if (body_in_hdr > 0) {
                write_all(outfd, hdr_buf + body_off, body_in_hdr);
                total_bytes += static_cast<unsigned long>(body_in_hdr);
            }
        }
    }

    // Stream remaining body
    for (;;) {
        char buf[4096];
        int n = xrecv(sock, buf, sizeof(buf));
        if (n <= 0) break;
        write_all(outfd, buf, n);
        total_bytes += static_cast<unsigned long>(n);
    }

    close(sock);
    if (output_file != nullptr) close(outfd);

    if (!quiet) {
        write_str(2, "Downloaded "); write_uint(2, total_bytes); write_str(2, " bytes\n");
    }

    return (status_code >= 400 || (status_code == 0 && total_bytes == 0UL)) ? 1 : 0;
}
