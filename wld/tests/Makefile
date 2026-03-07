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

.PHONY: $(TESTS)
$(TESTS):
	+$(MAKE) -C $@

clean:
	@for d in $(TESTS); do \
		$(MAKE) -C $$d clean; \
	done
