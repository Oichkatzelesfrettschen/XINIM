/**
 * @file wget_i486.c
 * @brief Minimal HTTP/1.0 GET client for XINIM i486.
 *
 * Usage: wget http://host:port/path [-O output]
 * Connects via TCP socket, sends GET request, saves response body.
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

/* Socket syscall wrappers using int $0x80 */
struct sockaddr_in {
    unsigned short family;
    unsigned short port;
    unsigned char addr[4];
    unsigned char zero[8];
};

static unsigned int sys5(unsigned int nr, unsigned int a, unsigned int b,
                         unsigned int c, unsigned int d, unsigned int e) {
    unsigned int r;
    __asm__ volatile("int $0x80"
        : "=a"(r)
        : "a"(nr), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e)
        : "memory", "cc");
    return r;
}

static int xsocket(int domain, int type, int proto) {
    return (int)sys5(81, (unsigned)domain, (unsigned)type, (unsigned)proto, 0, 0);
}

static int xconnect(int fd, const struct sockaddr_in *addr) {
    return (int)sys5(85, (unsigned)fd,
                     (unsigned)(unsigned long)addr,
                     sizeof(struct sockaddr_in), 0, 0);
}

static int xsend(int fd, const void *buf, unsigned len) {
    return (int)sys5(86, (unsigned)fd,
                     (unsigned)(unsigned long)buf, len, 0, 0);
}

static int xrecv(int fd, void *buf, unsigned len) {
    return (int)sys5(87, (unsigned)fd,
                     (unsigned)(unsigned long)buf, len, 0, 0);
}

static void write_str(const char *s) { write(2, s, strlen(s)); }

static unsigned short htons_u(unsigned short h) {
    return (unsigned short)((h >> 8) | (h << 8));
}

static int parse_url(const char *url, char *host, int host_cap,
                     unsigned short *port, char *path, int path_cap) {
    /* Skip http:// */
    if (strncmp(url, "http://", 7) == 0) url += 7;

    /* Extract host:port */
    const char *slash = strchr(url, '/');
    const char *colon = strchr(url, ':');
    int host_len;

    if (colon && (!slash || colon < slash)) {
        host_len = (int)(colon - url);
        if (host_len >= host_cap) return -1;
        memcpy(host, url, (size_t)host_len);
        host[host_len] = '\0';
        *port = 0;
        const char *p = colon + 1;
        while (*p >= '0' && *p <= '9' && (!slash || p < slash)) {
            *port = (unsigned short)(*port * 10 + (*p - '0'));
            ++p;
        }
    } else {
        host_len = slash ? (int)(slash - url) : (int)strlen(url);
        if (host_len >= host_cap) return -1;
        memcpy(host, url, (size_t)host_len);
        host[host_len] = '\0';
        *port = 80;
    }

    if (slash) {
        int plen = (int)strlen(slash);
        if (plen >= path_cap) return -1;
        memcpy(path, slash, (size_t)plen + 1);
    } else {
        path[0] = '/';
        path[1] = '\0';
    }
    return 0;
}

/* Simple IP address parser (a.b.c.d) */
static int parse_ip(const char *s, unsigned char *out) {
    for (int i = 0; i < 4; ++i) {
        unsigned v = 0;
        while (*s >= '0' && *s <= '9') { v = v * 10 + (unsigned)(*s - '0'); ++s; }
        if (v > 255) return -1;
        out[i] = (unsigned char)v;
        if (i < 3) { if (*s != '.') return -1; ++s; }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        write_str("usage: wget http://host:port/path [-O output]\n");
        return 1;
    }

    const char *url = argv[1];
    const char *output_file = 0;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-O") == 0 && i + 1 < argc) output_file = argv[++i];
    }

    char host[128], path[256];
    unsigned short port;
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        write_str("wget: invalid URL\n");
        return 1;
    }

    /* Resolve host to IP (for now, only dotted-decimal IPs supported) */
    unsigned char ip[4];
    if (parse_ip(host, ip) != 0) {
        write_str("wget: cannot resolve host (use IP address)\n");
        return 1;
    }

    write_str("Connecting to ");
    write(2, host, strlen(host));
    write_str(":");
    char pstr[8]; int plen = 0;
    { unsigned v = port; if (v == 0) pstr[plen++] = '0';
      else { while (v) { pstr[plen++] = (char)('0' + v % 10); v /= 10; }
             for (int i=0;i<plen/2;++i){char t=pstr[i];pstr[i]=pstr[plen-1-i];pstr[plen-1-i]=t;}}}
    write(2, pstr, plen);
    write_str("...\n");

    int sock = xsocket(2, 1, 0); /* AF_INET, SOCK_STREAM */
    if (sock < 0) {
        write_str("wget: socket failed\n");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.family = 2;
    addr.port = htons_u(port);
    memcpy(addr.addr, ip, 4);

    if (xconnect(sock, &addr) < 0) {
        write_str("wget: connect failed\n");
        return 1;
    }

    /* Send HTTP GET request */
    char request[512];
    int rlen = 0;
    const char *get = "GET ";
    memcpy(request + rlen, get, 4); rlen += 4;
    memcpy(request + rlen, path, strlen(path)); rlen += (int)strlen(path);
    const char *http = " HTTP/1.0\r\nHost: ";
    memcpy(request + rlen, http, strlen(http)); rlen += (int)strlen(http);
    memcpy(request + rlen, host, strlen(host)); rlen += (int)strlen(host);
    const char *end = "\r\nConnection: close\r\n\r\n";
    memcpy(request + rlen, end, strlen(end)); rlen += (int)strlen(end);

    xsend(sock, request, (unsigned)rlen);

    /* Receive response */
    int outfd = 1; /* stdout by default */
    if (output_file) {
        outfd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outfd < 0) {
            write_str("wget: cannot create output file\n");
            return 1;
        }
    }

    char buf[1024];
    int header_done = 0;
    int total = 0;
    int n;
    while ((n = xrecv(sock, buf, sizeof(buf))) > 0) {
        if (!header_done) {
            /* Find end of HTTP headers (\r\n\r\n) */
            for (int i = 0; i < n - 3; ++i) {
                if (buf[i]=='\r' && buf[i+1]=='\n' && buf[i+2]=='\r' && buf[i+3]=='\n') {
                    header_done = 1;
                    int body_start = i + 4;
                    if (body_start < n) {
                        write(outfd, buf + body_start, n - body_start);
                        total += n - body_start;
                    }
                    break;
                }
            }
        } else {
            write(outfd, buf, n);
            total += n;
        }
    }

    if (outfd != 1) close(outfd);

    write_str("Downloaded ");
    char tstr[12]; int tlen = 0;
    { int v = total; if (v == 0) tstr[tlen++] = '0';
      else { while (v) { tstr[tlen++] = (char)('0' + v % 10); v /= 10; }
             for (int i=0;i<tlen/2;++i){char t=tstr[i];tstr[i]=tstr[tlen-1-i];tstr[tlen-1-i]=t;}}}
    write(2, tstr, tlen);
    write_str(" bytes\n");

    return 0;
}
