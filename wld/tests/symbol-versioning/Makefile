$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-versioning
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-versioning
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Symbol versioning test
# Four symbols are exported in a lib: f1@V1, f1@V2 and f2@V1 and f2@V2.
# f1 has a default version V1 and f2 a default version V2.

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest.so: $(LOCAL_SRC_DIR)/libtest.c $(LOCAL_SRC_DIR)/libtest.map
	${MUSL_GCC} $(LOCAL_SRC_DIR)/libtest.c -o $(LOCAL_BUILD_DIR)/libtest.so -shared -Wl,--version-script=$(LOCAL_SRC_DIR)/libtest.map

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${MUSL_WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest.so
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(GCC_CRTBEGINS)                                      \
	-L/usr/lib/x86_64-linux-musl                          \
	-L$(GCC_LIBRARY_PATH)                                 \
	$(LOCAL_BUILD_DIR)/main.o                             \
	-o $(LOCAL_BUILD_DIR)/main                            \
	-lc -lgcc -lgcc_eh                                    \
	-L$(LOCAL_BUILD_DIR) -ltest                           \
	$(GCC_CRTENDS)                                        \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: $(LOCAL_BUILD_DIR)/main
	@LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main
	@echo "Versioned symbol defaults test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
	@rm -f $(LOCAL_BUILD_DIR)/libtest.so
