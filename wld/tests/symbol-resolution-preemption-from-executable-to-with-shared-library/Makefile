$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-preemption-from-executable-to-with-shared-library
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-preemption-from-executable-to-with-shared-library
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Check that a symbol in a dynamic executable preempts the same symbol in a shared library

all: | $(LOCAL_BUILD_DIR)
all: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest1a.o: $(LOCAL_SRC_DIR)/libtest1.c
	${WCC} -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1a.o -fPIC

$(LOCAL_BUILD_DIR)/libtest1b.o: $(LOCAL_SRC_DIR)/libtest1.c
	${WCC} -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1b.o -fPIC

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_BUILD_DIR)/libtest1a.o
	${WLD} \
	-shared \
	-L $(GLIBC_LIBC_LIBRARY_PATH) \
	-L $(GCC_LIBRARY_PATH) \
	-o $(LOCAL_BUILD_DIR)/libtest1.so \
	$(LOCAL_BUILD_DIR)/libtest1a.o \
	-lc


$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest1b.o
	${WLD} \
	-dynamic-linker $(GLIBC_DYNAMIC_LINKER) \
	-L $(GLIBC_LIBC_LIBRARY_PATH) \
	-L $(GCC_LIBRARY_PATH) \
	-o $(LOCAL_BUILD_DIR)/main \
	$(FILE_crt1) \
	$(FILE_crti) \
	$(GCC_CRTBEGINS) \
	$(LOCAL_BUILD_DIR)/main.o \
	$(LOCAL_BUILD_DIR)/libtest1.so \
	-rpath $(LOCAL_BUILD_DIR) \
	-lc \
	-lgcc \
	-lgcc_eh \
	-lc \
	$(GCC_CRTENDS) \
	$(FILE_crtn) \
	-pie

run-main: $(LOCAL_BUILD_DIR)/main
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main

clean:
	rm -f $(LOCAL_BUILD_DIR)/main
	rm -f $(LOCAL_BUILD_DIR)/*.o
	rm -f $(LOCAL_BUILD_DIR)/*.so
