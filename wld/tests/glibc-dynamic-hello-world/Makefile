$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/glibc-dynamic-hello-world
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/glibc-dynamic-hello-world
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o -g

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o
	${WLD} \
	-dynamic-linker $(GLIBC_DYNAMIC_LINKER) \
	-L $(GLIBC_LIBC_LIBRARY_PATH) \
	-L $(GCC_LIBRARY_PATH) \
	-o $(LOCAL_BUILD_DIR)/main \
	$(FILE_crt1) \
	$(FILE_crti) \
	$(GCC_CRTBEGINS) \
	$(LOCAL_BUILD_DIR)/main.o \
	-lc \
	-lgcc \
	-lgcc_eh \
	-lc \
	$(GCC_CRTENDS) \
	$(FILE_crtn) \

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main # Catch non-zero error
	@MSG=$$(./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\

	## Ensure ld-linux isn't present in .dynamic section NEEDED
	@! readelf -dW $(LOCAL_BUILD_DIR)/main | grep NEEDED | grep ld-linux

	echo "Glibc dynamic hello world passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
