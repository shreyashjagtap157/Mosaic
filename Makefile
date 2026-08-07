PYTHON ?= python3

.PHONY: all native test qualify release fixtures clean rust-check status

all: native

native:
	$(MAKE) -C native all

fixtures:
	$(PYTHON) tools/generate_empty_pack.py --check
	$(PYTHON) tools/build_m2_fixture.py --check
	$(PYTHON) tools/build_m3_model_fixture.py --check
	$(PYTHON) tools/build_model_v2_fixture.py --check
	$(PYTHON) tools/build_language_packs.py --check
	$(PYTHON) tools/build_detector_pack.py --check
	$(PYTHON) tools/generate_language_malformed.py --check
	$(PYTHON) tools/generate_detector_malformed.py --check
	$(PYTHON) tools/build_unicode17_pack.py --check

test: fixtures
	$(MAKE) -C native test
	$(PYTHON) tools/validate_c_reference.py
	$(PYTHON) tools/validate_c_malformed.py
	$(PYTHON) tools/validate_c_unicode.py
	$(PYTHON) tools/validate_c_unicode_malformed.py
	$(PYTHON) tools/validate_c_api.py
	$(PYTHON) tools/validate_language_packs.py
	$(PYTHON) tools/validate_detector.py
	$(PYTHON) tools/benchmark_language_packs.py
	$(PYTHON) tools/benchmark_detector.py

qualify:
	$(PYTHON) tools/qualify_native.py

release: qualify
	$(PYTHON) tools/build_release.py --no-build
	$(PYTHON) tools/validate_release_package.py

rust-check:
	$(PYTHON) tools/qualify.py

status:
	@./build/mosaic-tokenizer --version

clean:
	$(MAKE) -C native clean
	rm -rf build/clang build/analyze
