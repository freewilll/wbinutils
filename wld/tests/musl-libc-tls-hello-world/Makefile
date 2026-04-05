$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/musl-libc-tls-hello-world
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/musl-libc-tls-hello-world
include $(LOCAL_SRC_DIR)/../gcc-runtime.mk

# A couple of tests of executables that have combinations of .tdata and.or .tbss sections.

test: | $(LOCAL_BUILD_DIR)
test: all

all: | $(LOCAL_BUILD_DIR)\
	run-main-data \
	run-main-bss \
	run-main-data-and-bss

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c | $(LOCAL_BUILD_DIR)
	${GCC} -c $< -o $@

$(LOCAL_BUILD_DIR)/main-bss: $(LOCAL_BUILD_DIR)/main-bss.o
	${GCC} $(LOCAL_BUILD_DIR)/main-bss.o -o $(LOCAL_BUILD_DIR)/main-bss

$(LOCAL_BUILD_DIR)/main-data-and-bss: $(LOCAL_BUILD_DIR)/main-data-and-bss.o
	${GCC} $(LOCAL_BUILD_DIR)/main-data-and-bss.o -o $(LOCAL_BUILD_DIR)/main-data-and-bss

$(LOCAL_BUILD_DIR)/%-data: $(LOCAL_BUILD_DIR)/%-data.o | $(LOCAL_BUILD_DIR)
	@${WLD} \
	$(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o \
	$(MUSL_LIBC_LIBRARY_PATH)/crti.o \
	$(GCC_CRTBEGINS) \
	-L$(MUSL_LIBC_LIBRARY_PATH) \
	-L$(GCC_LIBRARY_PATH) \
	$< \
	-o $@ \
	-lc -lgcc -lgcc_eh \
	$(GCC_CRTENDS) \
	$(MUSL_LIBC_LIBRARY_PATH)/crtn.o \
	-static

.PHONY: run-main-data
run-main-data: $(LOCAL_BUILD_DIR)/main-data
	@$(LOCAL_BUILD_DIR)/main-data # Catch non-zero error
	@MSG=$$(./main-data);\
	if [ "$$MSG" != "Hello World! 42" ]; then \
		echo "Message was not Hello World! 42"; \
		exit 1; \
	fi ;\
	echo "Musl hello world data passed"

.PHONY: run-main-bss
run-main-bss: $(LOCAL_BUILD_DIR)/main-bss
	@$(LOCAL_BUILD_DIR)/main-bss # Catch non-zero error exit code
	@MSG=$$(./main-bss);\
	if [ "$$MSG" != "Hello World! 0" ]; then \
		echo "Message was not Hello World! 0"; \
		exit 1; \
	fi ;\
	echo "Musl hello world bss passed"

.PHONY: run-main-bss
run-main-data-and-bss: $(LOCAL_BUILD_DIR)/main-data-and-bss
	@$(LOCAL_BUILD_DIR)/main-data-and-bss # Catch non-zero error exit code
	@MSG=$$(./main-data-and-bss);\
	if [ "$$MSG" != "Hello World! 42" ]; then \
		echo "Message was not Hello World! 42"; \
		exit 1; \
	fi ;\
	echo "Musl hello world data and bss passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main-data
	@rm -f $(LOCAL_BUILD_DIR)/main-bss
	@rm -f $(LOCAL_BUILD_DIR)/main-data-and-bss
