#include "nsigii.h"

#include <limits.h>
#include <string.h>

#define NSIGII_FLAG_CHUNKED 0x00000001u
#define NSIGII_FLAG_CRC32   0x00000002u
#define NSIGII_FLAGS_V1 (NSIGII_FLAG_CHUNKED | NSIGII_FLAG_CRC32)
#define NSIGII_STREAM_SIZE UINT64_C(0xFFFFFFFFFFFFFFFF)
#define NSIGII_FOOTER_MAGIC UINT32_C(0x31444E45) /* "END1" little-endian */

static const uint8_t NSIGII_MAGIC[8] = { 'N','S','I','G','I','I','0','1' };

typedef struct crc_state { uint32_t v; } crc_state;

static uint32_t crc32_step(uint32_t crc, const uint8_t *p, size_t n) {
    size_t i;
    crc = ~crc;
    for (i = 0; i < n; ++i) {
        unsigned k;
        crc ^= p[i];
        for (k = 0; k < 8; ++k) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

static void put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_u64le(uint8_t *p, uint64_t v) {
    unsigned i;
    for (i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (i * 8));
}

static uint32_t get_u32le(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t get_u64le(const uint8_t *p) {
    uint64_t v = 0;
    unsigned i;
    for (i = 0; i < 8; ++i) v |= ((uint64_t)p[i]) << (i * 8);
    return v;
}

static int write_all(nsigii_write_fn fn, void *ctx, const uint8_t *p, size_t n) {
    return n == 0 || (fn && fn(ctx, p, n) == 0);
}

static int read_exact(nsigii_read_fn fn, void *ctx, uint8_t *p, size_t n) {
    size_t off = 0;
    if (!fn) return 0;
    while (off < n) {
        size_t got = fn(ctx, p + off, n - off);
        if (got == 0 || got > n - off) return 0;
        off += got;
    }
    return 1;
}

uint32_t nsigii_abi_version(void) {
    return (NSIGII_VERSION_MAJOR << 16) | NSIGII_VERSION_MINOR;
}

const char *nsigii_status_string(nsigii_status status) {
    switch (status) {
        case NSIGII_OK: return "ok";
        case NSIGII_EINVAL: return "invalid argument";
        case NSIGII_EFORMAT: return "invalid nsigii format";
        case NSIGII_EVERSION: return "unsupported nsigii version";
        case NSIGII_ECHECKSUM: return "payload checksum mismatch";
        case NSIGII_EIO: return "I/O callback failure or truncated input";
        case NSIGII_ENOSPACE: return "output buffer too small";
        case NSIGII_EOVERFLOW: return "size overflow";
        default: return "unknown nsigii status";
    }
}

uint32_t nsigii_crc32(const void *data, size_t size) {
    if (!data && size != 0) return 0;
    return crc32_step(0, (const uint8_t *)data, size);
}

nsigii_status nsigii_encode_stream(
    nsigii_read_fn read_fn, void *read_ctx,
    nsigii_write_fn write_fn, void *write_ctx,
    size_t chunk_size, nsigii_info *out_info) {
    uint8_t header[NSIGII_HEADER_SIZE];
    uint8_t lenbuf[4];
    uint8_t footer[NSIGII_FOOTER_SIZE];
    uint8_t buffer[NSIGII_DEFAULT_CHUNK_SIZE];
    uint8_t *chunk = buffer;
    uint64_t total = 0;
    uint32_t crc = 0;

    if (!read_fn || !write_fn) return NSIGII_EINVAL;
    if (chunk_size == 0) chunk_size = NSIGII_DEFAULT_CHUNK_SIZE;
    /* Core stays allocation-free: v1 portable stack chunk is fixed at 64 KiB. */
    if (chunk_size > NSIGII_DEFAULT_CHUNK_SIZE) return NSIGII_EINVAL;

    memset(header, 0, sizeof header);
    memcpy(header, NSIGII_MAGIC, sizeof NSIGII_MAGIC);
    put_u32le(header + 8, NSIGII_HEADER_SIZE);
    put_u32le(header + 12, NSIGII_FLAGS_V1);
    put_u64le(header + 16, NSIGII_STREAM_SIZE);
    put_u32le(header + 24, NSIGII_VERSION_MAJOR);
    put_u32le(header + 28, NSIGII_VERSION_MINOR);
    if (!write_all(write_fn, write_ctx, header, sizeof header)) return NSIGII_EIO;

    for (;;) {
        size_t got = read_fn(read_ctx, chunk, chunk_size);
        if (got > chunk_size) return NSIGII_EIO;
        if (got == 0) break;
        if (total > UINT64_MAX - (uint64_t)got) return NSIGII_EOVERFLOW;
        put_u32le(lenbuf, (uint32_t)got);
        if (!write_all(write_fn, write_ctx, lenbuf, sizeof lenbuf) ||
            !write_all(write_fn, write_ctx, chunk, got)) return NSIGII_EIO;
        crc = crc32_step(crc, chunk, got);
        total += (uint64_t)got;
    }

    put_u32le(lenbuf, 0);
    if (!write_all(write_fn, write_ctx, lenbuf, sizeof lenbuf)) return NSIGII_EIO;
    put_u64le(footer + 0, total);
    put_u32le(footer + 8, crc);
    put_u32le(footer + 12, NSIGII_FOOTER_MAGIC);
    if (!write_all(write_fn, write_ctx, footer, sizeof footer)) return NSIGII_EIO;

    if (out_info) {
        memset(out_info, 0, sizeof *out_info);
        out_info->abi_size = (uint32_t)sizeof *out_info;
        out_info->version_major = NSIGII_VERSION_MAJOR;
        out_info->version_minor = NSIGII_VERSION_MINOR;
        out_info->flags = NSIGII_FLAGS_V1;
        out_info->payload_size = total;
        out_info->payload_crc32 = crc;
    }
    return NSIGII_OK;
}

nsigii_status nsigii_decode_stream(
    nsigii_read_fn read_fn, void *read_ctx,
    nsigii_write_fn write_fn, void *write_ctx,
    nsigii_info *out_info) {
    uint8_t header[NSIGII_HEADER_SIZE];
    uint8_t lenbuf[4];
    uint8_t footer[NSIGII_FOOTER_SIZE];
    uint8_t buffer[NSIGII_DEFAULT_CHUNK_SIZE];
    uint64_t total = 0;
    uint32_t crc = 0;
    uint32_t header_size, flags, major, minor;

    if (!read_fn || !write_fn) return NSIGII_EINVAL;
    if (!read_exact(read_fn, read_ctx, header, sizeof header)) return NSIGII_EIO;
    if (memcmp(header, NSIGII_MAGIC, sizeof NSIGII_MAGIC) != 0) return NSIGII_EFORMAT;

    header_size = get_u32le(header + 8);
    flags = get_u32le(header + 12);
    major = get_u32le(header + 24);
    minor = get_u32le(header + 28);
    if (header_size != NSIGII_HEADER_SIZE) return NSIGII_EFORMAT;
    if (major != NSIGII_VERSION_MAJOR) return NSIGII_EVERSION;
    if ((flags & NSIGII_FLAGS_V1) != NSIGII_FLAGS_V1 || (flags & ~NSIGII_FLAGS_V1) != 0)
        return NSIGII_EFORMAT;
    (void)get_u64le(header + 16); /* v1 is stream-sized; footer is authoritative. */

    for (;;) {
        uint32_t chunk_len;
        size_t remaining;
        if (!read_exact(read_fn, read_ctx, lenbuf, sizeof lenbuf)) return NSIGII_EIO;
        chunk_len = get_u32le(lenbuf);
        if (chunk_len == 0) break;
        if (chunk_len > NSIGII_MAX_CHUNK_SIZE) return NSIGII_EFORMAT;
        remaining = chunk_len;
        while (remaining != 0) {
            size_t part = remaining < sizeof buffer ? remaining : sizeof buffer;
            if (!read_exact(read_fn, read_ctx, buffer, part)) return NSIGII_EIO;
            if (!write_all(write_fn, write_ctx, buffer, part)) return NSIGII_EIO;
            if (total > UINT64_MAX - (uint64_t)part) return NSIGII_EOVERFLOW;
            crc = crc32_step(crc, buffer, part);
            total += (uint64_t)part;
            remaining -= part;
        }
    }

    if (!read_exact(read_fn, read_ctx, footer, sizeof footer)) return NSIGII_EIO;
    if (get_u32le(footer + 12) != NSIGII_FOOTER_MAGIC) return NSIGII_EFORMAT;
    if (get_u64le(footer + 0) != total) return NSIGII_EFORMAT;
    if (get_u32le(footer + 8) != crc) return NSIGII_ECHECKSUM;

    if (out_info) {
        memset(out_info, 0, sizeof *out_info);
        out_info->abi_size = (uint32_t)sizeof *out_info;
        out_info->version_major = major;
        out_info->version_minor = minor;
        out_info->flags = flags;
        out_info->payload_size = total;
        out_info->payload_crc32 = crc;
    }
    return NSIGII_OK;
}

typedef struct mem_reader { const uint8_t *p; size_t n; size_t off; } mem_reader;
typedef struct mem_writer { uint8_t *p; size_t n; size_t off; int failed; } mem_writer;

static size_t mem_read(void *ctx, uint8_t *dst, size_t capacity) {
    mem_reader *r = (mem_reader *)ctx;
    size_t left = r->n - r->off;
    size_t n = left < capacity ? left : capacity;
    if (n) memcpy(dst, r->p + r->off, n);
    r->off += n;
    return n;
}

static int mem_write(void *ctx, const uint8_t *src, size_t size) {
    mem_writer *w = (mem_writer *)ctx;
    if (size > w->n - w->off) { w->failed = 1; return -1; }
    if (size) memcpy(w->p + w->off, src, size);
    w->off += size;
    return 0;
}

nsigii_status nsigii_encoded_bound(size_t payload_size, size_t chunk_size, size_t *out_size) {
    size_t chunks, overhead;
    if (!out_size) return NSIGII_EINVAL;
    if (chunk_size == 0) chunk_size = NSIGII_DEFAULT_CHUNK_SIZE;
    if (chunk_size > NSIGII_DEFAULT_CHUNK_SIZE || chunk_size > UINT32_MAX) return NSIGII_EINVAL;
    chunks = payload_size / chunk_size + ((payload_size % chunk_size) != 0);
    if (chunks > (SIZE_MAX - NSIGII_HEADER_SIZE - 4u - NSIGII_FOOTER_SIZE) / 4u)
        return NSIGII_EOVERFLOW;
    overhead = NSIGII_HEADER_SIZE + chunks * 4u + 4u + NSIGII_FOOTER_SIZE;
    if (payload_size > SIZE_MAX - overhead) return NSIGII_EOVERFLOW;
    *out_size = payload_size + overhead;
    return NSIGII_OK;
}

nsigii_status nsigii_encode_buffer(
    const void *payload, size_t payload_size,
    void *encoded, size_t encoded_capacity,
    size_t *out_encoded_size) {
    mem_reader r;
    mem_writer w;
    nsigii_status st;
    if ((!payload && payload_size) || !encoded || !out_encoded_size) return NSIGII_EINVAL;
    r.p = (const uint8_t *)payload; r.n = payload_size; r.off = 0;
    w.p = (uint8_t *)encoded; w.n = encoded_capacity; w.off = 0; w.failed = 0;
    st = nsigii_encode_stream(mem_read, &r, mem_write, &w, NSIGII_DEFAULT_CHUNK_SIZE, NULL);
    if (w.failed) return NSIGII_ENOSPACE;
    if (st == NSIGII_OK) *out_encoded_size = w.off;
    return st;
}

nsigii_status nsigii_decode_buffer(
    const void *encoded, size_t encoded_size,
    void *payload, size_t payload_capacity,
    size_t *out_payload_size, nsigii_info *out_info) {
    mem_reader r;
    mem_writer w;
    nsigii_status st;
    if ((!encoded && encoded_size) || (!payload && payload_capacity) || !out_payload_size) return NSIGII_EINVAL;
    r.p = (const uint8_t *)encoded; r.n = encoded_size; r.off = 0;
    w.p = (uint8_t *)payload; w.n = payload_capacity; w.off = 0; w.failed = 0;
    st = nsigii_decode_stream(mem_read, &r, mem_write, &w, out_info);
    if (w.failed) return NSIGII_ENOSPACE;
    if (st == NSIGII_OK) *out_payload_size = w.off;
    return st;
}
