CC ?= cc
CXX ?= c++
AR ?= ar
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror
CXXFLAGS ?= -O2 -std=c++11 -Wall -Wextra -Wpedantic -Werror
PICFLAGS ?= -fPIC -fvisibility=hidden
SAN_FLAGS ?= -O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer

ROOT := ..
BUILD := $(ROOT)/build
SRC := src/mosaic.c src/mosaic_cache.c src/mosaic_executor.c
INC := include
TESTS := $(ROOT)/conformance/c
MODEL := $(ROOT)/fixtures/packs/model-v2.mpack
UNICODE := $(ROOT)/fixtures/packs/unicode17-v1.mpack

CLI := $(BUILD)/mosaic-tokenizer
OBJ := $(BUILD)/mosaic_lib.o $(BUILD)/mosaic_cache.o $(BUILD)/mosaic_executor.o
STATIC_LIB := $(BUILD)/libmosaic.a
SHARED_LIB := $(BUILD)/libmosaic.so
SAN_OBJ := $(BUILD)/mosaic_lib_san.o $(BUILD)/mosaic_cache_san.o $(BUILD)/mosaic_executor_san.o
SAN_API := $(BUILD)/mosaic-api-smoke-san
SAN_MALFORMED := $(BUILD)/mosaic-malformed-smoke-san
SAN_LANGUAGE := $(BUILD)/mosaic-language-smoke-san
SAN_DETECTOR := $(BUILD)/mosaic-detector-smoke-san
SAN_BPE := $(BUILD)/mosaic-bpe-smoke-san
SAN_SECURITY := $(BUILD)/mosaic-security-smoke-san
SAN_NORMALIZATION := $(BUILD)/mosaic-normalization-smoke-san
SAN_ONLINE := $(BUILD)/mosaic-online-stream-smoke-san
SAN_INCREMENTAL := $(BUILD)/mosaic-incremental-smoke-san
SAN_RESYNC := $(BUILD)/mosaic-resync-smoke-san
SAN_TOKEN_DOCUMENT := $(BUILD)/mosaic-token-document-smoke-san
SAN_TOKEN_DOCUMENT_RICH := $(BUILD)/mosaic-token-document-rich-smoke-san
SAN_TOKEN_DOCUMENT_SERIALIZATION := $(BUILD)/mosaic-token-document-serialization-smoke-san
SAN_LEXER := $(BUILD)/mosaic-lexer-smoke-san
SAN_SEMANTIC := $(BUILD)/mosaic-semantic-subbyte-smoke-san
SAN_BLOCK_STORAGE := $(BUILD)/mosaic-block-storage-smoke-san
SAN_CACHE := $(BUILD)/mosaic-cache-smoke-san
SAN_CACHE_BACKEND := $(BUILD)/mosaic-cache-backend-smoke-san
SAN_RUNTIME_POLICY := $(BUILD)/mosaic-runtime-policy-smoke-san
SAN_LOW_MEMORY := $(BUILD)/mosaic-low-memory-smoke-san
SAN_EXECUTOR := $(BUILD)/mosaic-executor-smoke-san
SAN_RELIABILITY := $(BUILD)/mosaic-reliability-smoke-san
SAN_OBSERVABILITY := $(BUILD)/mosaic-observability-smoke-san
BPE_SMOKE := $(BUILD)/mosaic-bpe-smoke
SECURITY_SMOKE := $(BUILD)/mosaic-security-smoke
NORMALIZATION_SMOKE := $(BUILD)/mosaic-normalization-smoke
ONLINE_SMOKE := $(BUILD)/mosaic-online-stream-smoke
INCREMENTAL_SMOKE := $(BUILD)/mosaic-incremental-smoke
RESYNC_SMOKE := $(BUILD)/mosaic-resync-smoke
TOKEN_DOCUMENT_SMOKE := $(BUILD)/mosaic-token-document-smoke
TOKEN_DOCUMENT_RICH_SMOKE := $(BUILD)/mosaic-token-document-rich-smoke
TOKEN_DOCUMENT_SERIALIZATION_SMOKE := $(BUILD)/mosaic-token-document-serialization-smoke
LEXER_SMOKE := $(BUILD)/mosaic-lexer-smoke
SEMANTIC_SMOKE := $(BUILD)/mosaic-semantic-subbyte-smoke
BLOCK_STORAGE_SMOKE := $(BUILD)/mosaic-block-storage-smoke
CACHE_SMOKE := $(BUILD)/mosaic-cache-smoke
CACHE_BACKEND_SMOKE := $(BUILD)/mosaic-cache-backend-smoke
RUNTIME_POLICY_SMOKE := $(BUILD)/mosaic-runtime-policy-smoke
LOW_MEMORY_SMOKE := $(BUILD)/mosaic-low-memory-smoke
EXECUTOR_SMOKE := $(BUILD)/mosaic-executor-smoke
RELIABILITY_SMOKE := $(BUILD)/mosaic-reliability-smoke
OBSERVABILITY_SMOKE := $(BUILD)/mosaic-observability-smoke
SECURITY := $(ROOT)/fixtures/packs/security17-v1.mpack
NORMALIZATION := $(ROOT)/fixtures/packs/normalization16-v1.mpack
ICU_CFLAGS := $(shell pkg-config --cflags icu-uc)
ICU_LIBS := $(shell pkg-config --libs icu-uc)
OPENSSL_CFLAGS := $(shell pkg-config --cflags openssl)
OPENSSL_LIBS := $(shell pkg-config --libs openssl)
TRUST_OBJ := $(BUILD)/mosaic_trust.o
TRUST_STATIC_LIB := $(BUILD)/libmosaic_trust.a
TRUST_SHARED_LIB := $(BUILD)/libmosaic_trust.so
TRUST_SAN_OBJ := $(BUILD)/mosaic_trust_san.o
TRUST_SMOKE := $(BUILD)/mosaic-trust-smoke
SAN_TRUST := $(BUILD)/mosaic-trust-smoke-san
TRUST_PUB := $(ROOT)/fixtures/trust/conformance-ed25519.pub
TRUST_SIG := $(ROOT)/fixtures/packs/model-v2.mpack.sig
BPE_MODEL := $(ROOT)/fixtures/packs/raw-bpe-v1.mpack
ONLINE_ADVERSARIAL := $(ROOT)/fixtures/packs/online-adversarial-v1.mpack
API_STATIC := $(BUILD)/mosaic-api-smoke-static
API_SHARED := $(BUILD)/mosaic-api-smoke-shared
API_CPP := $(BUILD)/mosaic-api-smoke-cpp
DETECTOR := $(ROOT)/fixtures/packs/detector/reference-v1.mpack
DETECTOR_SMOKE := $(BUILD)/mosaic-detector-smoke

