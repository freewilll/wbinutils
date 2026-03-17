$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/relocation-bss-in-shared-library
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/relocation-bss-in-shared-library

# Test a dynamic executable that uses .bss symbols from a shared library

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest1.o: $(LOCAL_SRC_DIR)/libtest1.c
	gcc -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.o -fPIC

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	gcc -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o -fPIC

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_BUILD_DIR)/libtest1.o
	${WLD}                                                \
	$(LOCAL_BUILD_DIR)/libtest1.o                         \
	-o $(LOCAL_BUILD_DIR)/libtest1.so                     \
	-L/usr/lib/x86_64-linux-musl                          \
	-lc                                                   \
	-shared

$(LOCAL_BUILD_DIR)/libtest2.so: $(LOCAL_BUILD_DIR)/libtest2.o
	${WLD}                                                \
	$(LOCAL_BUILD_DIR)/libtest2.o                         \
	-o $(LOCAL_BUILD_DIR)/libtest2.so                     \
	-L/usr/lib/x86_64-linux-musl                          \
	-lc                                                   \
	-shared

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	gcc -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.so
	${WLD}                                                \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(LOCAL_BUILD_DIR)/main.o                             \
	-o $(LOCAL_BUILD_DIR)/main                            \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	$(LOCAL_BUILD_DIR)/libtest1.so                        \
	$(LOCAL_BUILD_DIR)/libtest2.so                        \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-pie

run-main: $(LOCAL_BUILD_DIR)/main
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.so
	@rm -f $(LOCAL_BUILD_DIR)/main
