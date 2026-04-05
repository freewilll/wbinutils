$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/musl-libc-dynamic-weak-strong-library-symbol
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/musl-libc-dynamic-weak-strong-library-symbol
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test that a strong symbol in an .o $(LOCAL_BUILD_DIR)/file overrides a weak symbol in an .so $(LOCAL_BUILD_DIR)/file

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest.o: $(LOCAL_SRC_DIR)/libtest.s
	${AS} -c $(LOCAL_SRC_DIR)/libtest.s -o $(LOCAL_BUILD_DIR)/libtest.o

$(LOCAL_BUILD_DIR)/libtest.so: $(LOCAL_BUILD_DIR)/libtest.o ${WLD}
	${WLD} \
		-shared \
		-L$(GLIBC_LIBC_LIBRARY_PATH) \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest.so \
		$(LOCAL_BUILD_DIR)/libtest.o \
		-L$(MUSL_LIBC_LIBRARY_PATH) \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest.so ${WLD}
	${WLD} \
	$(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o \
	$(MUSL_LIBC_LIBRARY_PATH)/crti.o \
	$(GCC_CRTBEGINS) \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-L$(GCC_LIBRARY_PATH) \
	$(LOCAL_BUILD_DIR)/main.o \
	-o $(LOCAL_BUILD_DIR)/main \
 	-L $(LOCAL_BUILD_DIR) \
 	-ltest \
	-lc -lgcc -lgcc_eh \
	-dynamic-linker $(MUSL_DYNAMIC_LINKER) \
	$(GCC_CRTENDS) \
	$(MUSL_LIBC_LIBRARY_PATH)/crtn.o

run-main: $(LOCAL_BUILD_DIR)/main
	@LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
	@rm -f $(LOCAL_BUILD_DIR)/libtest.so