all: $(CLI) $(STATIC_LIB) $(SHARED_LIB) $(TRUST_STATIC_LIB) $(TRUST_SHARED_LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(CLI): $(SRC) $(INC)/mosaic.h | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC) $(SRC) -o $@

$(BUILD)/mosaic_lib.o: src/mosaic.c $(INC)/mosaic.h | $(BUILD)
	$(CC) $(CFLAGS) $(PICFLAGS) -I$(INC) -DMOSAIC_LIBRARY_ONLY -c $< -o $@

$(BUILD)/mosaic_cache.o: src/mosaic_cache.c src/mosaic_internal.h $(INC)/mosaic.h | $(BUILD)
	$(CC) $(CFLAGS) $(PICFLAGS) -I$(INC) -c $< -o $@

$(BUILD)/mosaic_executor.o: src/mosaic_executor.c $(INC)/mosaic.h | $(BUILD)
	$(CC) $(CFLAGS) $(PICFLAGS) -I$(INC) -c $< -o $@

$(STATIC_LIB): $(OBJ)
	$(AR) rcs $@ $(OBJ)

$(SHARED_LIB): $(OBJ)
	$(CC) -shared $(OBJ) -o $@

$(TRUST_OBJ): src/mosaic_trust.c $(INC)/mosaic_trust.h $(INC)/mosaic.h | $(BUILD)
	$(CC) $(CFLAGS) $(PICFLAGS) $(OPENSSL_CFLAGS) -I$(INC) -c $< -o $@

