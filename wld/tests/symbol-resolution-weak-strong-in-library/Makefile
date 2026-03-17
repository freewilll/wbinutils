$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-weak-strong-in-library
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-weak-strong-in-library

# Test a bug where an undefined weak symbol doesn't get upgraded to strong

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c | $(LOCAL_BUILD_DIR)
	${GCC} $< -c -o $@

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/liba.a: $(LOCAL_BUILD_DIR)/test1.o $(LOCAL_BUILD_DIR)/test2.o $(LOCAL_BUILD_DIR)/test3.o
	ar rcs $@ $(LOCAL_BUILD_DIR)/test1.o $(LOCAL_BUILD_DIR)/test2.o $(LOCAL_BUILD_DIR)/test3.o

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/liba.a $(LOCAL_BUILD_DIR)/main.o ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/main.o -L $(LOCAL_BUILD_DIR) -la -o $(LOCAL_BUILD_DIR)/main -static

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.a
	@rm -f $(LOCAL_BUILD_DIR)/main
