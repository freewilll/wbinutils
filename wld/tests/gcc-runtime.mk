GCC ?= gcc

# Mirrors what's present wcc's configure

ifeq ($(origin FILE_crt1), undefined)
FILE_crt1 := $(shell $(GCC) -print-file-name=crt1.o 2>/dev/null)
endif

ifeq ($(origin FILE_crti), undefined)
FILE_crti := $(shell $(GCC) -print-file-name=crti.o 2>/dev/null)
endif

ifeq ($(origin FILE_crtn), undefined)
FILE_crtn := $(shell $(GCC) -print-file-name=crtn.o 2>/dev/null)
endif

ifeq ($(origin FILE_crtbegin), undefined)
FILE_crtbegin := $(shell $(GCC) -print-file-name=crtbegin.o 2>/dev/null)
endif

ifeq ($(origin FILE_crtend), undefined)
FILE_crtend := $(shell $(GCC) -print-file-name=crtend.o 2>/dev/null)
endif

ifeq ($(origin GCC_LIBRARY_PATH), undefined)
GCC_LIBRARY_PATH := $(shell dirname "$$($(GCC) -print-libgcc-file-name 2>/dev/null)")
endif

GLIBC_DYNAMIC_LINKER ?= /lib64/ld-linux-x86-64.so.2
GLIBC_LIBC_LIBRARY_PATH ?= /usr/lib
GLIBC_STARTFILES ?= $(FILE_crt1) $(FILE_crti) $(FILE_crtbegin)
GLIBC_ENDFILES ?= $(FILE_crtend) $(FILE_crtn)
GLIBC_INCLUDE_PATHS ?= "/usr/local/include", "/usr/include/x86_64-linux-gnu", "/usr/include"

MUSL_DYNAMIC_LINKER ?= /lib/ld-musl-x86_64.so.1

ifeq ($(origin MUSL_LIBC_LIBRARY_PATH), undefined)
MUSL_LIBC_LIBRARY_PATH := $(shell \
	if [ -d "/usr/lib/x86_64-linux-musl" ]; then \
		echo /usr/lib/x86_64-linux-musl; \
	elif [ -d "/usr/x86_64-linux-musl/lib64" ]; then \
		echo /usr/x86_64-linux-musl/lib64; \
	else \
		echo /usr/lib; \
	fi \
)
endif

ifeq ($(origin MUSL_INCLUDE_PATHS), undefined)
MUSL_INCLUDE_PATHS := $(shell \
	if [ -d "/usr/include/x86_64-linux-musl" ]; then \
		echo '"/usr/include/x86_64-linux-musl"'; \
	elif [ -d "/usr/x86_64-linux-musl/include" ]; then \
		echo '"/usr/x86_64-linux-musl/include"'; \
	else \
		echo '"/usr/include"'; \
	fi \
)
endif

MUSL_STARTFILES ?= $(MUSL_LIBC_LIBRARY_PATH)/Scrt1.o $(MUSL_LIBC_LIBRARY_PATH)/crti.o $(FILE_crtbegin)
MUSL_ENDFILES ?= $(FILE_crtend) $(MUSL_LIBC_LIBRARY_PATH)/crtn.o

GCC_CRTBEGINS := $(FILE_crtbegin)
GCC_CRTENDS := $(FILE_crtend)
