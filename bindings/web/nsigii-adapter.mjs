/*
 * nsigii-adapter.mjs — a thin wrapper over the NSIGII01 C codec compiled to
 * WebAssembly (`make wasm` -> nsigii.js + nsigii.wasm).
 *
 * This is the ONLY supported way for JavaScript to encode/decode NSIGII01: the
 * C core is the source of truth for the wire format. Do not re-implement the
 * framing in JS.
 *
 *   Browser:
 *     <script src="nsigii.js"></script>        <!-- defines global NsigiiModule -->
 *     <script type="module">
 *       import { loadCore } from "./nsigii-adapter.mjs";
 *       const core = await loadCore();
 *       const payload = core.decode(containerBytes);   // Uint8Array -> Uint8Array
 *     </script>
 *
 *   Node / bundler:
 *     import NsigiiModule from "./nsigii.js";
 *     import { createCoreAdapter } from "./nsigii-adapter.mjs";
 *     const core = createCoreAdapter(await NsigiiModule());
 *
 * The exported buffer API only (from Makefile `wasm`): _nsigii_encoded_bound,
 * _nsigii_encode_buffer, _nsigii_decode_buffer, _nsigii_abi_version, _malloc,
 * _free, cwrap. No streaming, no filesystem — the caller owns all bytes.
 */

/** Stable FFI status codes from include/nsigii.h. */
export const NSIGII_STATUS = Object.freeze({
  0: "OK", 1: "EINVAL", 2: "EFORMAT", 3: "EVERSION",
  4: "ECHECKSUM", 5: "EIO", 6: "ENOSPACE", 7: "EOVERFLOW",
});

const ENCODE_CHUNK = 65536; // matches the C reference encoder

const asU8 = (x) =>
  x instanceof Uint8Array ? x : ArrayBuffer.isView(x)
    ? new Uint8Array(x.buffer, x.byteOffset, x.byteLength)
    : new Uint8Array(x);

/**
 * Wrap an already-instantiated Emscripten Module. Synchronous; does no loading.
 */
export function createCoreAdapter(Module) {
  if (!Module || typeof Module.cwrap !== "function" || typeof Module._malloc !== "function") {
    throw new Error("nsigii: expected an instantiated NsigiiModule (call NsigiiModule() and await it)");
  }
  const cBound = Module.cwrap("nsigii_encoded_bound", "number", ["number", "number", "number"]);
  const cEncode = Module.cwrap("nsigii_encode_buffer", "number", ["number", "number", "number", "number", "number"]);
  const cDecode = Module.cwrap("nsigii_decode_buffer", "number", ["number", "number", "number", "number", "number", "number"]);
  const cAbi = Module.cwrap("nsigii_abi_version", "number", []);

  // HEAPU8 must be re-read after every malloc: ALLOW_MEMORY_GROWTH can detach it.
  const heap = () => Module.HEAPU8;
  const readU32 = (ptr) => new DataView(heap().buffer, ptr, 4).getUint32(0, true);

  const fail = (verb, status) => {
    const name = NSIGII_STATUS[status] || `status ${status}`;
    return new Error(`nsigii ${verb} failed: ${name}`);
  };

  return {
    /** { major, minor } of the C ABI backing this module. */
    abiVersion() {
      const v = cAbi() >>> 0;
      return { major: v >>> 16, minor: v & 0xffff };
    },

    /** arbitrary bytes -> a NSIGII01 container (Uint8Array). */
    encode(payload) {
      const src = asU8(payload);
      const inPtr = Module._malloc(Math.max(1, src.length));
      const boundPtr = Module._malloc(4);
      try {
        heap().set(src, inPtr);
        if (cBound(src.length, ENCODE_CHUNK, boundPtr) !== 0) {
          throw new Error("nsigii encode failed: could not size the output");
        }
        const cap = readU32(boundPtr);
        const outPtr = Module._malloc(Math.max(1, cap));
        const outNPtr = Module._malloc(4);
        try {
          const st = cEncode(inPtr, src.length, outPtr, cap, outNPtr);
          if (st !== 0) throw fail("encode", st);
          return heap().slice(outPtr, outPtr + readU32(outNPtr));
        } finally {
          Module._free(outPtr);
          Module._free(outNPtr);
        }
      } finally {
        Module._free(inPtr);
        Module._free(boundPtr);
      }
    },

    /** a NSIGII01 container -> its original bytes (Uint8Array). Throws on a
     *  bad magic, version, or CRC-32 mismatch. */
    decode(container) {
      const src = asU8(container);
      // A decoded payload is always smaller than its framed container.
      const inPtr = Module._malloc(Math.max(1, src.length));
      const outPtr = Module._malloc(Math.max(1, src.length));
      const outNPtr = Module._malloc(4);
      try {
        heap().set(src, inPtr);
        const st = cDecode(inPtr, src.length, outPtr, src.length, outNPtr, 0);
        if (st !== 0) throw fail("decode", st);
        return heap().slice(outPtr, outPtr + readU32(outNPtr));
      } finally {
        Module._free(inPtr);
        Module._free(outPtr);
        Module._free(outNPtr);
      }
    },
  };
}

/**
 * Resolve an NsigiiModule factory and instantiate it, then wrap it.
 *
 * Resolution order: `opts.factory` -> `globalThis.NsigiiModule` (set by a
 * <script src="nsigii.js">) -> dynamic import of `opts.moduleUrl` (default
 * ./nsigii.js next to this file; works in Node, not in a bare browser).
 */
export async function loadCore(opts = {}) {
  let factory = opts.factory;
  if (typeof factory !== "function" && typeof globalThis !== "undefined") {
    factory = globalThis.NsigiiModule;
  }
  if (typeof factory !== "function") {
    const url = opts.moduleUrl || new URL("./nsigii.js", import.meta.url).href;
    try {
      factory = (await import(url)).default;
    } catch (err) {
      throw new Error(
        `nsigii: could not load the WASM module from ${url}. In a browser, add ` +
        `<script src="nsigii.js"></script> before importing this adapter. (${err.message})`,
      );
    }
  }
  if (typeof factory !== "function") {
    throw new Error("nsigii: no NsigiiModule factory found (run `make wasm` in obinexus/nsigii_project)");
  }
  return createCoreAdapter(await factory(opts.moduleArg || {}));
}
