#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include "nsigii.h"

static int l_encode(lua_State *L) {
    size_t n, bound, out_n;
    const char *src = luaL_checklstring(L, 1, &n);
    uint8_t *out;
    nsigii_status st = nsigii_encoded_bound(n, NSIGII_DEFAULT_CHUNK_SIZE, &bound);
    if (st != NSIGII_OK) return luaL_error(L, "%s", nsigii_status_string(st));
    out = (uint8_t *)malloc(bound ? bound : 1);
    if (!out) return luaL_error(L, "allocation failed");
    st = nsigii_encode_buffer(src, n, out, bound, &out_n);
    if (st != NSIGII_OK) { free(out); return luaL_error(L, "%s", nsigii_status_string(st)); }
    lua_pushlstring(L, (const char *)out, out_n);
    free(out);
    return 1;
}

static int l_decode(lua_State *L) {
    size_t n, out_n;
    const char *src = luaL_checklstring(L, 1, &n);
    uint8_t *out = (uint8_t *)malloc(n ? n : 1);
    nsigii_status st;
    if (!out) return luaL_error(L, "allocation failed");
    st = nsigii_decode_buffer(src, n, out, n, &out_n, NULL);
    if (st != NSIGII_OK) { free(out); return luaL_error(L, "%s", nsigii_status_string(st)); }
    lua_pushlstring(L, (const char *)out, out_n);
    free(out);
    return 1;
}

int luaopen_nsigii(lua_State *L) {
    static const luaL_Reg funcs[] = {
        {"encode", l_encode}, {"decode", l_decode}, {NULL, NULL}
    };
    luaL_newlib(L, funcs);
    return 1;
}
