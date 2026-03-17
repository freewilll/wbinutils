$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/relocation-got
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/relocation-got

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-main run-main-with-optional

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/main: $(LOCAL_BUILD_DIR)/main.o
	${WLD} -o $(LOCAL_BUILD_DIR)/main $(LOCAL_BUILD_DIR)/main.o -static

run-main: $(LOCAL_BUILD_DIR)/main
	$(LOCAL_BUILD_DIR)/main
	echo "relocation-got test passed"

main-with-optional: $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/optional.o
	${WLD} -o main-with-optional $(LOCAL_BUILD_DIR)/main.o $(LOCAL_BUILD_DIR)/optional.o -static

run-main-with-optional: main-with-optional
	@$(LOCAL_BUILD_DIR)/main-with-optional; ERR=$$? ;\
	if [ "$$ERR" != "2" ]; then \
		echo "Exit code was $$ERR, not 2."; \
		exit 1; \
	else \
		echo "relocation-got with optional test passed"; \
	fi

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/main
