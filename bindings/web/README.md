# NSIGII01 in the browser

The C codec compiled to WebAssembly, plus a thin adapter. This is the supported
way for JavaScript to encode/decode `NSIGII01` — the C core is authoritative for
the wire format (`../../FORMAT.md`); nothing here re-implements the framing.

## Build

```sh
make wasm        # from the repo root; needs Emscripten (emcc)
```

Produces (both git-ignored — build outputs, not committed):

- `nsigii.js` — Emscripten glue, `MODULARIZE=1`, `EXPORT_NAME=NsigiiModule`
- `nsigii.wasm` — the codec

Exported: `_nsigii_encoded_bound`, `_nsigii_encode_buffer`,
`_nsigii_decode_buffer`, `_nsigii_abi_version`, `_malloc`, `_free`, `cwrap`.
Buffer API only — no streaming, no filesystem, caller owns all bytes.

## Adapter — `nsigii-adapter.mjs`

`encode(Uint8Array) -> Uint8Array` and `decode(Uint8Array) -> Uint8Array`
(throws on bad magic / version / CRC-32), plus `abiVersion()`.

### Browser

```html
<script src="nsigii.js"></script>
<script type="module">
  import { loadCore } from "./nsigii-adapter.mjs";
  const core = await loadCore();
  const payload = core.decode(new Uint8Array(await file.arrayBuffer()));
</script>
```

### Node / bundler

```js
import NsigiiModule from "./nsigii.js";
import { createCoreAdapter } from "./nsigii-adapter.mjs";
const core = createCoreAdapter(await NsigiiModule());
```

## Test

```sh
node bindings/web/nsigii-adapter.test.mjs      # after `make wasm`
```

Round-trips empty / NUL / multi-chunk payloads and checks CRC-32 tamper
detection. Runs in CI (`.github/workflows/ci.yml`).

## `index.html`

A minimal file packer/unpacker UI using the raw module (no adapter). Serve this
directory over HTTP after `make wasm`.
