$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-with-mixed-bss-and-common-sections
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-with-mixed-bss-and-common-sections

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-test1

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/test1: $(LOCAL_BUILD_DIR)/test1.o ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/test1.o -o $(LOCAL_BUILD_DIR)/test1 -static

run-test1: $(LOCAL_BUILD_DIR)/test1
	@$(LOCAL_BUILD_DIR)/test1; ERR=$$? ;\
	if [ "$$ERR" != "7" ]; then \
		echo "Exit code was $$ERR, not 7."; \
		exit 1; \
	else \
		echo "symbol-resolution-with-mixed-bss-and-common-sections test passed"; \
	fi

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/test1
