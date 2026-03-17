$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/lib-object-file-selection
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/lib-object-file-selection

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${WAS} $< -o $@

$(LOCAL_BUILD_DIR)/lib1.a: $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o $(LOCAL_BUILD_DIR)/lib3.o
	ar rcs $(LOCAL_BUILD_DIR)/lib1.a $(LOCAL_BUILD_DIR)/lib1.o $(LOCAL_BUILD_DIR)/lib2.o $(LOCAL_BUILD_DIR)/lib3.o

$(LOCAL_BUILD_DIR)/main:  $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/lib1.a ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/main.o -L$(LOCAL_BUILD_DIR) -l1 -o $(LOCAL_BUILD_DIR)/main -static

run-main: $(LOCAL_BUILD_DIR)/main
	@MSG=$$(./main); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	echo "lib-object-file-selection test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/lib1.a
	@rm -f $(LOCAL_BUILD_DIR)/main
