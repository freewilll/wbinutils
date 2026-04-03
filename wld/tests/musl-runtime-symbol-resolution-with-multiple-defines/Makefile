$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/musl-runtime-symbol-resolution-with-multiple-defines
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/musl-runtime-symbol-resolution-with-multiple-defines
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test two cases:
# 1. An object $(LOCAL_BUILD_DIR)/file defines a variable. Two libraries also define it, but the definition in the object $(LOCAL_BUILD_DIR)/file comes first
# 2. Two library files define a variable. The first definition comes first. The order is tested.

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main-with-def run-main-no-def-test1-first run-main-no-def-test2-first

$(LOCAL_BUILD_DIR)/main-no-def.o: $(LOCAL_SRC_DIR)/main-no-def.c
	${GCC} -c -o $(LOCAL_BUILD_DIR)/main-no-def.o $(LOCAL_SRC_DIR)/main-no-def.c

$(LOCAL_BUILD_DIR)/main-with-def.o: $(LOCAL_SRC_DIR)/main-with-def.c
	${GCC} -c -o $(LOCAL_BUILD_DIR)/main-with-def.o $(LOCAL_SRC_DIR)/main-with-def.c

$(LOCAL_BUILD_DIR)/libtest1.o: $(LOCAL_SRC_DIR)/libtest1.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.o -g -fPIC

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o -g -fPIC

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_BUILD_DIR)/libtest1.o
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest1.so \
		$(LOCAL_BUILD_DIR)/libtest1.o \
		-L/usr/lib/x86_64-linux-musl \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

$(LOCAL_BUILD_DIR)/libtest2.so: $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.o
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest2.so \
		$(LOCAL_BUILD_DIR)/libtest2.o \
		-L/usr/lib/x86_64-linux-musl \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		-L $(LOCAL_BUILD_DIR) \
		-ltest1

$(LOCAL_BUILD_DIR)/main-with-def: $(LOCAL_BUILD_DIR)/main-with-def.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(LOCAL_BUILD_DIR)/main-with-def.o                  \
	-o $(LOCAL_BUILD_DIR)/main-with-def                 \
	-L $(LOCAL_BUILD_DIR)                               \
	-ltest1                                               \
	-ltest2                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

$(LOCAL_BUILD_DIR)/main-no-def-test1-first: $(LOCAL_BUILD_DIR)/main-no-def.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(LOCAL_BUILD_DIR)/main-no-def.o                      \
	-o $(LOCAL_BUILD_DIR)/main-no-def-test1-first         \
	-L $(LOCAL_BUILD_DIR)                                 \
	-ltest1                                               \
	-ltest2                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

$(LOCAL_BUILD_DIR)/main-no-def-test2-first: $(LOCAL_BUILD_DIR)/main-no-def.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(LOCAL_BUILD_DIR)/main-no-def.o                      \
	-o $(LOCAL_BUILD_DIR)/main-no-def-test2-first         \
	-L $(LOCAL_BUILD_DIR)                                 \
	-ltest2                                               \
	-ltest1                                               \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o

run-main-with-def: $(LOCAL_BUILD_DIR)/main-with-def
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main-with-def; \
	test $$? -eq 1 || { echo "Exit code was $$?, not 1"; exit 1; }
	@echo "Runtime symbol resolution with-def passed"

run-main-no-def-test1-first: $(LOCAL_BUILD_DIR)/main-no-def-test1-first
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main-no-def-test1-first; \
	test $$? -eq 2 || { echo "Exit code was $$?, not 2"; exit 1; }
	@echo "Runtime symbol resolution no-def-test1 passed"

run-main-no-def-test2-first: $(LOCAL_BUILD_DIR)/main-no-def-test2-first
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main-no-def-test2-first; \
	test $$? -eq 3 || { echo "Exit code was $$?, not 3"; exit 1; }
	@echo "Runtime symbol resolution no-def-test2 passed"

clean:
	rm -f $(LOCAL_BUILD_DIR)/*.so
	rm -f $(LOCAL_BUILD_DIR)/*.o
	rm -f $(LOCAL_BUILD_DIR)/main-with-def
	rm -f $(LOCAL_BUILD_DIR)/main-no-def-test1-first
	rm -f $(LOCAL_BUILD_DIR)/main-no-def-test2-first
