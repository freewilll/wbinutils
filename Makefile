export SRC_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
export BUILD_DIR := $(CURDIR)
CONFIG ?= $(BUILD_DIR)/config.mk

-include $(CONFIG)

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
	mkdir -p $(BUILD_DIR)/lib
	+${MAKE} -C ${SRC_DIR}/lib/liblist

.PHONY: lib/libstrmap.a
lib/libstrmap.a:
	mkdir -p $(BUILD_DIR)/lib/libstrmap
	+${MAKE} -C ${SRC_DIR}/lib/libstrmap

.PHONY: lib/libstrmap_ordered.a
lib/libstrmap_ordered.a:
	mkdir -p $(BUILD_DIR)/lib/libstrmap_ordered
	+${MAKE} -C ${SRC_DIR}/lib/libstrmap_ordered

.PHONY: lib/libmap_ordered.a
lib/libmap_ordered.a:
	mkdir -p $(BUILD_DIR)/lib/libmap_ordered
	+${MAKE} -C ${SRC_DIR}/lib/libmap_ordered

.PHONY: lib/liberror.a
lib/liberror.a:
	mkdir -p $(BUILD_DIR)/lib/liberror
	+${MAKE} -C ${SRC_DIR}/lib/liberror

.PHONY: lib/libelf.a
lib/libelf.a: lib/liblist.a lib/libstrmap.a
	mkdir -p $(BUILD_DIR)/lib/libelf
	+${MAKE} -C ${SRC_DIR}/lib/libelf

bin/was: ${LIBS}
	mkdir -p $(BUILD_DIR)/was
	+${MAKE} -C ${SRC_DIR}/was

bin/wld: ${LIBS}
	mkdir -p $(BUILD_DIR)/wld
	+${MAKE} -C ${SRC_DIR}/wld

.PHONY: test
test: bin/was bin/wld lib/liblist.a lib/libstrmap.a lib/libstrmap_ordered.a lib/libmap_ordered.a
	+${MAKE} -C ${SRC_DIR}/was test
	+${MAKE} -C ${SRC_DIR}/wld test
	+${MAKE} -C ${SRC_DIR}/lib/liblist test
	+${MAKE} -C ${SRC_DIR}/lib/libstrmap test
	+${MAKE} -C ${SRC_DIR}/lib/libstrmap_ordered test
	+${MAKE} -C ${SRC_DIR}/lib/libmap_ordered test
	@echo wbinutils tests passed

install: bin/was bin/wld
	mkdir -p '${INSTALL_BIN_DIR}'
	cp bin/was '${INSTALL_BIN_DIR}/was'
	cp bin/wld '${INSTALL_BIN_DIR}/wld'

clean:
	+${MAKE} -C ${SRC_DIR}/lib/liblist clean
	+${MAKE} -C ${SRC_DIR}/lib/libstrmap clean
	+${MAKE} -C ${SRC_DIR}/lib/libstrmap_ordered clean
	+${MAKE} -C ${SRC_DIR}/lib/libmap_ordered clean
	+${MAKE} -C ${SRC_DIR}/lib/liberror clean
	+${MAKE} -C ${SRC_DIR}/lib/libelf clean
	+${MAKE} -C ${SRC_DIR}/was clean
	+${MAKE} -C ${SRC_DIR}/wld clean

# Detect out-of-source build
ifneq ($(SRC_DIR),$(BUILD_DIR))

distclean: clean
	@rm -f config.mk
	@rm -f Makefile

else

distclean: clean
	@rm -f config.mk

endif
