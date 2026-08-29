// Dependency-free Node example using the CLI as a process boundary.
// For in-process use, compile the same C core with `make wasm` and load the
// resulting Emscripten module, or write a tiny N-API wrapper over nsigii.h.
import { spawnSync } from 'node:child_process';
import { mkdtempSync, writeFileSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

const root = resolve(new URL('../..', import.meta.url).pathname);
const cli = join(root, 'build', 'nsigii');
const dir = mkdtempSync(join(tmpdir(), 'nsigii-'));
const input = join(dir, 'in.dat'), packed = join(dir, 'x.nsigii'), output = join(dir, 'out.dat');
const raw = Buffer.from([0, 1, 2, 255, 78, 83, 73, 71]);
writeFileSync(input, raw);
if (spawnSync(cli, ['pack', input, packed]).status !== 0) throw new Error('pack failed');
if (spawnSync(cli, ['unpack', packed, output]).status !== 0) throw new Error('unpack failed');
if (!readFileSync(output).equals(raw)) throw new Error('roundtrip mismatch');
console.log('node roundtrip: ok');
