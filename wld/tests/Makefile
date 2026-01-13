all: test

test: \
	sanity1 \
	sanity2 \
	glibc-hello-world \
	glibc-math \
	musl-libc-hello-world \
	musl-libc-tls-hello-world \
	lib-object-file-selection \
	symbol-resolution-bindings \
	symbol-resolution-double-strong-bindings \
	symbol-resolution-weak-strong-bindings \
	symbol-resolution-with-common-sections \
	symbol-resolution-weak-strong-in-library \
	symbol-resolution-weak-strong-both-defined-in-library \
	local-symbols \
	abs-symbol \
	symbol-resolution-with-mixed-bss-and-common-sections \
	relocation-relaxation \
	relocation-locals \
	relocation-got \
	relocation-tls \
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
	musl-libc-dynamic-weak-strong-library-symbol

.PHONY: sanity1
sanity1:
	+$(MAKE) -C sanity1

.PHONY: sanity2
sanity2:
	+$(MAKE) -C sanity2

.PHONY: glibc-math
glibc-math:
	+$(MAKE) -C glibc-math

.PHONY: glibc-with-gcc-ld
glibc-with-gcc-ld:
	+$(MAKE) -C glibc-with-gcc-ld

.PHONY: musl-libc-hello-world
musl-libc-hello-world:
	+$(MAKE) -C musl-libc-hello-world

.PHONY: musl-libc-tls-hello-world
musl-libc-tls-hello-world:
	+$(MAKE) -C musl-libc-tls-hello-world

.PHONY: lib-object-file-selection
lib-object-file-selection:
	+$(MAKE) -C lib-object-file-selection

.PHONY: symbol-resolution-bindings
symbol-resolution-bindings:
	+$(MAKE) -C symbol-resolution-bindings

.PHONY: symbol-resolution-double-strong-bindings
symbol-resolution-double-strong-bindings:
	+$(MAKE) -C symbol-resolution-double-strong-bindings

.PHONY: symbol-resolution-weak-strong-bindings
symbol-resolution-weak-strong-bindings:
	+$(MAKE) -C symbol-resolution-weak-strong-bindings

.PHONY: symbol-resolution-with-common-sections
symbol-resolution-with-common-sections:
	+$(MAKE) -C symbol-resolution-with-common-sections

.PHONY: symbol-resolution-weak-strong-in-library
symbol-resolution-weak-strong-in-library:
	+$(MAKE) -C symbol-resolution-weak-strong-in-library

.PHONY: symbol-resolution-weak-strong-both-defined-in-library
symbol-resolution-weak-strong-both-defined-in-library:
	+$(MAKE) -C symbol-resolution-weak-strong-both-defined-in-library

.PHONY: local-symbols
local-symbols:
	+$(MAKE) -C local-symbols

.PHONY: abs-symbol
abs-symbol:
	+$(MAKE) -C abs-symbol

.PHONY: symbol-resolution-with-mixed-bss-and-common-sections
symbol-resolution-with-mixed-bss-and-common-sections:
	+$(MAKE) -C symbol-resolution-with-mixed-bss-and-common-sections

.PHONY: relocation-relaxation
relocation-relaxation:
	+$(MAKE) -C relocation-relaxation

.PHONY: relocation-locals
relocation-locals:
	+$(MAKE) -C relocation-locals

.PHONY: relocation-got
relocation-got:
	+$(MAKE) -C relocation-got

.PHONY: relocation-tls
relocation-tls:
	+$(MAKE) -C relocation-tls

.PHONY: array-lifecycle
array-lifecycle:
	+$(MAKE) -C array-lifecycle

.PHONY: orphan-sections
orphan-sections:
	+$(MAKE) -C orphan-sections

.PHONY: ifuncs
ifuncs:
	+$(MAKE) -C ifuncs

.PHONY: undefined-unreferenced-symbol
undefined-unreferenced-symbol:
	+$(MAKE) -C undefined-unreferenced-symbol

.PHONY: shared-lib-minimal
shared-lib-minimal:
	+$(MAKE) -C shared-lib-minimal

.PHONY: shared-lib-using-shared-lib
shared-lib-using-shared-lib:
	+$(MAKE) -C shared-lib-using-shared-lib

.PHONY: shared-lib-and-two-object-files
shared-lib-and-two-object-files:
	+$(MAKE) -C shared-lib-and-two-object-files

.PHONY: musl-libc-dynamic-hello-world
musl-libc-dynamic-hello-world:
	+$(MAKE) -C musl-libc-dynamic-hello-world

.PHONY: musl-libc-dynamic-fprintf-to-stdout
musl-libc-dynamic-fprintf-to-stdout:
	+$(MAKE) -C musl-libc-dynamic-fprintf-to-stdout

.PHONY: musl-libc-dynamic-R_X86_64_RELATIVE-relocations
musl-libc-dynamic-R_X86_64_RELATIVE-relocations:
	+$(MAKE) -C musl-libc-dynamic-R_X86_64_RELATIVE-relocations

.PHONY: musl-libc-dynamic-weak-strong-library-symbol
musl-libc-dynamic-weak-strong-library-symbol:
	+$(MAKE) -C musl-libc-dynamic-weak-strong-library-symbol

clean:
	@+$(MAKE) -C sanity1 clean
	@+$(MAKE) -C sanity2 clean
	@+$(MAKE) -C glibc-hello-world clean
	@+$(MAKE) -C glibc-math clean
	@+$(MAKE) -C musl-libc-hello-world clean
	@+$(MAKE) -C musl-libc-tls-hello-world clean
	@+$(MAKE) -C lib-object-file-selection clean
	@+$(MAKE) -C symbol-resolution-bindings clean
	@+$(MAKE) -C symbol-resolution-double-strong-bindings clean
	@+$(MAKE) -C symbol-resolution-weak-strong-bindings clean
	@+$(MAKE) -C symbol-resolution-with-common-sections clean
	@+$(MAKE) -C symbol-resolution-weak-strong-in-library clean
	@+$(MAKE) -C symbol-resolution-weak-strong-both-defined-in-library clean
	@+$(MAKE) -C local-symbols clean
	@+$(MAKE) -C abs-symbol clean
	@+$(MAKE) -C symbol-resolution-with-mixed-bss-and-common-sections clean
	@+$(MAKE) -C relocation-relaxation clean
	@+$(MAKE) -C relocation-locals clean
	@+$(MAKE) -C relocation-got clean
	@+$(MAKE) -C relocation-tls clean
	@+$(MAKE) -C array-lifecycle clean
	@+$(MAKE) -C orphan-sections clean
	@+$(MAKE) -C ifuncs clean
	@+$(MAKE) -C undefined-unreferenced-symbol clean
	@+$(MAKE) -C shared-lib-minimal clean
	@+$(MAKE) -C shared-lib-using-shared-lib clean
	@+$(MAKE) -C shared-lib-and-two-object-files clean
	@+$(MAKE) -C musl-libc-dynamic-hello-world clean
	@+$(MAKE) -C musl-libc-dynamic-fprintf-to-stdout clean
	@+$(MAKE) -C musl-libc-dynamic-R_X86_64_RELATIVE-relocations clean
	@+$(MAKE) -C musl-libc-dynamic-weak-strong-library-symbol clean
