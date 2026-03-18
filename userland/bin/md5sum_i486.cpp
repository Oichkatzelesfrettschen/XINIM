// md5sum -- compute and check MD5 message digests (RFC 1321)
// Cleanroom C++23 implementation.
// Full MD5 algorithm, not a stub.

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr int kBufSize = 4096;

void write_all(int fd, const char* buf, int len) {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w;
        len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    write_all(fd, s, n);
}

int slen(const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// -- MD5 internals --

struct MD5_CTX {
    unsigned state[4];
    unsigned count[2];
    unsigned char buffer[64];
};

constexpr unsigned T[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

constexpr unsigned char S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

unsigned rol(unsigned x, unsigned n) {
    return (x << n) | (x >> (32 - n));
}

void md5_transform(unsigned* state, const unsigned char* block) {
    unsigned a = state[0], b = state[1], c = state[2], d = state[3];
    unsigned M[16];
    for (int i = 0; i < 16; ++i)
        M[i] = static_cast<unsigned>(block[i * 4])
             | (static_cast<unsigned>(block[i * 4 + 1]) << 8)
             | (static_cast<unsigned>(block[i * 4 + 2]) << 16)
             | (static_cast<unsigned>(block[i * 4 + 3]) << 24);

    for (int i = 0; i < 64; ++i) {
        unsigned f, g;
        if (i < 16) {
            f = (b & c) | ((~b) & d);
            g = static_cast<unsigned>(i);
        } else if (i < 32) {
            f = (d & b) | ((~d) & c);
            g = static_cast<unsigned>(5 * i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = static_cast<unsigned>(3 * i + 5) % 16;
        } else {
            f = c ^ (b | (~d));
            g = static_cast<unsigned>(7 * i) % 16;
        }
        unsigned tmp = d;
        d = c;
        c = b;
        b = b + rol(a + f + T[i] + M[g], S[i]);
        a = tmp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void md5_init(MD5_CTX* ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->count[0] = ctx->count[1] = 0;
}

void md5_update(MD5_CTX* ctx, const unsigned char* data, unsigned len) {
    unsigned idx = (ctx->count[0] >> 3) & 63;
    ctx->count[0] += len << 3;
    if (ctx->count[0] < (len << 3)) ctx->count[1]++;
    ctx->count[1] += len >> 29;

    unsigned part = 64 - idx;
    unsigned i = 0;

    if (len >= part) {
        for (unsigned j = 0; j < part; ++j) ctx->buffer[idx + j] = data[j];
        md5_transform(ctx->state, ctx->buffer);
        for (i = part; i + 63 < len; i += 64)
            md5_transform(ctx->state, data + i);
        idx = 0;
    }
    for (unsigned j = 0; j < len - i; ++j)
        ctx->buffer[idx + j] = data[i + j];
}

void md5_final(unsigned char digest[16], MD5_CTX* ctx) {
    unsigned char bits[8];
    for (int i = 0; i < 4; ++i) {
        bits[i] = static_cast<unsigned char>(ctx->count[0] >> (i * 8));
        bits[4 + i] = static_cast<unsigned char>(ctx->count[1] >> (i * 8));
    }

    unsigned idx = (ctx->count[0] >> 3) & 63;
    unsigned pad = (idx < 56) ? (56 - idx) : (120 - idx);
    unsigned char padding[64];
    for (unsigned i = 0; i < sizeof(padding); ++i) padding[i] = 0;
    padding[0] = 0x80;
    md5_update(ctx, padding, pad);
    md5_update(ctx, bits, 8);

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            digest[i * 4 + j] = static_cast<unsigned char>(ctx->state[i] >> (j * 8));
}

// -- Main logic --

int md5sum_fd(int fd, const char* name) {
    MD5_CTX ctx;
    md5_init(&ctx);
    unsigned char buf[kBufSize];

    for (;;) {
        auto nr = read(fd, buf, sizeof(buf));
        if (nr <= 0) break;
        md5_update(&ctx, buf, static_cast<unsigned>(nr));
    }

    unsigned char digest[16];
    md5_final(digest, &ctx);

    char hex[32];
    for (int i = 0; i < 16; ++i) {
        hex[i * 2]     = "0123456789abcdef"[digest[i] >> 4];
        hex[i * 2 + 1] = "0123456789abcdef"[digest[i] & 0x0f];
    }
    write_all(1, hex, 32);
    write_str(1, "  ");
    write_str(1, name);
    write_str(1, "\n");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        write_str(2, "usage: md5sum file ...\n");
        return 1;
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write_str(2, "md5sum: ");
            write_str(2, argv[i]);
            write_str(2, ": cannot open\n");
            status = 1;
            continue;
        }
        md5sum_fd(fd, argv[i]);
        close(fd);
    }
    return status;
}