$(TRUST_STATIC_LIB): $(TRUST_OBJ)
	$(AR) rcs $@ $(TRUST_OBJ)

$(TRUST_SHARED_LIB): $(TRUST_OBJ)
	$(CC) -shared $(TRUST_OBJ) $(OPENSSL_LIBS) -o $@

$(TRUST_SAN_OBJ): src/mosaic_trust.c $(INC)/mosaic_trust.h $(INC)/mosaic.h | $(BUILD)
	$(CC) $(SAN_FLAGS) $(OPENSSL_CFLAGS) -I$(INC) -c $< -o $@

$(BUILD)/mosaic_lib_san.o: src/mosaic.c $(INC)/mosaic.h | $(BUILD)
	$(CC) $(SAN_FLAGS) -I$(INC) -DMOSAIC_LIBRARY_ONLY -c $< -o $@

$(BUILD)/mosaic_cache_san.o: src/mosaic_cache.c src/mosaic_internal.h $(INC)/mosaic.h | $(BUILD)
	$(CC) $(SAN_FLAGS) -I$(INC) -c $< -o $@

$(BUILD)/mosaic_executor_san.o: src/mosaic_executor.c $(INC)/mosaic.h | $(BUILD)
	$(CC) $(SAN_FLAGS) -I$(INC) -c $< -o $@

$(TRUST_SMOKE): $(TESTS)/trust_smoke.c $(STATIC_LIB) $(TRUST_STATIC_LIB)
	$(CC) $(CFLAGS) $(OPENSSL_CFLAGS) -I$(INC) $< $(STATIC_LIB) $(TRUST_STATIC_LIB) $(OPENSSL_LIBS) -o $@

$(SAN_TRUST): $(TESTS)/trust_smoke.c $(SAN_OBJ) $(TRUST_SAN_OBJ)
	$(CC) $(SAN_FLAGS) $(OPENSSL_CFLAGS) -I$(INC) $< $(SAN_OBJ) $(TRUST_SAN_OBJ) $(OPENSSL_LIBS) -o $@

$(SAN_API): $(TESTS)/api_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_MALFORMED): $(TESTS)/malformed_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_LANGUAGE): $(TESTS)/language_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_DETECTOR): $(TESTS)/detector_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_BPE): $(TESTS)/bpe_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_SECURITY): $(TESTS)/security_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_NORMALIZATION): $(TESTS)/normalization_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $(ICU_CFLAGS) $< $(SAN_OBJ) $(ICU_LIBS) -o $@

$(SAN_ONLINE): $(TESTS)/online_stream_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_INCREMENTAL): $(TESTS)/incremental_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_RESYNC): $(TESTS)/resync_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_TOKEN_DOCUMENT): $(TESTS)/token_document_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_TOKEN_DOCUMENT_RICH): $(TESTS)/token_document_rich_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_TOKEN_DOCUMENT_SERIALIZATION): $(TESTS)/token_document_serialization_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_LEXER): $(TESTS)/lexer_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_SEMANTIC): $(TESTS)/semantic_subbyte_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_BLOCK_STORAGE): $(TESTS)/block_storage_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -o $@

$(SAN_CACHE): $(TESTS)/cache_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -pthread -o $@

$(SAN_CACHE_BACKEND): $(TESTS)/cache_backend_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -pthread -o $@

$(SAN_RUNTIME_POLICY): $(TESTS)/runtime_policy_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -pthread -o $@

$(SAN_LOW_MEMORY): $(TESTS)/low_memory_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -pthread -o $@

$(SAN_EXECUTOR): $(TESTS)/executor_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -pthread -o $@

$(SAN_OBSERVABILITY): $(TESTS)/observability_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -pthread -o $@

$(SAN_RELIABILITY): $(TESTS)/reliability_smoke.c $(SAN_OBJ)
	$(CC) $(SAN_FLAGS) -I$(INC) $< $(SAN_OBJ) -pthread -o $@

$(BPE_SMOKE): $(TESTS)/bpe_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(SECURITY_SMOKE): $(TESTS)/security_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(NORMALIZATION_SMOKE): $(TESTS)/normalization_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $(ICU_CFLAGS) $< $(STATIC_LIB) $(ICU_LIBS) -o $@

