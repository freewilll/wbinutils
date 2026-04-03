$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-versioning-weak-strong-default-in-two-libs
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-versioning-weak-strong-default-in-two-libs
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test a case where two .so files are used by an executable
# .so1 defines f() as a weak non-default symbol
# .so2 defines f() as a strong default symbol
# The libs are loaded as -ltest1, -ltest2, $(LOCAL_BUILD_DIR)/main.o
# This tests a bug where the weak default symbol would not resolve the symbol in main.

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${GCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_SRC_DIR)/libtest1.c $(LOCAL_SRC_DIR)/libtest1.map
	${GCC} $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.so -shared -Wl,--version-script=$(LOCAL_SRC_DIR)/libtest1.map

$(LOCAL_BUILD_DIR)/libtest2.so: $(LOCAL_BUILD_DIR)/libtest2.o $(LOCAL_SRC_DIR)/libtest2.map
	${GCC} $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.so -shared -Wl,--version-script=$(LOCAL_SRC_DIR)/libtest2.map

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.so
	@# Note the specfific order: -ltest2, -ltest1, $(LOCAL_BUILD_DIR)/main.o

	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(GCC_CRTBEGINS)                                      \
	-L/usr/lib/x86_64-linux-musl                          \
	-L$(GCC_LIBRARY_PATH)                                 \
	$(LOCAL_BUILD_DIR)/main.o                             \
	-o $(LOCAL_BUILD_DIR)/main                            \
	-lc -lgcc -lgcc_eh                                    \
	-L$(LOCAL_BUILD_DIR)                                  \
	-ltest1                                               \
	-ltest2                                               \
	$(GCC_CRTENDS)                                        \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: $(LOCAL_BUILD_DIR)/main
	@LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main
	@echo "Versioned strong-non-default and weak-default symbols in two libs"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
	@rm -f $(LOCAL_BUILD_DIR)/libtest1.so
	@rm -f $(LOCAL_BUILD_DIR)/libtest2.so
