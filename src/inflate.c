/* gzip for `vo package ENTRY NAME URL` (spec 041, research R2): the DEFLATE reader (RFC 1951: stored,
 * fixed and dynamic blocks) and the gzip wrapper (RFC 1952: header, CRC-32 and length), reading only.
 * The input is hostile: every read is checked against the end of the input, every write against the
 * end of the output, every distance against what was written, and the output against a limit.
 * Canonical Huffman codes are decoded one bit at a time by counting codes per length (the method of
 * Mark Adler's puff): slow next to a table, and a few megabytes of source archive do not notice.
 * The output buffer is mapped, doubles as it fills, and is its own window. */
#include "vo.h"

#define MAXBITS 15
#define MAXLCODES 286
#define MAXDCODES 30
#define FIXLCODES 288

typedef struct Inf {
    const uint8_t *in;
    long n, at;
    unsigned bitbuf;
    int bitcnt;
    char *out;
    long len, cap, limit;
    char *why; /* the first failure */
} Inf;

typedef struct Huff {
    short count[MAXBITS + 1];
    short symbol[FIXLCODES];
} Huff;

char *pk_map(long n)
{
    long p = vo_syscall6(SYS_mmap, 0, n, 3 /* read, write */, 0x4022 /* private, anonymous, no reserve */, -1, 0);
    return p < 0 && p > -4096 ? 0 : (char *)p;
}

void pk_unmap(char *p, long n)
{
    if (p) vo_syscall2(SYS_munmap, (long)p, n);
}

static int fail(Inf *s, char *why)
{
    if (!s->why) s->why = why;
    return -1;
}

/* need bits (0 to 16) of the input, least significant first; -1 at its end. */
static int bits(Inf *s, int need)
{
    unsigned long val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->at >= s->n) return fail(s, "the compressed data ends early");
        val |= (unsigned long)s->in[s->at++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = (unsigned)(val >> need);
    s->bitcnt -= need;
    return (int)(val & ((1UL << need) - 1));
}

/* Room for one more byte of output; 0 when the limit is reached or memory cannot be mapped. */
static int room(Inf *s, long need)
{
    if (s->len + need <= s->cap) return 1;
    if (s->len + need > s->limit) return fail(s, "it unpacks to more than 64 MiB") + 1;
    long cap = s->cap ? s->cap : 1L << 20;
    while (cap < s->len + need) cap *= 2;
    if (cap > s->limit) cap = s->limit;
    char *p = pk_map(cap);
    if (!p) return fail(s, "there is no memory to unpack it") + 1;
    mem_copy(p, s->out, s->len);
    pk_unmap(s->out, s->cap);
    s->out = p;
    s->cap = cap;
    return 1;
}

static int stored(Inf *s)
{
    s->bitbuf = 0;
    s->bitcnt = 0;
    if (s->at + 4 > s->n) return fail(s, "the compressed data ends early");
    unsigned len = s->in[s->at] | (unsigned)s->in[s->at + 1] << 8;
    unsigned nlen = s->in[s->at + 2] | (unsigned)s->in[s->at + 3] << 8;
    s->at += 4;
    if (len != (~nlen & 0xffff)) return fail(s, "the compressed data is damaged (a stored block's length)");
    if (s->at + (long)len > s->n) return fail(s, "the compressed data ends early");
    if (!room(s, len)) return -1;
    mem_copy(s->out + s->len, (char *)s->in + s->at, len);
    s->len += len;
    s->at += len;
    return 0;
}

/* The next symbol of code h, or -1. */
static int decode(Inf *s, Huff *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= MAXBITS; len++) {
        int b = bits(s, 1);
        if (b < 0) return -1;
        code |= b;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return fail(s, "the compressed data is damaged (a code that is not in its table)");
}

/* Builds the decoding table for n code lengths: 0 for a complete code, negative for an over-subscribed
 * one, positive for an incomplete one. */
static int construct(Huff *h, short *length, int n)
{
    short offs[MAXBITS + 1];
    for (int len = 0; len <= MAXBITS; len++) h->count[len] = 0;
    for (int sym = 0; sym < n; sym++) h->count[length[sym]]++;
    if (h->count[0] == n) return 0; /* no codes: complete, but decoding will fail */
    int left = 1;
    for (int len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return left;
    }
    offs[1] = 0;
    for (int len = 1; len < MAXBITS; len++) offs[len + 1] = (short)(offs[len] + h->count[len]);
    for (int sym = 0; sym < n; sym++)
        if (length[sym] != 0) h->symbol[offs[length[sym]]++] = (short)sym;
    return left;
}