$(ONLINE_SMOKE): $(TESTS)/online_stream_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(INCREMENTAL_SMOKE): $(TESTS)/incremental_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(RESYNC_SMOKE): $(TESTS)/resync_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(TOKEN_DOCUMENT_SMOKE): $(TESTS)/token_document_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(TOKEN_DOCUMENT_RICH_SMOKE): $(TESTS)/token_document_rich_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(TOKEN_DOCUMENT_SERIALIZATION_SMOKE): $(TESTS)/token_document_serialization_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(LEXER_SMOKE): $(TESTS)/lexer_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(SEMANTIC_SMOKE): $(TESTS)/semantic_subbyte_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(BLOCK_STORAGE_SMOKE): $(TESTS)/block_storage_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(CACHE_SMOKE): $(TESTS)/cache_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -pthread -o $@

$(CACHE_BACKEND_SMOKE): $(TESTS)/cache_backend_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -pthread -o $@

$(RUNTIME_POLICY_SMOKE): $(TESTS)/runtime_policy_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -pthread -o $@

$(LOW_MEMORY_SMOKE): $(TESTS)/low_memory_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -pthread -o $@

$(EXECUTOR_SMOKE): $(TESTS)/executor_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -pthread -o $@

$(RELIABILITY_SMOKE): $(TESTS)/reliability_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -pthread -o $@

$(OBSERVABILITY_SMOKE): $(TESTS)/observability_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -pthread -o $@

sanitize-api: $(SAN_API)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_API) $(MODEL) $(UNICODE)

sanitize-malformed: $(SAN_MALFORMED)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_MALFORMED) $(ROOT)

sanitize-language: $(SAN_LANGUAGE)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_LANGUAGE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack

sanitize-detector: $(SAN_DETECTOR)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_DETECTOR) $(MODEL) $(UNICODE) $(DETECTOR) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack

sanitize-bpe: $(SAN_BPE)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_BPE) $(BPE_MODEL)

sanitize-security: $(SAN_SECURITY)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_SECURITY) $(MODEL) $(UNICODE) $(SECURITY)

sanitize-normalization: $(SAN_NORMALIZATION)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_NORMALIZATION) $(NORMALIZATION) $(MODEL) $(UNICODE)

sanitize-online: $(SAN_ONLINE)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_ONLINE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(BPE_MODEL) $(ONLINE_ADVERSARIAL)

sanitize-incremental: $(SAN_INCREMENTAL)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_INCREMENTAL) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(BPE_MODEL)

sanitize-resync: $(SAN_RESYNC)
	MOSAIC_RESYNC_EDITS=$${MOSAIC_RESYNC_EDITS:-100} ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_RESYNC) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(BPE_MODEL)

sanitize-token-document: $(SAN_TOKEN_DOCUMENT) $(SAN_TOKEN_DOCUMENT_RICH) $(SAN_TOKEN_DOCUMENT_SERIALIZATION) $(SAN_LEXER) $(SAN_SEMANTIC) $(SAN_BLOCK_STORAGE) $(SAN_CACHE) $(SAN_CACHE_BACKEND) $(SAN_RUNTIME_POLICY) $(SAN_LOW_MEMORY) $(SAN_EXECUTOR) $(SAN_OBSERVABILITY) $(SAN_RELIABILITY)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_TOKEN_DOCUMENT) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(DETECTOR)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_TOKEN_DOCUMENT_RICH) $(MODEL) $(UNICODE) $(SECURITY) $(NORMALIZATION)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_TOKEN_DOCUMENT_SERIALIZATION) $(MODEL) $(UNICODE) $(SECURITY) $(NORMALIZATION) $(ROOT)/fixtures/packs/lexer/c-v1.mpack
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_LEXER) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/lexer/c-v1.mpack $(ROOT)/fixtures/packs/lexer/python-v1.mpack $(ROOT)/fixtures/packs/lexer/rust-v1.mpack $(ROOT)/fixtures/packs/lexer/json-v1.mpack
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_SEMANTIC) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/lexer/c-v1.mpack
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_BLOCK_STORAGE) $(MODEL) $(UNICODE)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_CACHE)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_CACHE_BACKEND)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_RUNTIME_POLICY) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_LOW_MEMORY) $(MODEL) $(UNICODE)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_EXECUTOR) $(MODEL) $(UNICODE)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_OBSERVABILITY) $(MODEL) $(UNICODE) $(DETECTOR) $(ROOT)/fixtures/packs/language/en-v1.mpack $(SECURITY) $(NORMALIZATION) $(ROOT)/fixtures/packs/lexer/c-v1.mpack
	MOSAIC_RELIABILITY_ITERS=500 ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_RELIABILITY) $(MODEL) $(UNICODE)

