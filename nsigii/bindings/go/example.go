package main

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -L../../build -lnsigii -Wl,-rpath,../../build
#include <stdlib.h>
#include "nsigii.h"
*/
import "C"
import (
    "fmt"
    "unsafe"
)

func main() {
    raw := []byte{'g','o',0,'d','a','t','a',255}
    var bound C.size_t
    if C.nsigii_encoded_bound(C.size_t(len(raw)), C.NSIGII_DEFAULT_CHUNK_SIZE, &bound) != C.NSIGII_OK { panic("bound") }
    enc := make([]byte, int(bound))
    var encN C.size_t
    if C.nsigii_encode_buffer(unsafe.Pointer(&raw[0]), C.size_t(len(raw)), unsafe.Pointer(&enc[0]), bound, &encN) != C.NSIGII_OK { panic("encode") }
    out := make([]byte, len(raw))
    var outN C.size_t
    if C.nsigii_decode_buffer(unsafe.Pointer(&enc[0]), encN, unsafe.Pointer(&out[0]), C.size_t(len(out)), &outN, nil) != C.NSIGII_OK { panic("decode") }
    fmt.Printf("go cgo roundtrip: %v\n", string(out[:int(outN)]) == string(raw))
}
