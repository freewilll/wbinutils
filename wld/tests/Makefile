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
	shared-lib-using-shared-lib

.PHONY: sanity1
sanity1:
	make -C sanity1

.PHONY: sanity2
sanity2:
	make -C sanity2

.PHONY: glibc-math
glibc-math:
	make -C glibc-math

.PHONY: glibc-with-gcc-ld
glibc-with-gcc-ld:
	make -C glibc-with-gcc-ld

.PHONY: musl-libc-hello-world
musl-libc-hello-world:
	make -C musl-libc-hello-world

.PHONY: musl-libc-tls-hello-world
musl-libc-tls-hello-world:
	make -C musl-libc-tls-hello-world

.PHONY: lib-object-file-selection
lib-object-file-selection:
	make -C lib-object-file-selection

.PHONY: symbol-resolution-bindings
symbol-resolution-bindings:
	make -C symbol-resolution-bindings

.PHONY: symbol-resolution-double-strong-bindings
symbol-resolution-double-strong-bindings:
	make -C symbol-resolution-double-strong-bindings

.PHONY: symbol-resolution-weak-strong-bindings
symbol-resolution-weak-strong-bindings:
	make -C symbol-resolution-weak-strong-bindings

.PHONY: symbol-resolution-with-common-sections
symbol-resolution-with-common-sections:
	make -C symbol-resolution-with-common-sections

.PHONY: symbol-resolution-weak-strong-in-library
symbol-resolution-weak-strong-in-library:
	make -C symbol-resolution-weak-strong-in-library

.PHONY: symbol-resolution-weak-strong-both-defined-in-library
symbol-resolution-weak-strong-both-defined-in-library:
	make -C symbol-resolution-weak-strong-both-defined-in-library

.PHONY: local-symbols
local-symbols:
	make -C local-symbols

.PHONY: abs-symbol
abs-symbol:
	make -C abs-symbol

.PHONY: symbol-resolution-with-mixed-bss-and-common-sections
symbol-resolution-with-mixed-bss-and-common-sections:
	make -C symbol-resolution-with-mixed-bss-and-common-sections

.PHONY: relocation-relaxation
relocation-relaxation:
	make -C relocation-relaxation

.PHONY: relocation-locals
relocation-locals:
	make -C relocation-locals

.PHONY: relocation-got
relocation-got:
	make -C relocation-got

.PHONY: relocation-tls
relocation-tls:
	make -C relocation-tls

.PHONY: array-lifecycle
array-lifecycle:
	make -C array-lifecycle

.PHONY: orphan-sections
orphan-sections:
	make -C orphan-sections

.PHONY: ifuncs
ifuncs:
	make -C ifuncs

.PHONY: undefined-unreferenced-symbol
undefined-unreferenced-symbol:
	make -C undefined-unreferenced-symbol

.PHONY: shared-lib-minimal
shared-lib-minimal:
	make -C shared-lib-minimal

.PHONY: shared-lib-using-shared-lib
shared-lib-using-shared-lib:
	make -C shared-lib-using-shared-lib

clean:
	@make -C sanity1 clean
	@make -C sanity2 clean
	@make -C glibc-hello-world clean
	@make -C glibc-math clean
	@make -C musl-libc-hello-world clean
	@make -C musl-libc-tls-hello-world clean
	@make -C lib-object-file-selection clean
	@make -C symbol-resolution-bindings clean
	@make -C symbol-resolution-double-strong-bindings clean
	@make -C symbol-resolution-weak-strong-bindings clean
	@make -C symbol-resolution-with-common-sections clean
	@make -C symbol-resolution-weak-strong-in-library clean
	@make -C symbol-resolution-weak-strong-both-defined-in-library clean
	@make -C local-symbols clean
	@make -C abs-symbol clean
	@make -C symbol-resolution-with-mixed-bss-and-common-sections clean
	@make -C relocation-relaxation clean
	@make -C relocation-locals clean
	@make -C relocation-got clean
	@make -C relocation-tls clean
	@make -C array-lifecycle clean
	@make -C orphan-sections clean
	@make -C ifuncs clean
	@make -C undefined-unreferenced-symbol clean
	@make -C shared-lib-minimal clean
	@make -C shared-lib-using-shared-lib clean
