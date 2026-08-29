#ifndef NSIGII_H
#define NSIGII_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifdef NSIGII_BUILD_DLL
#    define NSIGII_API __declspec(dllexport)
#  elif defined(NSIGII_USE_DLL)
#    define NSIGII_API __declspec(dllimport)
#  else
#    define NSIGII_API
#  endif
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define NSIGII_API __attribute__((visibility("default")))
#  else
#    define NSIGII_API
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define NSIGII_VERSION_MAJOR 1u
#define NSIGII_VERSION_MINOR 0u
#define NSIGII_HEADER_SIZE 32u
#define NSIGII_FOOTER_SIZE 16u
#define NSIGII_DEFAULT_CHUNK_SIZE (64u * 1024u)
#define NSIGII_MAX_CHUNK_SIZE (16u * 1024u * 1024u)

/* Stable status values for foreign-function interfaces. */
typedef enum nsigii_status {
    NSIGII_OK = 0,
    NSIGII_EINVAL = 1,
    NSIGII_EFORMAT = 2,
    NSIGII_EVERSION = 3,
    NSIGII_ECHECKSUM = 4,
    NSIGII_EIO = 5,
    NSIGII_ENOSPACE = 6,
    NSIGII_EOVERFLOW = 7
} nsigii_status;

typedef size_t (*nsigii_read_fn)(void *ctx, uint8_t *dst, size_t capacity);
typedef int (*nsigii_write_fn)(void *ctx, const uint8_t *src, size_t size);

typedef struct nsigii_info {
    uint32_t abi_size;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t flags;
    uint64_t payload_size;
    uint32_t payload_crc32;
    uint32_t reserved;
} nsigii_info;

/* ABI / diagnostics */
NSIGII_API uint32_t nsigii_abi_version(void);
NSIGII_API const char *nsigii_status_string(nsigii_status status);
NSIGII_API uint32_t nsigii_crc32(const void *data, size_t size);

/* Pure callback codec: no stdio, filesystem, allocator, or format knowledge. */
NSIGII_API nsigii_status nsigii_encode_stream(
    nsigii_read_fn read_fn,
    void *read_ctx,
    nsigii_write_fn write_fn,
    void *write_ctx,
    size_t chunk_size,
    nsigii_info *out_info);

NSIGII_API nsigii_status nsigii_decode_stream(
    nsigii_read_fn read_fn,
    void *read_ctx,
    nsigii_write_fn write_fn,
    void *write_ctx,
    nsigii_info *out_info);

/* Convenience buffer API. Caller owns all memory. */
NSIGII_API nsigii_status nsigii_encoded_bound(
    size_t payload_size,
    size_t chunk_size,
    size_t *out_size);

NSIGII_API nsigii_status nsigii_encode_buffer(
    const void *payload,
    size_t payload_size,
    void *encoded,
    size_t encoded_capacity,
    size_t *out_encoded_size);

NSIGII_API nsigii_status nsigii_decode_buffer(
    const void *encoded,
    size_t encoded_size,
    void *payload,
    size_t payload_capacity,
    size_t *out_payload_size,
    nsigii_info *out_info);

#ifdef __cplusplus
}
#endif

#endif