$(API_STATIC): $(TESTS)/api_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

$(API_SHARED): $(TESTS)/api_smoke.c $(SHARED_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< -L$(BUILD) -Wl,-rpath,'$$ORIGIN' -lmosaic -o $@

$(API_CPP): $(TESTS)/api_smoke.cpp $(SHARED_LIB)
	$(CXX) $(CXXFLAGS) -I$(INC) $< -L$(BUILD) -Wl,-rpath,'$$ORIGIN' -lmosaic -o $@

$(DETECTOR_SMOKE): $(TESTS)/detector_smoke.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INC) $< $(STATIC_LIB) -o $@

sanitize-trust: $(SAN_TRUST)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_TRUST) $(MODEL) $(TRUST_PUB) $(TRUST_SIG)

sanitize-low-memory: $(SAN_LOW_MEMORY)
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(SAN_LOW_MEMORY) $(MODEL) $(UNICODE)

smoke: $(API_STATIC) $(API_SHARED)
	$(API_STATIC) $(MODEL) $(UNICODE)
	$(API_SHARED) $(MODEL) $(UNICODE)

cpp-smoke: $(API_CPP)
	$(API_CPP)

sanitize: sanitize-api sanitize-malformed sanitize-language sanitize-detector sanitize-bpe sanitize-security sanitize-normalization sanitize-online sanitize-incremental sanitize-resync sanitize-token-document sanitize-token-document-serialization sanitize-trust sanitize-low-memory

