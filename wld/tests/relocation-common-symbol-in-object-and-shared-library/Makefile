$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/relocation-common-symbol-in-object-and-shared-library
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/relocation-common-symbol-in-object-and-shared-library
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test some scenarios with a common symbol in an object $(LOCAL_BUILD_DIR)/file that
# is resolved by a symbol in a shared library that can be in the .bss or .data.

all: | $(LOCAL_BUILD_DIR)
all: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${GCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest1.o: $(LOCAL_SRC_DIR)/libtest1.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.o -fPIC

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o -fPIC

$(LOCAL_BUILD_DIR)/libtest3.o: $(LOCAL_SRC_DIR)/libtest3.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest3.c -o $(LOCAL_BUILD_DIR)/libtest3.o -fPIC

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_BUILD_DIR)/libtest1.o $(LOCAL_BUILD_DIR)/libtest2.o $(LOCAL_BUILD_DIR)/libtest3.o
	${WLD} \
	-shared                            \
	-L /usr/lib                        \
	-L $(GCC_LIBRARY_PATH) \
	-o $(LOCAL_BUILD_DIR)/libtest1.so  \
	$(LOCAL_BUILD_DIR)/libtest1.o      \
	$(LOCAL_BUILD_DIR)/libtest2.o      \
	$(LOCAL_BUILD_DIR)/libtest3.o      \
	-lc

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so
	${WLD} \
	-dynamic-linker /lib64/ld-linux-x86-64.so.2  \
	-L /usr/lib                                  \
	-L $(GCC_LIBRARY_PATH)                       \
	-o $(LOCAL_BUILD_DIR)/main                   \
	/usr/lib/x86_64-linux-gnu/crt1.o             \
	/usr/lib/x86_64-linux-gnu/crti.o             \
	$(GCC_CRTBEGINS)                             \
	$(LOCAL_BUILD_DIR)/main.o                    \
	$(LOCAL_BUILD_DIR)/libtest1.so               \
	-rpath $(LOCAL_BUILD_DIR)                    \
	-lc                                          \
	-lgcc                                        \
	-lgcc_eh                                     \
	-lc                                          \
	$(GCC_CRTENDS)                               \
	/usr/lib/x86_64-linux-gnu/crtn.o             \
	-pie

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main

clean:
	rm -f $(LOCAL_BUILD_DIR)/main
	rm -f $(LOCAL_BUILD_DIR)/*.o
	rm -f $(LOCAL_BUILD_DIR)/*.so
