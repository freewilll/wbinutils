# Assembler and Linker for x86_64 Linux

This repository consists of two related projects:
- The [WAS assembler](was/README.md)
- The [WLD linker](wld/README.md)

They work together with the [WCC compiler](https://github.com/freewilll/wcc) to make a complete toolchain.

The toolchain has been developed and tested on Ubuntu 20.04.3 LTS. Other linux distributions may not work.

The toolchain is able to create the following:
- Shared libraries
- Static executables
- Dynamic position independent (PIE) executables

The glibc and musl C libraries are both supported.

See the individual READMEs for more details.
