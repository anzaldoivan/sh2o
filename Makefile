# Thin driver for the CMake build. Copyright (c) 2026 sh2o Contributors. MIT License.
# Each compiler and each variant gets its own folder in build/, so CC=gcc and
# CC=clang do not mix. The GitHub workflows call these targets.
PYTHON ?= python3
OUT := build/$(notdir $(CC))
# The C++ compiler that matches CC (gcc-13 -> g++-13, clang -> clang++), for the C++ consumer.
CXX_FOR_CC = $(if $(findstring clang,$(notdir $(CC))),$(subst clang,clang++,$(CC)),$(subst gcc,g++,$(CC)))
# SH2O_ZAM=ON: make all, test, proofs and proof-tests build the host library
# with the experimental SH2ZAM delegate in its own folder.
SH2O_ZAM ?= OFF
ifeq ($(SH2O_ZAM),ON)
BUILD := $(OUT)-zam
ZAM_FLAGS := -DSH2O_ZAM=ON
else
BUILD := $(OUT)
ZAM_FLAGS :=
endif
# SH2O_COUNT=ON: the same targets with the call counters (sh2o/count.h), the folder
# gets -count (also after -zam); make cross SH2O_COUNT=ON builds the counted SH-2 library.
SH2O_COUNT ?= OFF
ifeq ($(SH2O_COUNT),ON)
BUILD := $(BUILD)-count
ZAM_FLAGS += -DSH2O_COUNT=ON
endif
# The SH-2 image is the Yaul image that SH2ZAM's CI also pins. Set SH2_IMAGE to
# use another image that has sh2eb-elf-gcc.
SH2_IMAGE ?= ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6
# The pinned Yabause source of make sh2-model, outside build/ (CI keeps it in a cache).
YABAUSE_SOURCE ?= .cache/yabause

# $(call variant,<folder>,<cmake options>[,<environment>]): configure, build and run the
# tests (not the long proofs: make proofs runs them); <environment> goes before ctest.
define variant
	cmake -S . -B $(1) -DCMAKE_C_COMPILER=$(CC) $(2)
	cmake --build $(1) --parallel
	$(3) ctest --test-dir $(1) --output-on-failure -LE proof
endef

.PHONY: all test proofs proof-tests check style public-check pin-check oracles oracles-check sh2-manifest sanitize strict portable \
	install-check cross sh2-model sh2zam-tests psyq47 ci clean

all:
	cmake -S . -B $(BUILD) -DCMAKE_C_COMPILER=$(CC) -DCMAKE_BUILD_TYPE=Release $(ZAM_FLAGS)
	cmake --build $(BUILD) --parallel

# The tests without the label proof. make proofs runs all tests, the long
# proofs too.
test: all
	ctest --test-dir $(BUILD) --output-on-failure -LE proof

proofs: all
	ctest --test-dir $(BUILD) --output-on-failure

# The long proofs only (label proof).
proof-tests: all
	ctest --test-dir $(BUILD) --output-on-failure -L proof

# Fast checks that need no compiler.
check: style public-check pin-check oracles-check

style:
	$(PYTHON) scripts/style_check.py --self-test
	$(PYTHON) scripts/style_check.py

# Only files and text that may be public. TREE: a directory or a revision
# (default: HEAD). The self-test first proves that each rule still fires.
public-check:
	$(PYTHON) scripts/check_ci.py --self-test
	$(PYTHON) scripts/check_ci.py
	$(PYTHON) scripts/check_public.py --self-test
	$(PYTHON) scripts/check_public.py $(TREE)

pin-check:
	$(PYTHON) scripts/check_pin.py --self-test
	$(PYTHON) scripts/check_pin.py

