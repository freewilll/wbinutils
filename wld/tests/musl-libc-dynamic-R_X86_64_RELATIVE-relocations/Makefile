$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/musl-libc-dynamic-R_X86_64_RELATIVE-relocations
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/musl-libc-dynamic-R_X86_64_RELATIVE-relocations
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test the conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${MUSL_WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o
	${WLD} \
	$(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o \
	$(MUSL_LIBC_LIBRARY_PATH)/crti.o \
	$(LOCAL_BUILD_DIR)/main.o \
	-o $(LOCAL_BUILD_DIR)/main \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-dynamic-linker $(MUSL_DYNAMIC_LINKER) \
	-lc \
	$(MUSL_LIBC_LIBRARY_PATH)/crtn.o

run-main: $(LOCAL_BUILD_DIR)/main
	 $(LOCAL_BUILD_DIR)/main
	@MSG=$$(./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Conversion of a R_X86_64_64 to a R_X86_64_RELATIVE in dynamic executables passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
