#include "nsigii_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s pack   INPUT.dat OUTPUT.nsigii\n"
        "  %s unpack INPUT.nsigii OUTPUT.dat\n"
        "  %s pack   - -       # stdin -> stdout\n"
        "  %s unpack - -       # stdin -> stdout\n",
        argv0, argv0, argv0, argv0);
}

static FILE *open_in(const char *path) { return strcmp(path, "-") == 0 ? stdin : fopen(path, "rb"); }
static FILE *open_out(const char *path) { return strcmp(path, "-") == 0 ? stdout : fopen(path, "wb"); }

int main(int argc, char **argv) {
    FILE *in, *out;
    nsigii_info info;
    nsigii_status st;
    int pack;

    if (argc != 4 || (strcmp(argv[1], "pack") != 0 && strcmp(argv[1], "unpack") != 0)) {
        usage(argv[0]); return 2;
    }
    pack = strcmp(argv[1], "pack") == 0;
    in = open_in(argv[2]);
    if (!in) { perror(argv[2]); return 1; }
    out = open_out(argv[3]);
    if (!out) { perror(argv[3]); if (in != stdin) fclose(in); return 1; }

    st = pack ? nsigii_pack_FILE(in, out, &info) : nsigii_unpack_FILE(in, out, &info);
    if (out != stdout && fclose(out) != 0 && st == NSIGII_OK) st = NSIGII_EIO;
    if (in != stdin) fclose(in);
    if (st != NSIGII_OK) {
        fprintf(stderr, "nsigii: %s\n", nsigii_status_string(st));
        if (strcmp(argv[3], "-") != 0) remove(argv[3]);
        return 1;
    }
    fprintf(stderr, "nsigii: %s %llu bytes, crc32=%08x\n",
            pack ? "packed" : "unpacked",
            (unsigned long long)info.payload_size,
            (unsigned)info.payload_crc32);
    return 0;
}
