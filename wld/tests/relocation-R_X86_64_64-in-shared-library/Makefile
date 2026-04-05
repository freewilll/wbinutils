$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/relocation-R_X86_64_64-in-shared-library
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/relocation-R_X86_64_64-in-shared-library
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test a R_X86_64_64 relocation in a shared library

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest.o: $(LOCAL_SRC_DIR)/libtest.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest.c -o $(LOCAL_BUILD_DIR)/libtest.o -fPIC

$(LOCAL_BUILD_DIR)/libtest.so: $(LOCAL_BUILD_DIR)/libtest.o
	${WLD} \
	$(LOCAL_BUILD_DIR)/libtest.o \
	-o $(LOCAL_BUILD_DIR)/libtest.so \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-lc \
	-shared

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${GCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest.so
	${WLD} \
	$(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o \
	$(MUSL_LIBC_LIBRARY_PATH)/crti.o \
	$(LOCAL_BUILD_DIR)/main.o \
	-o $(LOCAL_BUILD_DIR)/main \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-dynamic-linker $(MUSL_DYNAMIC_LINKER) \
	-L $(LOCAL_BUILD_DIR) \
	-ltest \
	-lc \
	$(MUSL_LIBC_LIBRARY_PATH)/crtn.o \


run-main: $(LOCAL_BUILD_DIR)/main
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.so
	@rm -f $(LOCAL_BUILD_DIR)/main