# Writes each tests/vectors/<family>.vec again when scripts/vectors_<family>.py
# exists, then each tests/<family>_oracle.h from its vector file.
oracles:
	@for vec in tests/vectors/*.vec; do \
	    family=$$(basename $$vec .vec); \
	    if [ -f scripts/vectors_$$family.py ]; then $(PYTHON) scripts/vectors_$$family.py $$vec || exit 1; fi; \
	    $(PYTHON) scripts/oracle_header.py $$vec tests/$${family}_oracle.h || exit 1; \
	    echo "oracles: $$family"; \
	done

# Writes tests/sh2/objects.sha256 again from the SH-2 build (needs the SH2ZAM checkout).
sh2-manifest:
	$(sh2_build)
	bash scripts/sh2_manifest.sh write $(SH2_IMAGE)

# The generated files must equal the files in git.
oracles-check: oracles
	git diff --exit-code -- tests/vectors tests/*_oracle.h
	@test -z "$$(git status --porcelain -- tests/vectors tests/*_oracle.h)" || \
	    { echo "oracles-check: generated files are not in git"; git status --short -- tests; exit 1; }

# ASan, UBSan and LeakSanitizer (on with ASan on Linux). Any report stops the test.
sanitize:
	$(call variant,$(OUT)-sanitize,-DCMAKE_BUILD_TYPE=Debug -DSH2O_SANITIZE=ON -DSH2O_STRICT=ON,\
	    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=detect_leaks=1)

strict:
	$(call variant,$(OUT)-strict,-DCMAKE_BUILD_TYPE=Release -DSH2O_STRICT=ON)

portable:
	$(call variant,$(OUT)-portable,-DCMAKE_BUILD_TYPE=Release -DSH2O_FORCE_PORTABLE=ON -DSH2O_STRICT=ON)

# Installs the library and builds an example against the installed package.
# The package holds nothing of the SH2ZAM delegate: not its header, no sh2zam
# header or text, no sh2z or sh2o_zam symbol in libsh2o.a; and nothing of
# psyz (no psyz path; no psyz text but in NOTICE.md, the licence notice,
# which must equal this tree's NOTICE.md).
# INSTALL_PLANT=psyz-path, psyz-text or notice-text puts one
# planted file in the prefix before the checks; the target must then fail.
install-check:
	cmake -S . -B $(OUT)-install -DCMAKE_C_COMPILER=$(CC) -DCMAKE_BUILD_TYPE=Release \
	    -DSH2O_BUILD_TESTS=OFF -DSH2O_BUILD_EXAMPLES=OFF
	cmake --build $(OUT)-install --parallel
	cmake --install $(OUT)-install --prefix $(CURDIR)/$(OUT)-install/prefix
	@prefix=$(CURDIR)/$(OUT)-install/prefix; \
	rm -f $$prefix/share/PsyZ-planted.txt $$prefix/share/planted.txt; \
	case "$(INSTALL_PLANT)" in \
	    psyz-path) mkdir -p $$prefix/share && echo planted > $$prefix/share/PsyZ-planted.txt ;; \
	    psyz-text) mkdir -p $$prefix/share && echo "planted PSYZ text" > $$prefix/share/planted.txt ;; \
	    notice-text) echo "planted psyz text" >> $$prefix/share/doc/sh2o/NOTICE.md ;; \
	    "") ;; \
	    *) echo "install-check: INSTALL_PLANT must be psyz-path, psyz-text or notice-text"; exit 1 ;; \
	esac; \
	if [ -e $$prefix/include/sh2o/zam_experimental.h ]; then \
	    echo "install-check: the prefix holds include/sh2o/zam_experimental.h"; exit 1; fi; \
	if find $$prefix -path '*sh2zam*' | grep .; then echo "install-check: the prefix holds an sh2zam path"; exit 1; fi; \
	if grep -rIl sh2zam $$prefix; then echo "install-check: sh2zam text under the prefix"; exit 1; fi; \
	if find $$prefix -ipath '*psyz*' | grep .; then echo "install-check: the prefix holds a psyz path"; exit 1; fi; \
	if grep -rIli --exclude=NOTICE.md psyz $$prefix; then echo "install-check: psyz text under the prefix"; exit 1; fi; \
	for notice in $$(find $$prefix -name NOTICE.md); do cmp -s $$notice NOTICE.md || \
	    { echo "install-check: $$notice is not the notice of this tree"; exit 1; }; done; \
	lib=$$(find $$prefix -name libsh2o.a); test -n "$$lib" || { echo "install-check: no libsh2o.a"; exit 1; }; \
	if nm $$lib | awk 'NF >= 2 {print $$NF}' | grep -E 'sh2z|sh2o_zam'; then \
	    echo "install-check: libsh2o.a holds an SH2ZAM symbol"; exit 1; fi; \
	echo "install-check: no SH2ZAM delegate and no psyz in the package"
	cmake -S tests/consumer -B $(OUT)-install/consumer -DCMAKE_C_COMPILER=$(CC) \
	    -DCMAKE_CXX_COMPILER=$(CXX_FOR_CC) -DCMAKE_PREFIX_PATH=$(CURDIR)/$(OUT)-install/prefix
	cmake --build $(OUT)-install/consumer
	$(OUT)-install/consumer/sh2o_consumer
	$(OUT)-install/consumer/sh2o_cxx_consumer

# SH-2 cross build in the Yaul image, with no network: the library, the suites and, when SH2ZAM's
# model.ld is checked out, the model ELFs of both lanes (OFF and SH2O_ZAM=ON).
define sh2_build
	mkdir -p build
	docker run --rm --network none --user $$(id -u):$$(id -g) -v $(CURDIR):/work -w /work \
	    $(SH2_IMAGE) sh -c 'sh scripts/sh2_cross.sh && \
	    if [ -f external/SH2ZAM/saturn/benchmarks/integer-model/model.ld ]; then \
	    sh scripts/sh2_model.sh elf && SH2O_ZAM=ON sh scripts/sh2_model.sh elf; fi'
endef

# The sha256 of every built object, library and ELF must equal tests/sh2/objects.sha256.
# SH2O_COUNT=ON: only the counted library (build/sh2-count-*), no lane ELF and no manifest check.
cross:
ifeq ($(SH2O_COUNT),ON)
	mkdir -p build
	docker run --rm --network none --user $$(id -u):$$(id -g) -v $(CURDIR):/work -w /work \
	    -e SH2O_COUNT=ON $(SH2_IMAGE) sh scripts/sh2_cross.sh
	@echo "cross: SH2O_COUNT=ON: no lane ELF and no manifest check (the manifest pins the OFF objects)"
else
	$(sh2_build)
	bash scripts/sh2_manifest.sh check $(SH2_IMAGE)
endif

# SH-2 model run of each family (scripts/sh2_model.sh finds them): ELFs built in the Yaul image with no
# network, then the GPL harness on the host (it gets the pinned Yabause source).
# SH2O_ZAM=ON: every family with the experimental SH2ZAM delegate, in build/sh2-model-zam.
sh2-model:
	mkdir -p build
	docker run --rm --network none --user $$(id -u):$$(id -g) -v $(CURDIR):/work -w /work \
	    -e SH2_IMAGE=$(SH2_IMAGE) -e SH2O_ZAM=$(SH2O_ZAM) $(SH2_IMAGE) sh scripts/sh2_model.sh elf
	SH2_IMAGE=$(SH2_IMAGE) SH2O_ZAM=$(SH2O_ZAM) YABAUSE_SOURCE=$(YABAUSE_SOURCE) bash scripts/sh2_model.sh run

# SH2ZAM's own tests, built from the pinned submodule without a change.
sh2zam-tests:
	cmake -S external/SH2ZAM -B $(OUT)-sh2zam -DCMAKE_C_COMPILER=$(CC) -DCMAKE_BUILD_TYPE=Release \
	    -DSH2Z_BUILD_TESTS=ON -DSH2Z_BUILD_EXAMPLES=ON
	cmake --build $(OUT)-sh2zam --parallel
	ctest --test-dir $(OUT)-sh2zam --output-on-failure

# The PsyQ 4.7 archive against the game's libgte (scripts/psyq47_job.py): fetch the
# archive into build/psyq47 (a failed fetch prints the skip line and ends the target
# with exit code 0), build both variants, run the tests with the label psyq47 in each
# with the archive required, then report. The CI job sh2o psyq47 runs it.
PSYQ47_ZIP := $(CURDIR)/build/psyq47/Psy-Q_47.zip
psyq47:
	rc=0; $(PYTHON) scripts/psyq47_job.py fetch || rc=$$?; \
	if [ $$rc -eq 77 ]; then exit 0; elif [ $$rc -ne 0 ]; then exit $$rc; fi; \
	$(MAKE) all && $(MAKE) all SH2O_ZAM=ON || exit 1; \
	fail=0; \
	for b in $(OUT) $(OUT)-zam; do \
	    rm -f $(CURDIR)/$$b/psyq47-junit.xml; \
	    SH2O_PSYQ47_ARCHIVE=$(PSYQ47_ZIP) SH2O_REQUIRE_PSYQ47=1 ctest --test-dir $$b -L psyq47 --no-tests=error \
	        --output-on-failure --output-junit $(CURDIR)/$$b/psyq47-junit.xml || fail=1; \
	done; \
	$(PYTHON) scripts/psyq47_job.py report $(PSYQ47_ZIP) $(CURDIR)/$(OUT)/psyq47-junit.xml \
	    $(CURDIR)/$(OUT)-zam/psyq47-junit.xml || fail=1; \
	exit $$fail

# All local jobs of the GitHub workflows: make check, then each make run of a CI job row.
ci: check
	$(MAKE) proofs CC=gcc SH2O_REQUIRE_PSYZ=1
	$(MAKE) test CC=clang SH2O_REQUIRE_PSYZ=1
	$(MAKE) test CC=gcc SH2O_ZAM=ON SH2O_REQUIRE_PSYZ=1
	$(MAKE) test CC=clang SH2O_ZAM=ON SH2O_REQUIRE_PSYZ=1
	$(MAKE) strict CC=gcc SH2O_REQUIRE_PSYZ=1
	$(MAKE) strict CC=clang SH2O_REQUIRE_PSYZ=1
	$(MAKE) sanitize CC=gcc SH2O_REQUIRE_PSYZ=1
	$(MAKE) sanitize CC=clang SH2O_REQUIRE_PSYZ=1
	$(MAKE) portable CC=gcc SH2O_REQUIRE_PSYZ=1
	$(MAKE) portable CC=clang SH2O_REQUIRE_PSYZ=1
	$(MAKE) install-check CC=gcc SH2O_REQUIRE_PSYZ=1
	$(MAKE) install-check CC=clang SH2O_REQUIRE_PSYZ=1
	$(MAKE) sh2zam-tests CC=gcc
	$(MAKE) sh2zam-tests CC=clang
	$(MAKE) psyq47 CC=gcc
	$(MAKE) cross
	$(MAKE) sh2-model
	$(MAKE) sh2-model SH2O_ZAM=ON

clean:
	rm -rf build
