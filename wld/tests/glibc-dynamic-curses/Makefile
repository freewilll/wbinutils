$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/glibc-dynamic-curses
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/glibc-dynamic-curses
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# Only enable the test if libcurses.a exists
HAS_LIBCURSES_A := $(shell \
	for d in $(GLIBC_LIBC_LIBRARY_PATH) $(GCC_LIBRARY_PATH) /usr/lib /usr/lib64; do \
		if [ -f $$d/libcurses.a ]; then echo yes; exit; fi; \
	done)
ifeq ($(HAS_LIBCURSES_A),yes)
test: | $(LOCAL_BUILD_DIR)
test: run-main
else
test:
	@echo "Skipping glibc-dynamic-curses test (no libcurses.a)"
endif

all: test

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	# wcc can't cope with curses.h, so use gcc
	gcc  -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o -g

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
	-lcurses \
	$(GCC_CRTENDS) \
	$(FILE_crtn) \

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main
	@echo Glibc curses test passed

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