test: $(TOKEN_DOCUMENT_SERIALIZATION_SMOKE) all smoke cpp-smoke $(DETECTOR_SMOKE) $(BPE_SMOKE) $(SECURITY_SMOKE) $(NORMALIZATION_SMOKE) $(ONLINE_SMOKE) $(INCREMENTAL_SMOKE) $(RESYNC_SMOKE) $(TOKEN_DOCUMENT_SMOKE) $(TOKEN_DOCUMENT_RICH_SMOKE) $(TOKEN_DOCUMENT_SERIALIZATION_SMOKE) $(LEXER_SMOKE) $(SEMANTIC_SMOKE) $(BLOCK_STORAGE_SMOKE) $(CACHE_SMOKE) $(CACHE_BACKEND_SMOKE) $(RUNTIME_POLICY_SMOKE) $(EXECUTOR_SMOKE) $(OBSERVABILITY_SMOKE) $(RELIABILITY_SMOKE) $(TRUST_SMOKE) $(RESYNC_SMOKE) sanitize
	$(DETECTOR_SMOKE) $(MODEL) $(UNICODE) $(DETECTOR) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack
	$(BPE_SMOKE) $(BPE_MODEL)
	$(SECURITY_SMOKE) $(MODEL) $(UNICODE) $(SECURITY)
	$(NORMALIZATION_SMOKE) $(NORMALIZATION) $(MODEL) $(UNICODE)
	$(ONLINE_SMOKE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(BPE_MODEL) $(ONLINE_ADVERSARIAL)
	$(INCREMENTAL_SMOKE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(BPE_MODEL)
	$(RESYNC_SMOKE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(BPE_MODEL)
	$(TOKEN_DOCUMENT_SMOKE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack $(ROOT)/fixtures/packs/language/hi-v1.mpack $(ROOT)/fixtures/packs/language/ja-v1.mpack $(DETECTOR)
	$(TOKEN_DOCUMENT_RICH_SMOKE) $(MODEL) $(UNICODE) $(SECURITY) $(NORMALIZATION)
	$(TOKEN_DOCUMENT_SERIALIZATION_SMOKE) $(MODEL) $(UNICODE) $(SECURITY) $(NORMALIZATION) $(ROOT)/fixtures/packs/lexer/c-v1.mpack
	$(LEXER_SMOKE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/lexer/c-v1.mpack $(ROOT)/fixtures/packs/lexer/python-v1.mpack $(ROOT)/fixtures/packs/lexer/rust-v1.mpack $(ROOT)/fixtures/packs/lexer/json-v1.mpack
	$(SEMANTIC_SMOKE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/lexer/c-v1.mpack
	$(BLOCK_STORAGE_SMOKE) $(MODEL) $(UNICODE)
	$(CACHE_SMOKE)
	$(CACHE_BACKEND_SMOKE)
	$(RUNTIME_POLICY_SMOKE) $(MODEL) $(UNICODE) $(ROOT)/fixtures/packs/language/en-v1.mpack
	$(LOW_MEMORY_SMOKE) $(MODEL) $(UNICODE)
	$(EXECUTOR_SMOKE) $(MODEL) $(UNICODE)
	MOSAIC_RELIABILITY_ITERS=1000 $(RELIABILITY_SMOKE) $(MODEL) $(UNICODE)
	$(OBSERVABILITY_SMOKE) $(MODEL) $(UNICODE) $(DETECTOR) $(ROOT)/fixtures/packs/language/en-v1.mpack $(SECURITY) $(NORMALIZATION) $(ROOT)/fixtures/packs/lexer/c-v1.mpack
	$(TRUST_SMOKE) $(MODEL) $(TRUST_PUB) $(TRUST_SIG)

clean:
	rm -f $(CLI) $(OBJ) $(STATIC_LIB) $(SHARED_LIB) $(SAN_OBJ) $(SAN_API) $(SAN_MALFORMED) $(SAN_LANGUAGE) $(SAN_DETECTOR) $(SAN_BPE) $(SAN_SECURITY) $(SAN_NORMALIZATION) $(SAN_ONLINE) $(SAN_INCREMENTAL) $(SAN_RESYNC) $(SAN_TOKEN_DOCUMENT) $(SAN_TOKEN_DOCUMENT_RICH) $(SAN_TOKEN_DOCUMENT_SERIALIZATION) $(SAN_LEXER) $(SAN_SEMANTIC) $(SAN_BLOCK_STORAGE) $(SAN_CACHE) $(SAN_CACHE_BACKEND) $(SAN_RUNTIME_POLICY) $(SAN_LOW_MEMORY) $(SAN_EXECUTOR) $(SAN_OBSERVABILITY) $(SAN_RELIABILITY) $(BPE_SMOKE) $(SECURITY_SMOKE) $(NORMALIZATION_SMOKE) $(ONLINE_SMOKE) $(INCREMENTAL_SMOKE) $(RESYNC_SMOKE) $(TOKEN_DOCUMENT_SMOKE) $(TOKEN_DOCUMENT_RICH_SMOKE) $(TOKEN_DOCUMENT_SERIALIZATION_SMOKE) $(LEXER_SMOKE) $(SEMANTIC_SMOKE) $(BLOCK_STORAGE_SMOKE) $(CACHE_SMOKE) $(CACHE_BACKEND_SMOKE) $(RUNTIME_POLICY_SMOKE) $(LOW_MEMORY_SMOKE) $(EXECUTOR_SMOKE) $(RELIABILITY_SMOKE) $(API_STATIC) $(API_SHARED) $(API_CPP) $(DETECTOR_SMOKE) $(TRUST_OBJ) $(TRUST_STATIC_LIB) $(TRUST_SHARED_LIB) $(TRUST_SAN_OBJ) $(TRUST_SMOKE) $(SAN_TRUST)

.PHONY: all clean sanitize sanitize-api sanitize-malformed sanitize-language sanitize-detector sanitize-bpe sanitize-security sanitize-normalization sanitize-online sanitize-incremental sanitize-resync sanitize-token-document sanitize-token-document-serialization sanitize-trust sanitize-low-memory smoke cpp-smoke test
