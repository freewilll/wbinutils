$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/shared-lib-minimal
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/shared-lib-minimal
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# A simple executable that uses a data and function symbol from a library

CURRENT_DIR := $(LOCAL_BUILD_DIR)

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest.o: $(LOCAL_SRC_DIR)/libtest.c
	${WCC} -shared -c $(LOCAL_SRC_DIR)/libtest.c -o $(LOCAL_BUILD_DIR)/libtest.o

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest.so: $(LOCAL_BUILD_DIR)/libtest.o
	${WLD} \
		-shared \
		-L$(GLIBC_LIBC_LIBRARY_PATH) \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest.so \
		$(LOCAL_BUILD_DIR)/libtest.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest.so
	${WLD} \
		-rpath ${CURRENT_DIR} \
		-dynamic-linker $(GLIBC_DYNAMIC_LINKER) \
		-L$(GLIBC_LIBC_LIBRARY_PATH) \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/main \
		$(LOCAL_BUILD_DIR)/main.o \
		-L $(LOCAL_BUILD_DIR) \
		-ltest \
		$(FILE_crt1) \
		$(FILE_crti) \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		$(FILE_crtn) \

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main
	echo "Minimal lib test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.so
	@rm -f $(LOCAL_BUILD_DIR)/main
