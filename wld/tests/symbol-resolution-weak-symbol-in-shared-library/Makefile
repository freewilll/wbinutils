$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-weak-symbol-in-shared-library
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-weak-symbol-in-shared-library

# Test a case originally found when dynamically linking sqlite3 with glibc.
# A library is built that calls stat(). stat() is declared as a weak hidden function
# in from an object $(LOCAL_BUILD_DIR)/file inside a library. This object $(LOCAL_BUILD_DIR)/file is linked
# both with the library and an executable that uses the library.
# wld must convert the weak symbol to a local symbol in the library.

all: | $(LOCAL_BUILD_DIR)
all: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${GCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/libtest1.o: $(LOCAL_SRC_DIR)/libtest1.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest1.c -o $(LOCAL_BUILD_DIR)/libtest1.o

$(LOCAL_BUILD_DIR)/libtest2.o: $(LOCAL_SRC_DIR)/libtest2.c
	${GCC} -c $(LOCAL_SRC_DIR)/libtest2.c -o $(LOCAL_BUILD_DIR)/libtest2.o

$(LOCAL_BUILD_DIR)/libtest1.so: $(LOCAL_BUILD_DIR)/libtest1.o $(LOCAL_BUILD_DIR)/libtest2.o
	${WLD} \
	-shared                            \
	-L /usr/lib                        \
	-L /usr/lib/gcc/x86_64-linux-gnu/9 \
	-o $(LOCAL_BUILD_DIR)/libtest1.so                     \
	$(LOCAL_BUILD_DIR)/libtest1.o                         \
	$(LOCAL_BUILD_DIR)/libtest2.o                         \
	-lc

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/libtest1.so $(LOCAL_BUILD_DIR)/libtest2.o
	${WLD} \
	-dynamic-linker /lib64/ld-linux-x86-64.so.2  \
	-L /usr/lib                                  \
	-L /usr/lib/gcc/x86_64-linux-gnu/9           \
	-o $(LOCAL_BUILD_DIR)/main                   \
	/usr/lib/x86_64-linux-gnu/crt1.o             \
	/usr/lib/x86_64-linux-gnu/crti.o             \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o  \
	$(LOCAL_BUILD_DIR)/main.o                    \
	$(LOCAL_BUILD_DIR)/libtest1.so               \
	$(LOCAL_BUILD_DIR)/libtest2.o                \
	-rpath $(LOCAL_BUILD_DIR)                    \
	-lc                                          \
	-lgcc                                        \
	-lgcc_eh                                     \
	-lc                                          \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o    \
	/usr/lib/x86_64-linux-gnu/crtn.o             \
	-pie

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main

clean:
	rm -f $(LOCAL_BUILD_DIR)/main
	rm -f $(LOCAL_BUILD_DIR)/*.o
	rm -f $(LOCAL_BUILD_DIR)/*.so
