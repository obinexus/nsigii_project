#!/usr/bin/env python3
import ctypes as C
from pathlib import Path

root = Path(__file__).resolve().parents[2]
lib = C.CDLL(str(root / "build" / "libnsigii.so"))

lib.nsigii_encoded_bound.argtypes = [C.c_size_t, C.c_size_t, C.POINTER(C.c_size_t)]
lib.nsigii_encoded_bound.restype = C.c_int
lib.nsigii_encode_buffer.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
lib.nsigii_encode_buffer.restype = C.c_int
lib.nsigii_decode_buffer.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t), C.c_void_p]
lib.nsigii_decode_buffer.restype = C.c_int

raw = b"python\x00arbitrary\xffdata"
bound = C.c_size_t()
assert lib.nsigii_encoded_bound(len(raw), 65536, C.byref(bound)) == 0
encoded = C.create_string_buffer(bound.value)
encoded_n = C.c_size_t()
assert lib.nsigii_encode_buffer(raw, len(raw), encoded, len(encoded), C.byref(encoded_n)) == 0
restored = C.create_string_buffer(len(raw))
restored_n = C.c_size_t()
assert lib.nsigii_decode_buffer(encoded, encoded_n.value, restored, len(restored), C.byref(restored_n), None) == 0
assert restored.raw[:restored_n.value] == raw
print("python ffi roundtrip: ok")
