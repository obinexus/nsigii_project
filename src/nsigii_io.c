#include "nsigii_io.h"

typedef struct file_ctx { FILE *f; int error; } file_ctx;

static size_t file_read(void *ctx, uint8_t *dst, size_t capacity) {
    file_ctx *c = (file_ctx *)ctx;
    size_t n = fread(dst, 1, capacity, c->f);
    if (n == 0 && ferror(c->f)) c->error = 1;
    return n;
}

static int file_write(void *ctx, const uint8_t *src, size_t size) {
    file_ctx *c = (file_ctx *)ctx;
    if (size && fwrite(src, 1, size, c->f) != size) { c->error = 1; return -1; }
    return 0;
}

nsigii_status nsigii_pack_FILE(FILE *in, FILE *out, nsigii_info *info) {
    file_ctx r = { in, 0 }, w = { out, 0 };
    nsigii_status st;
    if (!in || !out) return NSIGII_EINVAL;
    st = nsigii_encode_stream(file_read, &r, file_write, &w, NSIGII_DEFAULT_CHUNK_SIZE, info);
    if (r.error || w.error) return NSIGII_EIO;
    return st;
}

nsigii_status nsigii_unpack_FILE(FILE *in, FILE *out, nsigii_info *info) {
    file_ctx r = { in, 0 }, w = { out, 0 };
    nsigii_status st;
    if (!in || !out) return NSIGII_EINVAL;
    st = nsigii_decode_stream(file_read, &r, file_write, &w, info);
    if (r.error || w.error) return NSIGII_EIO;
    return st;
}
