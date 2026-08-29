# `.nsigii` v1 wire format

`.nsigii` is a format-agnostic, data-only byte container. It does not identify, inspect, transform, or interpret the payload format.
All multibyte integers are unsigned little-endian.

## Header — 32 bytes

| Offset | Size | Field | v1 value |
|---:|---:|---|---|
| 0 | 8 | magic | ASCII `NSIGII01` |
| 8 | 4 | header_size | `32` |
| 12 | 4 | flags | `0x00000003` (`chunked | crc32`) |
| 16 | 8 | declared_size | `UINT64_MAX` for stream-sized v1 |
| 24 | 4 | version_major | `1` |
| 28 | 4 | version_minor | `0` |

## Payload records

Repeated records:

1. `chunk_length`: `u32`.
2. Exactly `chunk_length` payload bytes.
3. A zero `chunk_length` terminates payload records.

The reference encoder emits chunks up to 64 KiB. A conforming v1 decoder accepts chunks up to 16 MiB and rejects larger chunks.

## Footer — 16 bytes

Immediately after the zero-length record:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | decoded payload size (`u64`) |
| 8 | 4 | IEEE CRC-32 of decoded payload |
| 12 | 4 | footer magic, ASCII `END1` |

The footer size and CRC must match the reconstructed payload. This makes corruption/truncation detectable while preserving streaming operation.

## Invariant

`decode(encode(bytes)) == bytes` for every finite byte sequence, including empty data and data containing zero bytes.