static int codes(Inf *s, Huff *lencode, Huff *distcode)
{
    static const short lens[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dists[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int symbol = decode(s, lencode);
        if (symbol < 0) return -1;
        if (symbol < 256) {
            if (!room(s, 1)) return -1;
            s->out[s->len++] = (char)symbol;
            continue;
        }
        if (symbol == 256) return 0;
        symbol -= 257;
        if (symbol >= 29) return fail(s, "the compressed data is damaged (a length code)");
        int extra = bits(s, lext[symbol]);
        if (extra < 0) return -1;
        long len = lens[symbol] + extra;
        symbol = decode(s, distcode);
        if (symbol < 0) return -1;
        if (symbol >= 30) return fail(s, "the compressed data is damaged (a distance code)");
        extra = bits(s, dext[symbol]);
        if (extra < 0) return -1;
        long dist = dists[symbol] + extra;
        if (dist > s->len) return fail(s, "the compressed data is damaged (a distance before the start)");
        if (!room(s, len)) return -1;
        for (long i = 0; i < len; i++) { /* (may overlap itself: byte by byte) */
            s->out[s->len] = s->out[s->len - dist];
            s->len++;
        }
    }
}

static int fixed(Inf *s)
{
    Huff lencode, distcode;
    short lengths[FIXLCODES];
    int sym = 0;
    for (; sym < 144; sym++) lengths[sym] = 8;
    for (; sym < 256; sym++) lengths[sym] = 9;
    for (; sym < 280; sym++) lengths[sym] = 7;
    for (; sym < FIXLCODES; sym++) lengths[sym] = 8;
    construct(&lencode, lengths, FIXLCODES);
    for (sym = 0; sym < MAXDCODES; sym++) lengths[sym] = 5;
    construct(&distcode, lengths, MAXDCODES);
    return codes(s, &lencode, &distcode);
}

static int dynamic(Inf *s)
{
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[MAXLCODES + MAXDCODES];
    Huff lencode, distcode;
    int nlen = bits(s, 5), ndist = bits(s, 5), ncode = bits(s, 4);
    if (nlen < 0 || ndist < 0 || ncode < 0) return -1;
    nlen += 257;
    ndist += 1;
    ncode += 4;
    if (nlen > MAXLCODES || ndist > MAXDCODES) return fail(s, "the compressed data is damaged (too many codes)");
    int index = 0;
    for (; index < ncode; index++) {
        int b = bits(s, 3);
        if (b < 0) return -1;
        lengths[order[index]] = (short)b;
    }
    for (; index < 19; index++) lengths[order[index]] = 0;
    if (construct(&lencode, lengths, 19) != 0) return fail(s, "the compressed data is damaged (the code lengths' code)");
    index = 0;
    while (index < nlen + ndist) {
        int symbol = decode(s, &lencode);
        if (symbol < 0) return -1;
        if (symbol < 16) {
            lengths[index++] = (short)symbol;
            continue;
        }
        int len = 0, repeat;
        if (symbol == 16) {
            if (index == 0) return fail(s, "the compressed data is damaged (a repeat with nothing before it)");
            len = lengths[index - 1];
            repeat = bits(s, 2);
            if (repeat < 0) return -1;
            repeat += 3;
        } else if (symbol == 17) {
            repeat = bits(s, 3);
            if (repeat < 0) return -1;
            repeat += 3;
        } else {
            repeat = bits(s, 7);
            if (repeat < 0) return -1;
            repeat += 11;
        }
        if (index + repeat > nlen + ndist) return fail(s, "the compressed data is damaged (too many code lengths)");
        while (repeat--) lengths[index++] = (short)len;
    }
    if (lengths[256] == 0) return fail(s, "the compressed data is damaged (no end-of-block code)");
    int err = construct(&lencode, lengths, nlen);
    if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1]))
        return fail(s, "the compressed data is damaged (the literal and length code)");
    err = construct(&distcode, lengths + nlen, ndist);
    if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1]))
        return fail(s, "the compressed data is damaged (the distance code)");
    return codes(s, &lencode, &distcode);
}

/* One DEFLATE stream from s->at; 0 when its last block ended. */
static int inflate(Inf *s)
{
    s->bitbuf = 0;
    s->bitcnt = 0;
    for (;;) {
        int last = bits(s, 1), type = bits(s, 2);
        if (last < 0 || type < 0) return -1;
        int r = type == 0 ? stored(s) : type == 1 ? fixed(s) : type == 2 ? dynamic(s)
                                                                         : fail(s, "the compressed data is damaged (a block type)");
        if (r) return -1;
        if (last) return 0;
    }
}

