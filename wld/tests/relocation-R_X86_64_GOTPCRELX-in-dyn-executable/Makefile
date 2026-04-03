$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/relocation-R_X86_64_GOTPCRELX-in-dyn-executable
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/relocation-R_X86_64_GOTPCRELX-in-dyn-executable
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Test a GOTPCREL relocation in a call *foo@GOTPCREL(%rip) in a dynamic executable

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest.so: $(LOCAL_SRC_DIR)/libtest.c
	${GCC} $(LOCAL_SRC_DIR)/libtest.c -o $(LOCAL_BUILD_DIR)/libtest.so -shared

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.s
	# gas makes GOTPCREL instructions with -mrelax-relocations=no
	${AS} $(LOCAL_SRC_DIR)/main.s -o $(LOCAL_BUILD_DIR)/main.o -mrelax-relocations=no

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest.so
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(GCC_CRTBEGINS)                                      \
	-L/usr/lib/x86_64-linux-musl                          \
	-L$(GCC_LIBRARY_PATH)                                 \
	$(LOCAL_BUILD_DIR)/main.o                             \
	-o $(LOCAL_BUILD_DIR)/main                            \
	-lc -lgcc -lgcc_eh                                    \
	-L$(LOCAL_BUILD_DIR) -ltest                           \
	$(GCC_CRTENDS)                                        \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \

run-main: $(LOCAL_BUILD_DIR)/main
	@LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main
	@echo "Test GOTPCREL relocation in a call *foo@GOTPCREL(%rip) in a dynamic executable succeeded"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
	@rm -f $(LOCAL_BUILD_DIR)/libtest.so
