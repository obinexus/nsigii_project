#ifndef NSIGII_IO_H
#define NSIGII_IO_H

#include <stdio.h>
#include "nsigii.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional stdio adapter. The core codec in nsigii.h does not depend on FILE. */
nsigii_status nsigii_pack_FILE(FILE *in, FILE *out, nsigii_info *info);
nsigii_status nsigii_unpack_FILE(FILE *in, FILE *out, nsigii_info *info);

#ifdef __cplusplus
}
#endif

#endif
