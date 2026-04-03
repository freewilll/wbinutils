$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/undefined-unreferenced-symbol
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/undefined-unreferenced-symbol
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# A symbol may be undefined so long as it isn't used.
# This happens with __pthread_create in glibc

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${WAS} $< -o $@

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${GCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o
	${WLD} \
		-L/usr/lib \
		-L$(GCC_LIBRARY_PATH) \
		-static \
		-o $(LOCAL_BUILD_DIR)/main \
		$(LOCAL_BUILD_DIR)/main.o \
		/usr/lib/x86_64-linux-gnu/crt1.o \
		/usr/lib/x86_64-linux-gnu/crti.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		/usr/lib/x86_64-linux-gnu/crtn.o

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main
	echo "Undefined unreferenced symbol test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
