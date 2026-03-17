$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/sanity2
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/sanity2

all: | $(LOCAL_BUILD_DIR)
all: run-sanity2ab run-sanity2ba

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/sanity2ab: ${WLD} $(LOCAL_BUILD_DIR)/sanity2a.o $(LOCAL_BUILD_DIR)/sanity2b.o
	${WLD} $(LOCAL_BUILD_DIR)/sanity2a.o $(LOCAL_BUILD_DIR)/sanity2b.o -o $(LOCAL_BUILD_DIR)/sanity2ab -static

$(LOCAL_BUILD_DIR)/sanity2ba: ${WLD} $(LOCAL_BUILD_DIR)/sanity2a.o $(LOCAL_BUILD_DIR)/sanity2b.o
	${WLD} $(LOCAL_BUILD_DIR)/sanity2b.o $(LOCAL_BUILD_DIR)/sanity2a.o -o $(LOCAL_BUILD_DIR)/sanity2ba -static

run-sanity2ab: $(LOCAL_BUILD_DIR)/sanity2ab
	@MSG=$$(./sanity2ab); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Sanity 2ab test passed"

run-sanity2ba: $(LOCAL_BUILD_DIR)/sanity2ba
	@MSG=$$(./sanity2ba); ERR=$$? ;\
	if [ "$$ERR" != "42" ]; then \
		echo "Exit code was $$ERR, not 42."; \
		exit 1; \
	fi ;\
	if [ "$$MSG" != "Hello World!" ]; then \
		echo "Message was not hello world."; \
		exit 1; \
	fi ;\
	echo "Sanity 2ba test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/sanity2ab
	@rm -f $(LOCAL_BUILD_DIR)/sanity2ba
