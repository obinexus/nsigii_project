#include <node_api.h>
#include <stdlib.h>
#include "nsigii.h"

#define NAPI_CALL(env, call) do { if ((call) != napi_ok) return NULL; } while (0)

static napi_value status_error(napi_env env, nsigii_status st) {
    napi_value msg, err;
    NAPI_CALL(env, napi_create_string_utf8(env, nsigii_status_string(st), NAPI_AUTO_LENGTH, &msg));
    NAPI_CALL(env, napi_create_error(env, NULL, msg, &err));
    NAPI_CALL(env, napi_throw(env, err));
    return NULL;
}

static napi_value js_encode(napi_env env, napi_callback_info info) {
    size_t argc = 1, n = 0, bound = 0, out_n = 0;
    napi_value argv[1], out;
    void *src = NULL, *dst = NULL;
    nsigii_status st;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc != 1) { napi_throw_type_error(env, NULL, "encode expects one Buffer"); return NULL; }
    NAPI_CALL(env, napi_get_buffer_info(env, argv[0], &src, &n));
    st = nsigii_encoded_bound(n, NSIGII_DEFAULT_CHUNK_SIZE, &bound);
    if (st != NSIGII_OK) return status_error(env, st);
    NAPI_CALL(env, napi_create_buffer(env, bound, &dst, &out));
    st = nsigii_encode_buffer(src, n, dst, bound, &out_n);
    if (st != NSIGII_OK) return status_error(env, st);
    if (out_n != bound) { napi_throw_error(env, NULL, "internal encoded size mismatch"); return NULL; }
    return out;
}

static napi_value js_decode(napi_env env, napi_callback_info info) {
    size_t argc = 1, n = 0, out_n = 0;
    napi_value argv[1], out;
    void *src = NULL, *tmp = NULL, *copied = NULL;
    nsigii_status st;
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    if (argc != 1) { napi_throw_type_error(env, NULL, "decode expects one Buffer"); return NULL; }
    NAPI_CALL(env, napi_get_buffer_info(env, argv[0], &src, &n));
    tmp = malloc(n ? n : 1);
    if (!tmp) { napi_throw_error(env, NULL, "allocation failed"); return NULL; }
    st = nsigii_decode_buffer(src, n, tmp, n, &out_n, NULL);
    if (st != NSIGII_OK) { free(tmp); return status_error(env, st); }
    NAPI_CALL(env, napi_create_buffer_copy(env, out_n, tmp, &copied, &out));
    free(tmp);
    return out;
}

NAPI_MODULE_INIT() {
    napi_value encode, decode;
    NAPI_CALL(env, napi_create_function(env, "encode", NAPI_AUTO_LENGTH, js_encode, NULL, &encode));
    NAPI_CALL(env, napi_create_function(env, "decode", NAPI_AUTO_LENGTH, js_decode, NULL, &decode));
    NAPI_CALL(env, napi_set_named_property(env, exports, "encode", encode));
    NAPI_CALL(env, napi_set_named_property(env, exports, "decode", decode));
    return exports;
}
