$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/archive-file-on-command-line
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/archive-file-on-command-line
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o -g

$(LOCAL_BUILD_DIR)/test.o: $(LOCAL_SRC_DIR)/test.c
	${WCC} -c $(LOCAL_SRC_DIR)/test.c -o $(LOCAL_BUILD_DIR)/test.o -g

$(LOCAL_BUILD_DIR)/test.a: $(LOCAL_BUILD_DIR)/test.o
	@rm -f $(LOCAL_BUILD_DIR)/test.a
	ar rcs $@ $<

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/test.a
	${WLD} \
	-dynamic-linker $(GLIBC_DYNAMIC_LINKER) \
	-L $(GLIBC_LIBC_LIBRARY_PATH) \
	-L $(GCC_LIBRARY_PATH) \
	-o $(LOCAL_BUILD_DIR)/main \
	$(FILE_crt1) \
	$(FILE_crti) \
	$(GCC_CRTBEGINS) \
	$(LOCAL_BUILD_DIR)/main.o \
	$(LOCAL_BUILD_DIR)/test.a \
	-lc \
	-lgcc \
	-lgcc_eh \
	-lc \
	$(GCC_CRTENDS) \
	$(FILE_crtn) \

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main

	echo "Linking with .a file test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.a
	@rm -f $(LOCAL_BUILD_DIR)/main
