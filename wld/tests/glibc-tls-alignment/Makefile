$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/glibc-tls-alignment
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/glibc-tls-alignment
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test the TLS segment size not being a multiple of the aligment.
# This caused a bug where the start of the TLS section with respect to the thread pointer
# was incorrect since it has to be a multiple of the aligmnment.

all: | $(LOCAL_BUILD_DIR)
all: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c | $(LOCAL_BUILD_DIR)
	@# Use gcc since the source files have __thread which wcc doesn't support
	${GCC} -c $< -o $@

# Note the unusual placement of $(LOCAL_BUILD_DIR)/main.o at the end. This causes the TLS segment to be sized a multiple of 4
$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/test.o
	@${WLD} \
	-static \
	-L \
	$(GLIBC_LIBC_LIBRARY_PATH) \
	-L \
	$(GCC_LIBRARY_PATH) \
	-o $(LOCAL_BUILD_DIR)/main \
	$(FILE_crt1) \
	$(FILE_crti) \
	$(GCC_CRTBEGINS) \
	-lc \
	-lgcc \
	-lgcc_eh \
	$(LOCAL_BUILD_DIR)/test.o \
	-lc $(GCC_CRTENDS) \
	$(FILE_crtn) \
	$(LOCAL_BUILD_DIR)/main.o

.PHONY: run-main
run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main
	@echo glibc-tls-alignment test passed

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
