# nsigii

A small C11, format-agnostic, data-only container codec for arbitrary byte streams.

The C core has no filesystem dependency, no heap allocation, no payload parser, and no language-runtime dependency. It works through read/write callbacks and exposes a stable plain-C ABI for FFI. File handling and language adapters stay outside the codec.

## Relationship to the NSIGII ecosystem

This repository is the **transport layer** of a three-layer system. The `NSIGII`
prefix and the `.nsigii` extension are shared by three unrelated binary layouts,
each owned by one repository:

| Layer | Magic (bytes 0–7) | Repository | Role |
|---|---|---|---|
| **`CORE_V1`** — this repo | `NSIGII01` | `obinexus/nsigii_project` | frame arbitrary bytes, CRC-32, streaming |
| Constitutional wrapper | `NSIGII\0` + `0x07` at byte 7 | `obinexusmk2/nsigii` | SHA-256 + trident verification envelope |
| Legacy codec stream | `NSIGII\0\0` | `obinexus/nsigii_viewer` | `7.0.0` / `7.1.0A` media + state renderer |

They are **not** the same format and are never collapsed into one. `FORMAT.md`
in this repository is authoritative for `CORE_V1` only. A `CORE_V1` container may
hold any bytes — including a constitutional wrapper or a legacy stream — but this
codec neither knows nor cares: it frames bytes and checks their CRC, nothing
more. Do not add filenames, dimensions, timestamps, hashes-as-metadata,
consensus, or any application semantics to `src/nsigii.c`.

The cross-layer detection contract and the end-to-end architecture live in the
main repo: [`docs/DISPATCH.md`](https://github.com/obinexusmk2/nsigii/blob/main/docs/DISPATCH.md)
and [`docs/NSIGII-INTEGRATION.md`](https://github.com/obinexusmk2/nsigii/blob/main/docs/NSIGII-INTEGRATION.md).

## Build

```sh
make
make test
```

Outputs:

- `build/libnsigii.a` — static library
- `build/libnsigii.so` — shared library
- `build/nsigii` — native CLI

## CLI

```sh
build/nsigii pack input.dat output.nsigii
build/nsigii unpack output.nsigii restored.dat
cmp input.dat restored.dat
```

Streaming works without knowing the input size:

```sh
producer | build/nsigii pack - stream.nsigii
build/nsigii unpack stream.nsigii - | consumer
```

The `.dat` extension is not special; any bytes are accepted.

## Architecture / separation of concerns

- `include/nsigii.h` — public ABI only.
- `src/nsigii.c` — pure codec/framing/integrity; callback I/O, allocation-free.
- `src/nsigii_io.c` — `FILE *` adapter.
- `src/nsigii_cli.c` — command-line policy.
- `bindings/` — language-specific consumers of the ABI; they do not define the wire format.
- `FORMAT.md` — language-neutral wire specification.

## Foreign languages

The shared library can be called from any runtime with C FFI. Examples are included for Python `ctypes`, LuaJIT FFI, and Go `cgo`.

Node.js can link the exact same C object through the included N-API wrapper:

```sh
make node-addon
node bindings/node/native_example.mjs
```

Standard Lua can use `bindings/lua/nsigii_lua.c` (`make lua-module`, with Lua development headers installed). Browser JavaScript uses the exact C core compiled to WebAssembly with `make wasm` (Emscripten required), then `bindings/web/index.html` provides a minimal file packer UI.

The ABI intentionally uses fixed-width integers, byte pointers, lengths, integer status codes, and caller-owned buffers. No C structs need to cross the boundary for basic encode/decode calls.
