#include "nsigii.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_roundtrip(const uint8_t *src, size_t n) {
    size_t bound = 0, enc_n = 0, dec_n = 0;
    uint8_t *enc, *dec;
    nsigii_info info;
    nsigii_status st;

    if (nsigii_encoded_bound(n, NSIGII_DEFAULT_CHUNK_SIZE, &bound) != NSIGII_OK) return 0;
    enc = (uint8_t *)malloc(bound ? bound : 1);
    dec = (uint8_t *)malloc(n ? n : 1);
    if (!enc || !dec) return 0;
    st = nsigii_encode_buffer(src, n, enc, bound, &enc_n);
    if (st != NSIGII_OK) return 0;
    st = nsigii_decode_buffer(enc, enc_n, dec, n, &dec_n, &info);
    if (st != NSIGII_OK || dec_n != n || info.payload_size != n ||
        (n && memcmp(src, dec, n) != 0)) return 0;

    if (enc_n > 20) {
        enc[enc_n - 8] ^= 0x01; /* corrupt checksum/footer area */
        st = nsigii_decode_buffer(enc, enc_n, dec, n, &dec_n, NULL);
        if (st == NSIGII_OK) return 0;
    }
    free(enc); free(dec); return 1;
}

int main(void) {
    uint8_t small[257];
    uint8_t *large;
    size_t i;
    for (i = 0; i < sizeof small; ++i) small[i] = (uint8_t)(i * 37u + 11u);
    large = (uint8_t *)malloc(200000);
    if (!large) return 1;
    for (i = 0; i < 200000; ++i) large[i] = (uint8_t)((i * 131u) ^ (i >> 3));

    if (!check_roundtrip(NULL, 0) ||
        !check_roundtrip(small, sizeof small) ||
        !check_roundtrip(large, 200000)) {
        fprintf(stderr, "test failed\n"); free(large); return 1;
    }
    free(large);
    puts("ok");
    return 0;
}
