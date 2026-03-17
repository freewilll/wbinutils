$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/shared-lib-and-two-object-files
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/shared-lib-and-two-object-files

# A executable that uses a data and function symbol from a library
# The library itself has two object files, with one using data and a function from the other
# This tests the case of making as shared library that has object files with relocations in it.
# The relocations are meant to persist into the .so $(LOCAL_BUILD_DIR)/file and not get resolved statically.

CURRENT_DIR := $(LOCAL_BUILD_DIR)

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/libtest.o: $(LOCAL_SRC_DIR)/libtest.c
	${WCC} -fPIC -c $(LOCAL_SRC_DIR)/libtest.c -o $(LOCAL_BUILD_DIR)/libtest.o

$(LOCAL_BUILD_DIR)/libfuncs.o: $(LOCAL_SRC_DIR)/libfuncs.c
	${WCC} -fPIC -c $(LOCAL_SRC_DIR)/libfuncs.c -o $(LOCAL_BUILD_DIR)/libfuncs.o

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest.so: $(LOCAL_BUILD_DIR)/libtest.o $(LOCAL_BUILD_DIR)/libfuncs.o ${WLD}
	${WLD} \
		-shared \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o $(LOCAL_BUILD_DIR)/libtest.so \
		$(LOCAL_BUILD_DIR)/libtest.o \
		$(LOCAL_BUILD_DIR)/libfuncs.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest.so
	${WLD} \
		-rpath ${CURRENT_DIR} \
		-dynamic-linker /lib64/ld-linux-x86-64.so.2 \
		-L/usr/lib \
		-L/usr/lib/gcc/x86_64-linux-gnu/9 \
		-o $(LOCAL_BUILD_DIR)/main \
		$(LOCAL_BUILD_DIR)/main.o \
		-L $(LOCAL_BUILD_DIR) \
		-ltest \
		/usr/lib/x86_64-linux-gnu/crt1.o \
		/usr/lib/x86_64-linux-gnu/crti.o \
		-lc \
		-lgcc \
		-lgcc_eh \
		-lc \
		/usr/lib/x86_64-linux-gnu/crtn.o \

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main
	echo "Minimal lib test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.so
	@rm -f $(LOCAL_BUILD_DIR)/main
