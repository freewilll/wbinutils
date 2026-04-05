$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/glibc-math
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/glibc-math
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o
	${WLD} \
		-L$(GLIBC_LIBC_LIBRARY_PATH) \
		-L$(GCC_LIBRARY_PATH) \
		-static \
		-o $(LOCAL_BUILD_DIR)/main \
		$(LOCAL_BUILD_DIR)/main.o \
		$(FILE_crt1) \
		$(FILE_crti) \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		-lm \
		$(FILE_crtn) \
		-static

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main
	echo "Glibc math passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
