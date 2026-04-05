$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-versioning-double-undefined_symbol_resolution-with-default
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-versioning-double-undefined_symbol_resolution-with-default
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Deal with a special case of an undefined symbol being:
# - in a library as f1@V1
# - in an object $(LOCAL_BUILD_DIR)/file as f1
# The undefined f1 is seen first, then the undefined f1@V1, then the defined f1@@V1 which resolves both.

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${GCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_SRC_DIR)/libtest1.c $(LOCAL_SRC_DIR)/libtest1.map
	${GCC} $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.so -shared -Wl,--version-script=$(LOCAL_SRC_DIR)/libtest1.map

$(LOCAL_BUILD_DIR)/libtest2.so: $(LOCAL_BUILD_DIR)/libtest2.o
	@${WLD} -shared -L $(MUSL_LIBC_LIBRARY_PATH) -o $(LOCAL_BUILD_DIR)/libtest2.so -L $(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/libtest2.o -ltest1 -lc


$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.so
	@# Note the specfific order: -ltest2, -ltest1, $(LOCAL_BUILD_DIR)/main.o

	@${WLD} \
	$(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o \
	$(MUSL_LIBC_LIBRARY_PATH)/crti.o \
	$(GCC_CRTBEGINS) \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-L$(GCC_LIBRARY_PATH) \
	$(LOCAL_BUILD_DIR)/main.o \
	-o $(LOCAL_BUILD_DIR)/main \
	-lc -lgcc -lgcc_eh \
	-L$(LOCAL_BUILD_DIR) \
	-ltest2 \
	-ltest1 \
	$(GCC_CRTENDS) \
	$(MUSL_LIBC_LIBRARY_PATH)/crtn.o \
	-dynamic-linker $(MUSL_DYNAMIC_LINKER) \

run-main: $(LOCAL_BUILD_DIR)/main
	@LD_LIBRARY_PATH=$(LOCAL_BUILD_DIR) $(LOCAL_BUILD_DIR)/main
	@echo "Versioned symbol double undefined with default version test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
	@rm -f $(LOCAL_BUILD_DIR)/libtest1.so
	@rm -f $(LOCAL_BUILD_DIR)/libtest2.so
