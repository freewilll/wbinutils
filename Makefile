-include config.mk

INSTALL_BIN_DIR = ${PREFIX}/bin

LIBS = \
	lib/libelf.a \
	lib/liblist.a \
	lib/libstrmap.a \
	lib/libstrmap_ordered.a \
	lib/libmap_ordered.a \
	lib/liberror.a \

all: ${LIBS} bin/was bin/wld

.PHONY: lib/liblist.a
lib/liblist.a:
	+${MAKE} -C lib/liblist

.PHONY: lib/libstrmap.a
lib/libstrmap.a:
	+${MAKE} -C lib/libstrmap

.PHONY: lib/libstrmap_ordered.a
lib/libstrmap_ordered.a:
	+${MAKE} -C lib/libstrmap_ordered

.PHONY: lib/libmap_ordered.a
lib/libmap_ordered.a:
	+${MAKE} -C lib/libmap_ordered

.PHONY: lib/liberror.a
lib/liberror.a:
	+${MAKE} -C lib/liberror

.PHONY: lib/libelf.a
lib/libelf.a: lib/liblist.a lib/libstrmap.a
	+${MAKE} -C lib/libelf

bin/was: ${LIBS}
	+${MAKE} -C was

bin/wld: ${LIBS}
	+${MAKE} -C wld

.PHONY: test
test: bin/was bin/wld lib/liblist.a lib/libstrmap.a lib/libstrmap_ordered.a lib/libmap_ordered.a
	+${MAKE} -C was test
	+${MAKE} -C wld test
	+${MAKE} -C lib/liblist test
	+${MAKE} -C lib/libstrmap test
	+${MAKE} -C lib/libstrmap_ordered test
	+${MAKE} -C lib/libmap_ordered test

install: bin/was bin/wld
	mkdir -p '${INSTALL_BIN_DIR}'
	cp bin/was '${INSTALL_BIN_DIR}/was'
	cp bin/wld '${INSTALL_BIN_DIR}/wld'

clean:
	+${MAKE} -C lib/liblist clean
	+${MAKE} -C lib/libstrmap clean
	+${MAKE} -C lib/libstrmap_ordered clean
	+${MAKE} -C lib/libmap_ordered clean
	+${MAKE} -C lib/liberror clean
	+${MAKE} -C lib/libelf clean
	+${MAKE} -C was clean
	+${MAKE} -C wld clean

distclean: clean
	@rm -f config.mk
