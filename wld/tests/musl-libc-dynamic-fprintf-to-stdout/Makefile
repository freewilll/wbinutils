$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/musl-libc-dynamic-fprintf-to-stdout
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/musl-libc-dynamic-fprintf-to-stdout
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# This tests use case of wld adding a R_X86_64_COPY relocation
# due to $(LOCAL_BUILD_DIR)/main accessing an object in a library.
# This happens when accessing stdout in musl, which is also tested here

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${MUSL_WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest1.o: $(LOCAL_SRC_DIR)/libtest1.c
	${MUSL_WCC} -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.o -fPIC

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	${MUSL_WCC} -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o -fPIC

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_BUILD_DIR)/libtest1.o
	${WLD} \
		-shared \
		-L$(GLIBC_LIBC_LIBRARY_PATH) \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest1.so \
		$(LOCAL_BUILD_DIR)/libtest1.o \
		-L$(MUSL_LIBC_LIBRARY_PATH) \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

$(LOCAL_BUILD_DIR)/libtest2.so: $(LOCAL_BUILD_DIR)/libtest2.o $(LOCAL_BUILD_DIR)/libtest1.so
	${WLD} \
		-shared \
		-L$(GLIBC_LIBC_LIBRARY_PATH) \
		-L$(GCC_LIBRARY_PATH) \
		-o $(LOCAL_BUILD_DIR)/libtest2.so \
		$(LOCAL_BUILD_DIR)/libtest2.o \
		-L$(MUSL_LIBC_LIBRARY_PATH) \
		-L $(LOCAL_BUILD_DIR) \
		-ltest1 \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		-L $(LOCAL_BUILD_DIR) \
		-ltest1

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.so
	${WLD} \
	$(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o \
	$(MUSL_LIBC_LIBRARY_PATH)/crti.o \
	$(LOCAL_BUILD_DIR)/main.o \
	-o $(LOCAL_BUILD_DIR)/main \
	-L $(LOCAL_BUILD_DIR) \
	-ltest1 \
	-ltest2 \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-dynamic-linker $(MUSL_DYNAMIC_LINKER) \
	-lc \
	$(MUSL_LIBC_LIBRARY_PATH)/crtn.o \
	-pie

run-main: $(LOCAL_BUILD_DIR)/main
	LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main
	@MSG=$$(LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Musl hello world passed"
	echo "Using object in shared library and musl stdout passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
	@rm -f $(LOCAL_BUILD_DIR)/libtest.so
	@rm -f $(LOCAL_BUILD_DIR)/libtest2.so
