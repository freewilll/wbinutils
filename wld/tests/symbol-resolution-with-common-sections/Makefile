$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-with-common-sections
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-with-common-sections

all: run

test: run

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/file: ${WLD} $(LOCAL_BUILD_DIR)/file1.o $(LOCAL_BUILD_DIR)/file2.o
	${WLD} $(LOCAL_BUILD_DIR)/file1.o $(LOCAL_BUILD_DIR)/file2.o -o $(LOCAL_BUILD_DIR)/file -static

run: $(LOCAL_BUILD_DIR)/file
	@MSG=$$(./file); ERR=$$? ;\
	if [ "$$ERR" != "10" ]; then \
		echo "Exit code was $$ERR, not 10."; \
		exit 1; \
	fi ;\
	echo "Common sections test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/file