static uint32_t crc32_of(char *p, long n)
{
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    uint32_t c = 0xffffffffu;
    for (long i = 0; i < n; i++) c = table[(c ^ (uint8_t)p[i]) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

static uint32_t le32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* 1 when in starts with the gzip magic. */
int gzip_is(char *in, long n)
{
    return n >= 2 && (uint8_t)in[0] == 0x1f && (uint8_t)in[1] == 0x8b;
}

/* Unpacks a gzip file (one or more members) of at most limit bytes of output into a mapped buffer
 * (*out, *cap bytes mapped: pk_unmap it). 0, or -1 with *why saying what is wrong. */
int gzip_unpack(char *in, long n, char **out, long *out_n, long *cap, long limit, char **why)
{
    Inf s = {(const uint8_t *)in, n, 0, 0, 0, 0, 0, 0, limit, 0};
    for (;;) {
        const uint8_t *p = s.in + s.at;
        long left = s.n - s.at;
        if (left < 18 || p[0] != 0x1f || p[1] != 0x8b) {
            fail(&s, "the gzip header is damaged");
            break;
        }
        if (p[2] != 8 || (p[3] & 0xe0)) {
            fail(&s, "the gzip file uses a method vo does not read");
            break;
        }
        int flags = p[3];
        long at = s.at + 10;
        if (flags & 4) { /* an extra field: its length, then it */
            if (at + 2 > s.n) at = s.n + 1;
            else at += 2 + (s.in[at] | (long)s.in[at + 1] << 8);
        }
        for (int f = 8; f <= 16; f <<= 1) /* a name, then a comment: each ends with a zero byte */
            if (flags & f) {
                while (at < s.n && s.in[at]) at++;
                at++;
            }
        if (flags & 2) at += 2; /* a header checksum */
        if (at >= s.n) {
            fail(&s, "the gzip header is damaged");
            break;
        }
        s.at = at;
        long start = s.len;
        if (inflate(&s)) break;
        if (s.at + 8 > s.n) {
            fail(&s, "the gzip file ends early");
            break;
        }
        if (le32(s.in + s.at) != crc32_of(s.out + start, s.len - start)) {
            fail(&s, "the gzip checksum does not match: the file is damaged");
            break;
        }
        if (le32(s.in + s.at + 4) != (uint32_t)(s.len - start)) {
            fail(&s, "the gzip length does not match: the file is damaged");
            break;
        }
        s.at += 8;
        while (s.at < s.n && !s.in[s.at]) s.at++; /* zero padding after a member */
        if (s.at >= s.n) break;
    }
    if (s.why) {
        pk_unmap(s.out, s.cap);
        *why = s.why;
        return -1;
    }
    *out = s.out;
    *out_n = s.len;
    *cap = s.cap;
    return 0;
}

/* Spec 061: one zlib stream (RFC 1950, as git packs hold) from in into out (room for cap bytes, never
 * remapped). 0 with *out_n bytes written and *used input bytes consumed (the Adler-32 included and
 * verified), or -1 with *why. */
int zlib_unpack(char *in, long n, char *out, long cap, long *out_n, long *used, char **why)
{
    Inf s = {(const uint8_t *)in, n, 2, 0, 0, out, 0, cap, cap, 0};
    const uint8_t *p = s.in;
    if (n < 6 || (p[0] & 15) != 8 || (p[0] >> 4) > 7 || (p[0] << 8 | p[1]) % 31 || (p[1] & 0x20)) {
        *why = "a compressed object's header is damaged";
        return -1;
    }
    if (inflate(&s)) {
        *why = s.why;
        return -1;
    }
    if (s.at + 4 > s.n) {
        *why = "the compressed data ends early";
        return -1;
    }
    uint32_t a = 1, b = 0;
    for (long i = 0; i < s.len;) { /* Adler-32, reduced before the sums can overflow */
        long end = i + 5552 < s.len ? i + 5552 : s.len;
        for (; i < end; i++) a += (uint8_t)out[i], b += a;
        a %= 65521, b %= 65521;
    }
    p = s.in + s.at;
    if (((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]) != (b << 16 | a)) {
        *why = "a compressed object's checksum does not match: the pack is damaged";
        return -1;
    }
    *out_n = s.len;
    *used = s.at + 4;
    return 0;
}
