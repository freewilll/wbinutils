LOCAL_BUILD_DIR := $(BUILD_DIR)/was/tests
LOCAL_SRC_DIR := $(SRC_DIR)/was/tests

all: test-hello

.PHONY: test
test: test-hello

.PHONY: test-hello
test-hello: $(LOCAL_SRC_DIR)/hello.s $(BUILD_DIR)/bin/was
	@mkdir -p $(LOCAL_BUILD_DIR)
	@$(BUILD_DIR)/bin/was $(LOCAL_SRC_DIR)/hello.s -o $(LOCAL_BUILD_DIR)/hello.o
	@gcc -Werror $(LOCAL_BUILD_DIR)/hello.o -o $(LOCAL_BUILD_DIR)/hello
	@$(LOCAL_BUILD_DIR)/hello > $(LOCAL_BUILD_DIR)/hello.out
	@echo "Hello world!" > $(LOCAL_BUILD_DIR)/hello.out.expected
	diff $(LOCAL_BUILD_DIR)/hello.out.expected $(LOCAL_BUILD_DIR)/hello.out

clean:
	@rm -f $(LOCAL_BUILD_DIR)/hello.o
	@rm -f $(LOCAL_BUILD_DIR)/hello
	@rm -f $(LOCAL_BUILD_DIR)/hello.out
	@rm -f $(LOCAL_BUILD_DIR)/hello.out.expected
