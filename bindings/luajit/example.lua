local ffi = require("ffi")
ffi.cdef[[
int nsigii_encoded_bound(size_t payload_size, size_t chunk_size, size_t *out_size);
int nsigii_encode_buffer(const void *payload, size_t payload_size, void *encoded, size_t encoded_capacity, size_t *out_encoded_size);
int nsigii_decode_buffer(const void *encoded, size_t encoded_size, void *payload, size_t payload_capacity, size_t *out_payload_size, void *out_info);
]]
local lib = ffi.load("./build/libnsigii.so")
local raw = "lua\0arbitrary-data"
local bound = ffi.new("size_t[1]")
assert(lib.nsigii_encoded_bound(#raw, 65536, bound) == 0)
local enc = ffi.new("uint8_t[?]", tonumber(bound[0]))
local encn = ffi.new("size_t[1]")
assert(lib.nsigii_encode_buffer(raw, #raw, enc, tonumber(bound[0]), encn) == 0)
local out = ffi.new("uint8_t[?]", #raw)
local outn = ffi.new("size_t[1]")
assert(lib.nsigii_decode_buffer(enc, encn[0], out, #raw, outn, nil) == 0)
assert(ffi.string(out, tonumber(outn[0])) == raw)
print("luajit ffi roundtrip: ok")
