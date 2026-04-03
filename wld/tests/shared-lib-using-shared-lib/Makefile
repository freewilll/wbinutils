$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/shared-lib-using-shared-lib
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/shared-lib-using-shared-lib
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# There are three files with dependencies: $(LOCAL_SRC_DIR)/main.c -> $(LOCAL_SRC_DIR)/libtest1.c -> $(LOCAL_SRC_DIR)/libtest2.c
# $(LOCAL_BUILD_DIR)/main is a dynamic executable and the libs .so files.
# This tests the GOT handling and entries in .rela.dyn for both libs.

CURRENT_DIR := $(LOCAL_BUILD_DIR)

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest1.o: $(LOCAL_SRC_DIR)/libtest1.c
	${WCC} -fPIC -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.o

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	${WCC} -fPIC -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_BUILD_DIR)/libtest1.o $(LOCAL_BUILD_DIR)/libtest2.so
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest1.so \
		$(LOCAL_BUILD_DIR)/libtest1.o \
		-L $(LOCAL_BUILD_DIR) \
		-ltest2 \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

$(LOCAL_BUILD_DIR)/libtest2.so: $(LOCAL_BUILD_DIR)/libtest2.o
	@${WLD} \
		-shared \
		-L/usr/lib \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest2.so \
		$(LOCAL_BUILD_DIR)/libtest2.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so
	@LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) ${WLD} \
		-dynamic-linker /lib64/ld-linux-x86-64.so.2 \
		-L/usr/lib \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/main \
		$(LOCAL_BUILD_DIR)/main.o \
		-L $(LOCAL_BUILD_DIR) \
		-ltest1 \
		/usr/lib/x86_64-linux-gnu/crt1.o \
		/usr/lib/x86_64-linux-gnu/crti.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		/usr/lib/x86_64-linux-gnu/crtn.o \

run-main: $(LOCAL_BUILD_DIR)/main
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main
	echo "Lib using lib test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.so
	@rm -f $(LOCAL_BUILD_DIR)/main
