all: test

test: sanity1 sanity2 libc-with-gcc-ld lib-object-file-selection

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

clean:
	@make -C sanity1 clean
	@make -C sanity2 clean
	@make -C libc-with-gcc-ld clean
	@make -C lib-object-file-selection clean
