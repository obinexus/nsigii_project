import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const nsigii = require('./nsigii.node');

const raw = Buffer.from([0, 1, 2, 3, 255, 78, 83, 73, 71, 73, 73]);
const encoded = nsigii.encode(raw);
const decoded = nsigii.decode(encoded);
if (!decoded.equals(raw)) throw new Error('native addon roundtrip mismatch');
console.log(`node N-API roundtrip: ok (${raw.length} -> ${encoded.length} -> ${decoded.length})`);
