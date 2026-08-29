CC ?= cc
AR ?= ar
CFLAGS ?= -O2
CPPFLAGS ?=
LDFLAGS ?=

WARN := -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
STD := -std=c11
PIC := -fPIC
INC := -Iinclude
BUILD := build
LIB_STATIC := $(BUILD)/libnsigii.a
LIB_SHARED := $(BUILD)/libnsigii.so
CLI := $(BUILD)/nsigii
TEST := $(BUILD)/test_nsigii

CORE_OBJ := $(BUILD)/nsigii.o
IO_OBJ := $(BUILD)/nsigii_io.o

.PHONY: all clean test check shared static cli wasm node-addon lua-module examples
all: static shared cli

$(BUILD):
	mkdir -p $(BUILD)

$(CORE_OBJ): src/nsigii.c include/nsigii.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STD) $(PIC) $(INC) -c $< -o $@

$(IO_OBJ): src/nsigii_io.c include/nsigii.h include/nsigii_io.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STD) $(PIC) $(INC) -c $< -o $@

static: $(LIB_STATIC)
$(LIB_STATIC): $(CORE_OBJ)
	$(AR) rcs $@ $^

shared: $(LIB_SHARED)
$(LIB_SHARED): $(CORE_OBJ)
	$(CC) -shared $(LDFLAGS) $^ -o $@

cli: $(CLI)
$(CLI): src/nsigii_cli.c $(CORE_OBJ) $(IO_OBJ) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STD) $(INC) $^ $(LDFLAGS) -o $@

$(TEST): tests/test_nsigii.c $(CORE_OBJ) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STD) $(INC) $^ $(LDFLAGS) -o $@

test check: $(TEST) $(CLI)
	$(TEST)
	@printf 'raw\000bytes\377test\n' > $(BUILD)/sample.dat
	@$(CLI) pack $(BUILD)/sample.dat $(BUILD)/sample.nsigii >/dev/null
	@$(CLI) unpack $(BUILD)/sample.nsigii $(BUILD)/sample.out >/dev/null
	@cmp $(BUILD)/sample.dat $(BUILD)/sample.out
	@cat $(BUILD)/sample.dat | $(CLI) pack - - 2>/dev/null | $(CLI) unpack - - 2>/dev/null | cmp - $(BUILD)/sample.dat
	@echo "cli stream roundtrip: ok"


NODE ?= node
NODE_INCLUDE ?= $(shell $(NODE) -p "require('path').join(require('path').dirname(process.execPath),'..','include','node')")
NODE_ADDON := bindings/node/nsigii.node

node-addon: $(NODE_ADDON)
$(NODE_ADDON): bindings/node/nsigii_napi.c $(CORE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STD) $(PIC) $(INC) -I$(NODE_INCLUDE) -shared $^ -o $@

# Optional standard Lua module. Override LUA_PKG if your pkg-config name differs.
LUA_PKG ?= lua5.4
lua-module: $(CORE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(STD) $(PIC) $(INC) $$(pkg-config --cflags $(LUA_PKG)) \
	  -shared bindings/lua/nsigii_lua.c $(CORE_OBJ) $$(pkg-config --libs $(LUA_PKG)) -o bindings/lua/nsigii.so

# Optional browser/Node WebAssembly build. Requires Emscripten (emcc).
wasm: | $(BUILD)
	emcc -O2 -std=c11 -Iinclude src/nsigii.c \
	  -sMODULARIZE=1 -sEXPORT_NAME=NsigiiModule \
	  -sEXPORTED_FUNCTIONS='["_malloc","_free","_nsigii_abi_version","_nsigii_encoded_bound","_nsigii_encode_buffer","_nsigii_decode_buffer"]' \
	  -sALLOW_MEMORY_GROWTH=1 \
	  -sEXPORTED_RUNTIME_METHODS='["cwrap"]' \
	  -o bindings/web/nsigii.js

clean:
	rm -rf $(BUILD) bindings/web/nsigii.js bindings/web/nsigii.wasm bindings/node/nsigii.node bindings/lua/nsigii.so
