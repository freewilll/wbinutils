$(LOCAL_BUILD_DIR):
	@mkdir -p $@

LOCAL_BUILD_DIR := $(BUILD_DIR)/wld/tests/symbol-resolution-weak-strong-bindings
LOCAL_SRC_DIR := $(SRC_DIR)/wld/tests/symbol-resolution-weak-strong-bindings

# Add three symbols of the same name:
# The first one is undefined in a $(LOCAL_BUILD_DIR)/main object file. It is either strong or weak.
# The second one can be a object or lib and weak or strong
# The third is like the second

# These tests also have much more concise unit tests,
# so are somewhat redundant. But they are left here anyways, since
# they are good e2e tests.
all: | $(LOCAL_BUILD_DIR)
all: test

test: \
	run-mains_os_ow \
	run-mains_ls_ow \
	run-mains_os_lw \
	run-mains_ls_lw \
	run-mains_ow_os \
	run-mains_lw_os \
	run-mains_ow_ls \
	run-mains_lw_ls \
	run-mainw_os_ow \
	run-mainw_ls_ow \
	run-mainw_os_lw \
	run-mainw_ls_lw \
	run-mainw_ow_os \
	run-mainw_lw_os \
	run-mainw_ow_ls \
	run-mainw_lw_ls \

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.c | $(LOCAL_BUILD_DIR)
	${GCC} $< -c -o $@

$(LOCAL_BUILD_DIR)/%.o: $(LOCAL_SRC_DIR)/%.s | $(LOCAL_BUILD_DIR)
	${AS} $< -o $@

$(LOCAL_BUILD_DIR)/%.a: $(LOCAL_BUILD_DIR)/%.o | $(LOCAL_BUILD_DIR)
	ar rcs $@ $<

$(LOCAL_BUILD_DIR)/mains_os_ow: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.o
	${WLD} $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.o -o $(LOCAL_BUILD_DIR)/mains_os_ow -static

$(LOCAL_BUILD_DIR)/mains_ls_ow: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.o
	${WLD} $(LOCAL_BUILD_DIR)/mains.o -L $(LOCAL_BUILD_DIR) -ls $(LOCAL_BUILD_DIR)/libw.o -o $(LOCAL_BUILD_DIR)/mains_ls_ow -static

$(LOCAL_BUILD_DIR)/mains_os_lw: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.o -L $(LOCAL_BUILD_DIR) -lw -o $(LOCAL_BUILD_DIR)/mains_os_lw -static

$(LOCAL_BUILD_DIR)/mains_ls_lw: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mains.o -L $(LOCAL_BUILD_DIR) -ls -lw -o $(LOCAL_BUILD_DIR)/mains_ls_lw -static

$(LOCAL_BUILD_DIR)/mains_ow_os: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.o ${WLD1}
	${WLD} $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libw.o $(LOCAL_BUILD_DIR)/libs.o -o $(LOCAL_BUILD_DIR)/mains_ow_os -static

$(LOCAL_BUILD_DIR)/mains_ow_ls: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.o
	${WLD} $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libw.o -L $(LOCAL_BUILD_DIR) -ls -o $(LOCAL_BUILD_DIR)/mains_ow_ls -static

$(LOCAL_BUILD_DIR)/mains_lw_os: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mains.o -L $(LOCAL_BUILD_DIR) -lw $(LOCAL_BUILD_DIR)/libs.o -o $(LOCAL_BUILD_DIR)/mains_lw_os -static

$(LOCAL_BUILD_DIR)/mains_lw_ls: $(LOCAL_BUILD_DIR)/mains.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mains.o -L $(LOCAL_BUILD_DIR) -lw -ls -o $(LOCAL_BUILD_DIR)/mains_lw_ls -static

$(LOCAL_BUILD_DIR)/mainw_os_ow: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.o
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.o -o $(LOCAL_BUILD_DIR)/mainw_os_ow -static

$(LOCAL_BUILD_DIR)/mainw_ls_ow: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.o
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o -L $(LOCAL_BUILD_DIR) -ls $(LOCAL_BUILD_DIR)/libw.o -o $(LOCAL_BUILD_DIR)/mainw_ls_ow -static

$(LOCAL_BUILD_DIR)/mainw_os_lw: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.o -L $(LOCAL_BUILD_DIR) -lw -o $(LOCAL_BUILD_DIR)/mainw_os_lw -static

$(LOCAL_BUILD_DIR)/mainw_ls_lw: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o -L $(LOCAL_BUILD_DIR) -ls -lw -o $(LOCAL_BUILD_DIR)/mainw_ls_lw -static

$(LOCAL_BUILD_DIR)/mainw_ow_os: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.o ${WLD1}
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libw.o $(LOCAL_BUILD_DIR)/libs.o -o $(LOCAL_BUILD_DIR)/mainw_ow_os -static

$(LOCAL_BUILD_DIR)/mainw_ow_ls: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.o
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libw.o -L $(LOCAL_BUILD_DIR) -ls -o $(LOCAL_BUILD_DIR)/mainw_ow_ls -static

$(LOCAL_BUILD_DIR)/mainw_lw_os: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.o $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o -L $(LOCAL_BUILD_DIR) -lw $(LOCAL_BUILD_DIR)/libs.o -o $(LOCAL_BUILD_DIR)/mainw_lw_os -static

$(LOCAL_BUILD_DIR)/mainw_lw_ls: $(LOCAL_BUILD_DIR)/mainw.o $(LOCAL_BUILD_DIR)/libs.a $(LOCAL_BUILD_DIR)/libw.a
	${WLD} $(LOCAL_BUILD_DIR)/mainw.o -L $(LOCAL_BUILD_DIR) -lw -ls -o $(LOCAL_BUILD_DIR)/mainw_lw_ls -static

run-mains_%: $(LOCAL_BUILD_DIR)/mains_%
	@$<; ERR=$$? ; if [ "$$ERR" != "1" ]; then echo "Exit code was $$ERR, not 1."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mains_%_ls: $(LOCAL_BUILD_DIR)/mains_%_ls
	@$<; ERR=$$? ; if [ "$$ERR" != "2" ]; then echo "Exit code was $$ERR, not 2."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_%: $(LOCAL_BUILD_DIR)/mainw_%
	@$<; ERR=$$? ; if [ "$$ERR" != "2" ]; then echo "Exit code was $$ERR, not 2."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_os_%: $(LOCAL_BUILD_DIR)/mainw_os_%
	@$<; ERR=$$? ; if [ "$$ERR" != "1" ]; then echo "Exit code was $$ERR, not 1."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_ls_lw: $(LOCAL_BUILD_DIR)/mainw_ls_lw
	@$<; ERR=$$? ; if [ "$$ERR" != "0" ]; then echo "Exit code was $$ERR, not 0."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_%_os: $(LOCAL_BUILD_DIR)/mainw_%_os
	@$<; ERR=$$? ; if [ "$$ERR" != "1" ]; then echo "Exit code was $$ERR, not 1."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

run-mainw_lw_ls: $(LOCAL_BUILD_DIR)/mainw_lw_ls
	@$<; ERR=$$? ; if [ "$$ERR" != "0" ]; then echo "Exit code was $$ERR, not 0."; exit 1; fi
	@echo "lib-object-file-selection $< test passed"

clean:
	@rm -f $(LOCAL_BUILD_DIR)/*.o
	@rm -f $(LOCAL_BUILD_DIR)/*.a
	@rm -f $(LOCAL_BUILD_DIR)/mains_*
	@rm -f $(LOCAL_BUILD_DIR)/mainw_*
