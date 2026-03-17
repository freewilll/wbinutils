$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/glibc-dynamic-curses
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/glibc-dynamic-curses

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	# wcc can't cope with curses.h, so use gcc
	gcc  -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o -g

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o
	${WLD} \
	-dynamic-linker /lib64/ld-linux-x86-64.so.2  \
	-L /usr/lib                                  \
	-L /usr/lib/gcc/x86_64-linux-gnu/9           \
	-o $(LOCAL_BUILD_DIR)/main                   \
	/usr/lib/x86_64-linux-gnu/crt1.o             \
	/usr/lib/x86_64-linux-gnu/crti.o             \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtbeginS.o  \
	$(LOCAL_BUILD_DIR)/main.o                    \
	-lc                                          \
	-lgcc                                        \
	-lgcc_eh                                     \
	-lc                                          \
	-lcurses                                     \
	/usr/lib/gcc/x86_64-linux-gnu/9/crtendS.o    \
	/usr/lib/x86_64-linux-gnu/crtn.o \

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
