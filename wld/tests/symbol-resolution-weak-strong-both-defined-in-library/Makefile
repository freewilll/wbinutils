$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-weak-strong-both-defined-in-library
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-weak-strong-both-defined-in-library

# A library has one symbol defined twice: weak, then strong.
# If the second object $(LOCAL_BUILD_DIR)/file is included, the strong symbol takes over the weak.

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main12 run-main21

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c | $(LOCAL_BUILD_DIR)
	${GCC} $< -c -o $@

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/liba12.a: $(LOCAL_BUILD_DIR)/test1.o $(LOCAL_BUILD_DIR)/test2.o
	ar rcs $@ $(LOCAL_BUILD_DIR)/test1.o $(LOCAL_BUILD_DIR)/test2.o

$(LOCAL_BUILD_DIR)/liba21.a: $(LOCAL_BUILD_DIR)/test1.o $(LOCAL_BUILD_DIR)/test2.o
	ar rcs $@ $(LOCAL_BUILD_DIR)/test2.o $(LOCAL_BUILD_DIR)/test1.o

$(LOCAL_BUILD_DIR)/main12: $(LOCAL_BUILD_DIR)/liba12.a $(LOCAL_BUILD_DIR)/main.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o -L $(LOCAL_BUILD_DIR) -la12 -o $(LOCAL_BUILD_DIR)/main12 -static

$(LOCAL_BUILD_DIR)/main21: $(LOCAL_BUILD_DIR)/liba21.a $(LOCAL_BUILD_DIR)/main.o
	${WLD} $(LOCAL_BUILD_DIR)/main.o -L $(LOCAL_BUILD_DIR) -la21 -o $(LOCAL_BUILD_DIR)/main21 -static

run-main12: $(LOCAL_BUILD_DIR)/main12
	$(LOCAL_BUILD_DIR)/main12

run-main21: $(LOCAL_BUILD_DIR)/main21
	$(LOCAL_BUILD_DIR)/main21

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.a
	@rm -f $(LOCAL_BUILD_DIR)/main12
	@rm -f $(LOCAL_BUILD_DIR)/main21

