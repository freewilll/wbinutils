LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests

all: test

TESTS = \
	sanity1 \
	sanity2 \
	glibc-hello-world \
	glibc-math \
	musl-libc-hello-world \
	musl-libc-tls-hello-world \
	lib-object-file-selection \
	lib-two-libs-with-same-object-filename-inside \
	symbol-resolution-bindings \
	symbol-resolution-double-strong-bindings \
	symbol-resolution-weak-strong-bindings \
	symbol-resolution-with-common-sections \
	symbol-resolution-weak-strong-in-library \
	symbol-resolution-weak-strong-both-defined-in-library \
	symbol-resolution-weak-symbol-in-shared-library \
	symbol-resolution-preemption-from-executable-to-with-shared-library \
	local-symbols \
	abs-symbol \
	symbol-resolution-with-mixed-bss-and-common-sections \
	relocation-relaxation \
	relocation-locals \
	relocation-got \
	relocation-tls \
	relocation-R_X86_64_64-in-shared-library \
	relocation-R_X86_64_GOTPCRELX-in-dyn-executable \
	relocation-bss-in-shared-library \
	relocation-common-symbol-in-object-and-shared-library \
	array-lifecycle \
	orphan-sections \
	ifuncs \
	undefined-unreferenced-symbol \
	shared-lib-minimal \
	shared-lib-using-shared-lib \
	shared-lib-and-two-object-files \
	musl-libc-dynamic-hello-world \
	musl-libc-dynamic-fprintf-to-stdout \
	musl-libc-dynamic-R_X86_64_RELATIVE-relocations \
	musl-libc-dynamic-weak-strong-library-symbol \
	musl-libc-dynamic-pointer-to-function-in-lib \
	musl-libc-dynamic-pointer-to-function-in-lib-called-from-lib \
	musl-runtime-symbol-resolution-with-multiple-defines \
	glibc-tls-alignment \
	symbol-versioning \
	symbol-versioning-double-undefined_symbol_resolution-with-default \
	symbol-versioning-weak-strong-default-in-two-libs \
	glibc-dynamic-hello-world \
	glibc-dynamic-curses

.PHONY: test
test: $(TESTS)
	@echo wld tests passed

WAS ?= $(BUILD_DIR)/bin/was
WLD ?= $(BUILD_DIR)/bin/wld

WCC_ENV = LD=${WLD} AS=${WAS}

# Assume that the wcc repository has been checked out in the same place as wbinutils.
WCC      ?= ${WCC_ENV} $(abspath $(SRC_DIR)/../wcc/wcc)
MUSL_WCC ?= ${WCC_ENV} $(abspath $(SRC_DIR)/../wcc/musl-wcc)

GCC      ?= gcc
MUSL_GCC ?= musl-gcc
AS       ?= as

export WAS
export WLD
export WCC
export MUSL_WCC

export MUSL_GCC
export GCC
export AS

.PHONY: $(TESTS)
ifneq ($(BUILD_DIR),$(SRC_DIR))
$(TESTS):
	@mkdir -p $(LOCAL_BUILD_DIR)/$@
	+$(MAKE) -C $(LOCAL_BUILD_DIR)/$@ -f $(LOCAL_SRC_DIR)/$@/Makefile
else
$(TESTS):
	+$(MAKE) -C $@
endif

clean:
ifneq ($(BUILD_DIR),$(SRC_DIR))
	@case "$(LOCAL_BUILD_DIR)" in \
		""|"/"|".") \
			echo "Refusing to remove unsafe test build dir: $(LOCAL_BUILD_DIR)"; \
			exit 1; \
			;; \
		"$(LOCAL_SRC_DIR)"|$(LOCAL_SRC_DIR)/*) \
			echo "Refusing to remove source test dir: $(LOCAL_BUILD_DIR)"; \
			exit 1; \
			;; \
	esac
	@rm -rf $(LOCAL_BUILD_DIR)
else
	@for d in $(TESTS); do \
		$(MAKE) -C $$d clean; \
	done
endif
