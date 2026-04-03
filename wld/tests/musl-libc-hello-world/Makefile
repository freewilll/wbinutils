$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/musl-libc-hello-world
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/musl-libc-hello-world
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${WAS} $< -o $@

$(LOCAL_BUILD_DIR)/main.o: $(LOCAL_SRC_DIR)/main.c
	${WCC} -c $(LOCAL_SRC_DIR)/main.c -o $(LOCAL_BUILD_DIR)/main.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o
	${WLD} \
	/usr/lib/x86_64-linux-musl/Scrt1.o                    \
	/usr/lib/x86_64-linux-musl/crti.o                     \
	$(GCC_CRTBEGINS)                                      \
	-L/usr/lib/x86_64-linux-musl                          \
	-L$(GCC_LIBRARY_PATH)                                 \
	$(LOCAL_BUILD_DIR)/main.o                             \
	-o $(LOCAL_BUILD_DIR)/main                            \
	-lc -lgcc -lgcc_eh                                    \
	$(GCC_CRTENDS)                                        \
	/usr/lib/x86_64-linux-musl/crtn.o                     \
	-static

run-main: $(LOCAL_BUILD_DIR)/main
	@$(LOCAL_BUILD_DIR)/main # Catch non-zero error
	@MSG=$$(./main);\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Musl hello world passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
