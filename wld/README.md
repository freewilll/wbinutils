# WLD Linker for x86_64 Linux

This project is an ELF linker for `x86-64` linux intended for use with [wcc](https://github.com/freewilll/wcc). It can handle linking object files:
- Static and dynamic glibc libraries
- Static and dynamic musl libraries
- Object files made by the [was assembler](../was/README.md)

It can build:
- Static executables
- Dynamic PIE executables
- Shared libraries (`.so`)

All tests in [SQLite](https://www.sqlite.org/index.html)'s `tcltest` pass.

# Supported features
- glibc and musl C standard libraries
- Several relocations
- GNU symbol versions
- GNU Interactive functions (`IFUNC`)
- Thread local storage (`TLS`)
- DWARF debugging

# Building
Build

```
make wld
```

Run tests
```
make test
```

# Linker script support
WLD includes a custom linker script parser with support for commonly used constructs such as:

- `ENTRY`
- `SECTIONS`
- `INPUT`
- `GROUP`
- `AS_NEEDED`
- `PROVIDE`
- `PROVIDE_HIDDEN`
- `KEEP`
- `/DISCARD/`

It also supports expression handling used in scripts (`ALIGN`, `SIZEOF`, `SIZEOF_HEADERS`, arithmetic/comparisons/ternary).

# Relocations
The following relocations are supported:
- R_X86_64_32
- R_X86_64_32S
- R_X86_64_64
- R_X86_64_GOTPCREL
- R_X86_64_GOTPCRELX
- R_X86_64_GOTTPOFF
- R_X86_64_PC32
- R_X86_64_REX_GOTPCRELX
- R_X86_64_TPOFF32
- R_X86_64_PLT32
- R_X86_64_COPY

# Notes
WLD is intentionally focused on the needs of this toolchain, not full GNU ld compatibility.
