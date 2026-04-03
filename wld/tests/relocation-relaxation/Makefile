$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/relocation-relaxation
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/relocation-relaxation
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main-static-relaxed run-main-static-unrelaxed run-main-dynamic-relaxed run-main-dynamic-unrelaxed

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/main-static-relaxed.o: $(LOCAL_SRC_DIR)/main.s
	# gas makes R_X86_64_GOTPCRELX relocations for the GOTPCREL instructions
	as $< -o $@

$(LOCAL_BUILD_DIR)/main-static-unrelaxed.o: $(LOCAL_SRC_DIR)/main.s
	# gas makes R_X86_64_GOTPCREL relocations for the GOTPCREL instructions
	as $< -o $@ -mrelax-relocations=no

$(LOCAL_BUILD_DIR)/main-dynamic-relaxed.o: $(LOCAL_SRC_DIR)/main.s
	# gas makes R_X86_64_GOTPCRELX relocations for the GOTPCREL instructions
	as $< -o $@

$(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed.o: $(LOCAL_SRC_DIR)/main.s
	# gas makes R_X86_64_GOTPCREL relocations for the GOTPCREL instructions
	as $< -o $@ -mrelax-relocations=no

$(LOCAL_BUILD_DIR)/main-static-relaxed: $(LOCAL_BUILD_DIR)/main-static-relaxed.o $(LOCAL_BUILD_DIR)/libtest.o
	@${WLD}                                               \
	-static                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(GCC_CRTBEGINS)                                      \
	-L/usr/lib/x86_64-linux-musl                          \
	-L$(GCC_LIBRARY_PATH)                                 \
	$(LOCAL_BUILD_DIR)/main-static-relaxed.o              \
	$(LOCAL_BUILD_DIR)/libtest.o                          \
	-o $(LOCAL_BUILD_DIR)/main-static-relaxed             \
	-lc -lgcc -lgcc_eh                                    \
	$(GCC_CRTENDS)                                        \
	/usr/lib/x86_64-linux-musl/crtn.o                     \

$(LOCAL_BUILD_DIR)/main-static-unrelaxed: $(LOCAL_BUILD_DIR)/main-static-unrelaxed.o $(LOCAL_BUILD_DIR)/libtest.o
	@${WLD}                                               \
	-static                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(GCC_CRTBEGINS)                                      \
	-L/usr/lib/x86_64-linux-musl                          \
	-L$(GCC_LIBRARY_PATH)                                 \
	$(LOCAL_BUILD_DIR)/libtest.o                          \
	$(LOCAL_BUILD_DIR)/main-static-unrelaxed.o            \
	-o $(LOCAL_BUILD_DIR)/main-static-unrelaxed           \
	-lc -lgcc -lgcc_eh                                    \
	$(GCC_CRTENDS)                                        \
	/usr/lib/x86_64-linux-musl/crtn.o                     \

$(LOCAL_BUILD_DIR)/main-dynamic-relaxed: $(LOCAL_BUILD_DIR)/main-dynamic-relaxed.o $(LOCAL_BUILD_DIR)/libtest.o
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(LOCAL_BUILD_DIR)/libtest.o                          \
	$(LOCAL_BUILD_DIR)/main-dynamic-relaxed.o             \
	-o $(LOCAL_BUILD_DIR)/main-dynamic-relaxed            \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-pie                                                  \

$(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed: $(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed.o $(LOCAL_BUILD_DIR)/libtest.o
	@${WLD}                                               \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(LOCAL_BUILD_DIR)/libtest.o                          \
	$(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed.o           \
	-o $(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed          \
	-L/usr/lib/x86_64-linux-musl                          \
	-dynamic-linker /lib/ld-musl-x86_64.so.1              \
	-lc                                                   \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-pie                                                  \

run-main-static-relaxed: $(LOCAL_BUILD_DIR)/main-static-relaxed
	$(LOCAL_BUILD_DIR)/main-static-relaxed
	echo "relocation-relaxation static relaxed test passed"

run-main-static-unrelaxed: $(LOCAL_BUILD_DIR)/main-static-unrelaxed
	$(LOCAL_BUILD_DIR)/main-static-unrelaxed
	echo "relocation-relaxation static unrelaxed test passed"

run-main-dynamic-relaxed: $(LOCAL_BUILD_DIR)/main-dynamic-relaxed
	$(LOCAL_BUILD_DIR)/main-dynamic-relaxed
	echo "relocation-relaxation dynamic relaxed test passed"

run-main-dynamic-unrelaxed: $(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed
	$(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed
	echo "relocation-relaxation dynamic unrelaxed test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main-static-relaxed
	@rm -f $(LOCAL_BUILD_DIR)/main-static-unrelaxed
	@rm -f $(LOCAL_BUILD_DIR)/main-dynamic-relaxed
	@rm -f $(LOCAL_BUILD_DIR)/main-dynamic-unrelaxed
