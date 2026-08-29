/*
 * Self-test for nsigii-adapter.mjs against the real WASM build.
 * Run after `make wasm`:  node bindings/web/nsigii-adapter.test.mjs
 */
import NsigiiModule from "./nsigii.js";
import { createCoreAdapter } from "./nsigii-adapter.mjs";

const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const fail = (msg) => { console.error("FAIL: " + msg); process.exit(1); };

const core = createCoreAdapter(await NsigiiModule());

const abi = core.abiVersion();
if (abi.major !== 1 || abi.minor !== 0) fail(`abiVersion ${abi.major}.${abi.minor}, expected 1.0`);

const vectors = [
  new Uint8Array(0),
  new Uint8Array([0]),
  new Uint8Array([0, 1, 2, 255, 0, 78, 83]),
  new TextEncoder().encode("hello\0world with an embedded NUL"),
  Uint8Array.from({ length: 200000 }, (_, i) => (i * 131) ^ (i >> 3)), // > 64 KiB: multi-chunk
];

for (const [i, raw] of vectors.entries()) {
  const enc = core.encode(raw);
  if (String.fromCharCode(...enc.slice(0, 8)) !== "NSIGII01") fail(`vector ${i}: bad container magic`);
  const dec = core.decode(enc);
  if (!eq(dec, raw)) fail(`vector ${i}: decode(encode(x)) !== x (${raw.length} bytes)`);
}

// tamper detection: flip a byte in the CRC-32 field (footer is the last 16 bytes)
const enc = core.encode(new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]));
const bad = enc.slice();
bad[bad.length - 8] ^= 0xff;
let threw = false;
try { core.decode(bad); } catch { threw = true; }
if (!threw) fail("decode accepted a container with a corrupted CRC-32");

console.log("wasm adapter roundtrip: ok");
