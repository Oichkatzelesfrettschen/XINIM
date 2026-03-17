#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/* Minimal MD5 implementation (RFC 1321) */
typedef struct {
    unsigned int state[4];
    unsigned int count[2];
    unsigned char buffer[64];
} MD5_CTX;

static const unsigned int T[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
static const unsigned char S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

#define ROL(x,n) (((x)<<(n))|((x)>>(32-(n))))

static void md5_transform(unsigned int *state, const unsigned char *block) {
    unsigned int a=state[0], b=state[1], c=state[2], d=state[3];
    unsigned int M[16];
    for (int i=0; i<16; i++)
        M[i] = (unsigned int)block[i*4] | ((unsigned int)block[i*4+1]<<8) |
               ((unsigned int)block[i*4+2]<<16) | ((unsigned int)block[i*4+3]<<24);
    for (int i=0; i<64; i++) {
        unsigned int f, g;
        if (i<16)      { f=(b&c)|((~b)&d); g=(unsigned)i; }
        else if (i<32) { f=(d&b)|((~d)&c); g=(unsigned)(5*i+1)%16; }
        else if (i<48) { f=b^c^d;           g=(unsigned)(3*i+5)%16; }
        else           { f=c^(b|(~d));       g=(unsigned)(7*i)%16; }
        unsigned int tmp = d; d=c; c=b;
        b = b + ROL(a+f+T[i]+M[g], S[i]);
        a = tmp;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
}

static void md5_init(MD5_CTX *ctx) {
    ctx->state[0]=0x67452301; ctx->state[1]=0xefcdab89;
    ctx->state[2]=0x98badcfe; ctx->state[3]=0x10325476;
    ctx->count[0]=ctx->count[1]=0;
}

static void md5_update(MD5_CTX *ctx, const unsigned char *data, unsigned int len) {
    unsigned int idx = (ctx->count[0]>>3)&63;
    ctx->count[0] += len<<3;
    if (ctx->count[0] < (len<<3)) ctx->count[1]++;
    ctx->count[1] += len>>29;
    unsigned int part = 64-idx;
    unsigned int i = 0;
    if (len >= part) {
        memcpy(ctx->buffer+idx, data, part);
        md5_transform(ctx->state, ctx->buffer);
        for (i=part; i+63<len; i+=64) md5_transform(ctx->state, data+i);
        idx = 0;
    }
    memcpy(ctx->buffer+idx, data+i, len-i);
}

static void md5_final(unsigned char digest[16], MD5_CTX *ctx) {
    unsigned char bits[8];
    for (int i=0; i<4; i++) { bits[i]=(unsigned char)(ctx->count[0]>>(i*8)); bits[4+i]=(unsigned char)(ctx->count[1]>>(i*8)); }
    unsigned int idx = (ctx->count[0]>>3)&63;
    unsigned int pad = (idx<56) ? (56-idx) : (120-idx);
    unsigned char padding[64]; memset(padding,0,sizeof(padding)); padding[0]=0x80;
    md5_update(ctx, padding, pad);
    md5_update(ctx, bits, 8);
    for (int i=0; i<4; i++)
        for (int j=0; j<4; j++)
            digest[i*4+j] = (unsigned char)(ctx->state[i]>>(j*8));
}

int main(int argc, char **argv) {
    if (argc < 2) { write(2, "usage: md5sum file ...\n", 22); return 1; }
    for (int i = 1; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            write(2, "md5sum: cannot open ", 20);
            write(2, argv[i], strlen(argv[i]));
            write(2, "\n", 1);
            continue;
        }
        MD5_CTX ctx;
        md5_init(&ctx);
        unsigned char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) md5_update(&ctx, buf, (unsigned)n);
        close(fd);
        unsigned char digest[16];
        md5_final(digest, &ctx);
        for (int j = 0; j < 16; ++j) {
            char hex[2];
            hex[0] = "0123456789abcdef"[digest[j]>>4];
            hex[1] = "0123456789abcdef"[digest[j]&0xf];
            write(1, hex, 2);
        }
        write(1, "  ", 2);
        write(1, argv[i], strlen(argv[i]));
        write(1, "\n", 1);
    }
    return 0;
}
