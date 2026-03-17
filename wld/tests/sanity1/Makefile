$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/sanity1
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/sanity1

all: | $(LOCAL_BUILD_DIR)
all: test

test: | $(LOCAL_BUILD_DIR)
test: run-sanity1

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/sanity1: $(LOCAL_BUILD_DIR)/sanity1.o ${WLD}
	${WLD} $(LOCAL_BUILD_DIR)/sanity1.o -o $(LOCAL_BUILD_DIR)/sanity1 -static

run-sanity1: $(LOCAL_BUILD_DIR)/sanity1
	@$(LOCAL_BUILD_DIR)/sanity1; ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	else \
		echo "Sanity 1 test passed"; \
	fi

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/sanity1
