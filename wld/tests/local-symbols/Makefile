$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/local-symbols
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/local-symbols

all: | $(LOCAL_BUILD_DIR)
all: run-test1

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/test1: $(LOCAL_BUILD_DIR)/test1.o ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/test1.o -o $(LOCAL_BUILD_DIR)/test1 -static

run-test1: $(LOCAL_BUILD_DIR)/test1
	@$(LOCAL_BUILD_DIR)/test1; ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	else \
		echo "Local symbols test passed"; \
	fi

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/test1
