$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/relocation-locals
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/relocation-locals

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/main: ${WLD} $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o $(LOCAL_BUILD_DIR)/lib3.o
	${WLD} -o $(LOCAL_BUILD_DIR)/main $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o $(LOCAL_BUILD_DIR)/lib3.o -static

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
