$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/musl-libc-dynamic-pointer-to-function-in-lib-called-from-lib
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/musl-libc-dynamic-pointer-to-function-in-lib-called-from-lib
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test a case of a function pointer to a function in a library
# A R_X86_64_64 relocation must be present in .rela.dyn
# This tests the case of a $(LOCAL_BUILD_DIR)/main calling a library calling a library

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest.so: $(LOCAL_SRC_DIR)/libtest.c
	${WCC} $(LOCAL_SRC_DIR)/libtest.c -o $(LOCAL_BUILD_DIR)/libtest.so -shared

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest.so
	${WLD} \
	$(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o \
	$(MUSL_LIBC_LIBRARY_PATH)/crti.o \
	$(GCC_CRTBEGINS) \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-L$(GCC_LIBRARY_PATH) \
	$(LOCAL_BUILD_DIR)/main.o \
	-o $(LOCAL_BUILD_DIR)/main \
	-lc -lgcc -lgcc_eh \
	-L$(LOCAL_BUILD_DIR) -ltest \
	$(GCC_CRTENDS) \
	$(MUSL_LIBC_LIBRARY_PATH)/crtn.o \
	-dynamic-linker $(MUSL_DYNAMIC_LINKER) \

run-main: $(LOCAL_BUILD_DIR)/main
	@LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main
	@echo "Musl pointer to function in lib when called from lib passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
	@rm -f $(LOCAL_BUILD_DIR)/libtest.so
