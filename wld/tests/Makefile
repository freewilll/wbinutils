all: test

test: \
	sanity1 \
	sanity2 \
	libc-with-gcc-ld \
	lib-object-file-selection \
	symbol-resolution-bindings \
	symbol-resolution-double-strong-bindings \
	symbol-resolution-weak-strong-bindings \
	symbol-resolution-with-common-sections \
	local-symbols \
	abs-symbol \
	symbol-resolution-with-mixed-bss-and-common-sections \
	relocation-relaxation \
	relocation-locals \

.PHONY: sanity1
sanity1:
	make -C sanity1

.PHONY: sanity2
sanity2:
	make -C sanity2

.PHONY: libc-with-gcc-ld
libc-with-gcc-ld:
	make -C libc-with-gcc-ld

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

clean:
	@make -C sanity1 clean
	@make -C sanity2 clean
	@make -C libc-with-gcc-ld clean
	@make -C lib-object-file-selection clean
	@make -C symbol-resolution-bindings clean
	@make -C symbol-resolution-double-strong-bindings clean
	@make -C symbol-resolution-weak-strong-bindings clean
	@make -C symbol-resolution-with-common-sections clean
	@make -C local-symbols clean
	@make -C abs-symbol clean
	@make -C symbol-resolution-with-mixed-bss-and-common-sections clean
	@make -C relocation-relaxation clean
	@make -C relocation-locals clean
