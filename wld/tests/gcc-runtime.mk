GCC ?= gcc
GCC_BASE ?= /usr/lib/gcc/x86_64-linux-gnu

ifeq ($(origin GCC_LIBRARY_PATH), undefined)
GCC_LIBRARY_PATH := $(shell \
	if [ -d "$(GCC_BASE)" ]; then \
		GCC_DIR="$$(ls -1 "$(GCC_BASE)" 2>/dev/null | sort -V | tail -n 1)"; \
		if [ -n "$$GCC_DIR" ]; then \
			echo "$(GCC_BASE)/$$GCC_DIR"; \
			exit 0; \
		fi; \
	fi; \
	dirname "$$($${GCC:-gcc} -print-libgcc-file-name 2>/dev/null)" \
)
endif

GCC_CRTBEGINS := $(GCC_LIBRARY_PATH)/crtbeginS.o
GCC_CRTENDS := $(GCC_LIBRARY_PATH)/crtendS.o
