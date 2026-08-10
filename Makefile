PYTHON ?= python3
VERSION := $(shell $(PYTHON) -c "print(open('VERSION', encoding='utf-8').read().strip())")

.PHONY: all native test qualify release release-readiness fixtures clean rust-check status

all: native

native:
	$(MAKE) -C native all

fixtures:
	$(PYTHON) tools/generate_empty_pack.py --check
	$(PYTHON) tools/build_m2_fixture.py --check
	$(PYTHON) tools/build_m3_model_fixture.py --check
	$(PYTHON) tools/build_model_v2_fixture.py --check
	$(PYTHON) tools/build_tiktoken_compat_fixture.py --check
	$(PYTHON) tools/build_language_packs.py --check
	$(PYTHON) tools/build_detector_pack.py --check
	$(PYTHON) tools/generate_language_malformed.py --check
	$(PYTHON) tools/generate_detector_malformed.py --check
	$(PYTHON) tools/build_unicode17_pack.py --check
	$(PYTHON) tools/build_security17_pack.py --check
	$(PYTHON) tools/generate_security17_malformed.py --check
	$(PYTHON) tools/build_normalization16_pack.py --check
	$(PYTHON) tools/generate_normalization16_malformed.py --check
	$(PYTHON) tools/build_online_stream_fixture.py --check
	$(PYTHON) tools/generate_trust_fixture.py --check

test: fixtures
	$(MAKE) -C native test
	$(PYTHON) tools/validate_c_reference.py
	$(PYTHON) tools/validate_c_malformed.py
	$(PYTHON) tools/validate_c_unicode.py
	$(PYTHON) tools/validate_c_unicode_malformed.py
	$(PYTHON) tools/validate_c_api.py
	$(PYTHON) tools/validate_language_packs.py
	$(PYTHON) tools/validate_detector.py
	$(PYTHON) tools/validate_authoring.py
	$(PYTHON) tools/validate_tiktoken_compat.py
	$(PYTHON) tools/validate_security17.py
	$(PYTHON) tools/validate_packed_model.py
	./build/mosaic-token-document-serialization-smoke fixtures/packs/model-v2.mpack fixtures/packs/unicode17-v1.mpack fixtures/packs/security17-v1.mpack fixtures/packs/normalization16-v1.mpack fixtures/packs/lexer/c-v1.mpack /tmp/mosaic-token-ir-valid.bin
	$(PYTHON) tools/validate_token_ir_serialization.py build/libmosaic.so /tmp/mosaic-token-ir-valid.bin
	rm -f /tmp/mosaic-token-ir-valid.bin
	PYTHONPATH=tools $(PYTHON) tools/validate_registry.py
	$(PYTHON) tools/benchmark_language_packs.py
	$(PYTHON) tools/benchmark_detector.py
	$(PYTHON) tools/benchmark_resync.py

qualify:
	$(PYTHON) tools/qualify_native.py

release: qualify
	$(PYTHON) tools/validate_release_readiness.py --build-release --archive dist/mosaic-tokenizer-$(VERSION)-linux-x86_64.tar.gz

release-readiness:
	$(PYTHON) tools/validate_release_readiness.py --skip-package

rust-check:
	$(PYTHON) tools/qualify.py

status:
	@./build/mosaic-tokenizer --version

clean:
	$(MAKE) -C native clean
	rm -rf build/clang build/analyze
