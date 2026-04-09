# WAS Assembler for x86_64 Linux

This project is an assembler for `x86-64` linux intended for use with [wcc](https://github.com/freewilll/wcc).

# Features
- Assembly based on [x86reference.xml](https://raw.githubusercontent.com/Barebit/x86reference/master/x86reference.xml)
- A single `.text` section
- Multiple data sections
- Limited expressions such as used in `.size` and the debug symbols
- Branch shortening
- Debug symbols

# Building

A python script parses `x86reference.xml` into `opcodes-generated.c`. A python virtualenv must be installed with some dependencies. To set this up:
```
cd scripts
python3 -m virtualenv venv
source venv/bin/activate
pip install -r requirements.txt
```

Build
```
make was
```

Run tests

The tests require gcc and wcc to be installed. See [wcc](https://github.com/freewilll/wcc) for how to build it. If you check out and build wcc at the same level as wbinutils, the default test path will work.

```
make test
```
