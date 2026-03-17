$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/array-lifecycle
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/array-lifecycle

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/main.o -o $(LOCAL_BUILD_DIR)/main -static

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
