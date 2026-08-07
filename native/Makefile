CC ?= cc
CXX ?= c++
AR ?= ar
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic -Werror
PICFLAGS ?= -fPIC
SAN_FLAGS ?= -O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer

ROOT := ..
BUILD := $(ROOT)/build
SRC := src/mosaic.c
INC := include
TESTS := $(ROOT)/conformance/c
MODEL := $(ROOT)/fixtures/packs/m3-model-v1.mpack
UNICODE := $(ROOT)/fixtures/packs/unicode17-v1.mpack

CLI := $(BUILD)/mosaic-tokenizer
OBJ := $(BUILD)/mosaic_lib.o
STATIC_LIB := $(BUILD)/libmosaic.a
SHARED_LIB := $(BUILD)/libmosaic.so
SAN_OBJ := $(BUILD)/mosaic_lib_san.o
SAN_API := $(BUILD)/mosaic-api-smoke-san
SAN_MALFORMED := $(BUILD)/mosaic-malformed-smoke-san
API_STATIC := $(BUILD)/mosaic-api-smoke-static
API_SHARED := $(BUILD)/mosaic-api-smoke-shared
API_CPP := $(BUILD)/mosaic-api-smoke-cpp

all: $(CLI) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(CLI): $(SRC) $(INC)/mosaic.h | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC) $(SRC) -o $@

$(OBJ): $(SRC) $(INC)/mosaic.h | $(BUILD)
	$(CC) $(CFLAGS) $(PICFLAGS) -I$(INC) -DMOSAIC_LIBRARY_ONLY -c $(SRC) -o $@

$(STATIC_LIB): $(OBJ)
	$(AR) rcs $@ $<

$(SHARED_LIB): $(OBJ)
	$(CC) -shared $< -o $@

$(SAN_OBJ): $(SRC) $(INC)/mosaic.h | $(BUILD)
	$(CC) $(SAN_FLAGS) -I$(INC) -DMOSAIC_LIBRARY_ONLY -c $(SRC) -o $@

$(SAN_API): $(TESTS)/api_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_MALFORMED): $(TESTS)/malformed_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

sanitize-api: $(SAN_API)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_API) $(MODEL) $(UNICODE)

sanitize-malformed: $(SAN_MALFORMED)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_MALFORMED) $(ROOT)

$(API_STATIC): $(TESTS)/api_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(API_SHARED): $(TESTS)/api_smoke.c $(SHARED_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< -L$(BUILD) -Wl,-rpath,'$$ORIGIN' -lmosaic -o $@

$(API_CPP): $(TESTS)/api_smoke.cpp $(SHARED_LIB)
	$(CXX) $(CXXFLAGS) -I$(INC) $< -L$(BUILD) -Wl,-rpath,'$$ORIGIN' -lmosaic -o $@

smoke: $(API_STATIC) $(API_SHARED)
	$(API_STATIC) $(MODEL) $(UNICODE)
	$(API_SHARED) $(MODEL) $(UNICODE)

cpp-smoke: $(API_CPP)
	$(API_CPP)

sanitize: sanitize-api sanitize-malformed

test: all smoke cpp-smoke sanitize

clean:
	rm -f $(CLI) $(OBJ) $(STATIC_LIB) $(SHARED_LIB) $(SAN_OBJ) $(SAN_API) $(SAN_MALFORMED) $(API_STATIC) $(API_SHARED) $(API_CPP)

.PHONY: all clean sanitize sanitize-api sanitize-malformed smoke cpp-smoke test
