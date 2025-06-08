#ifdef SHARED
static char *DEFAULT_LINKER_SCRIPT_SHARED =
#else
static char *DEFAULT_LINKER_SCRIPT_STATIC =
#endif
    "ENTRY(_start)"
    "SECTIONS {"
#ifdef STATIC
    "    . = 0x400000 + SIZEOF_HEADERS;                                          \n"
#else
    "    . = SIZEOF_HEADERS;                                                     \n"
#endif
    "    . = ALIGN(CONSTANT (MAXPAGESIZE));                                      \n"

#ifdef SHARED
    "    .hash :  { *(.hash)                                                    }\n"
    "    .dynsym: { *(.dynsym)                                                  }\n"
    "    .dynstr: { *(.dynstr)                                                  }\n"
#endif
    "    .rela.plt : {                                                           \n"
    "        *(.rela.plt)                                                        \n"
#ifdef STATIC
    "        PROVIDE(__rela_iplt_start = .);                                     \n"
#endif
    "        *(.rela.iplt)                                                       \n"
#ifdef STATIC
    "        PROVIDE(__rela_iplt_end = .);                                       \n"
#endif
    "    }                                                                       \n"
    "    . = ALIGN(CONSTANT(MAXPAGESIZE));                                       \n"
    "    .init : { *(.init .init.*)                                             }\n"
    "    .plt :  { *(.plt) *(.iplt)                                             }\n"
    "    .text : { *(.text .text.*)                                             }\n"
    "    .fini : { *(.fini .fini.*)                                             }\n"
    "    PROVIDE(__etext = .);                                                   \n"
    "    PROVIDE(_etext = .);                                                    \n"
    "    PROVIDE(etext = .);                                                     \n"
    "                                                                            \n"
    "    . = ALIGN(CONSTANT (MAXPAGESIZE));                                      \n"
    "    .rodata :   { *(.rodata .rodata.*)                                     }\n"
    "    .eh_frame : { *(.eh_frame .eh_frame.*)                                 }\n"
    "                                                                            \n"
    "    . = ALIGN(CONSTANT (MAXPAGESIZE));                                      \n"
    "    .tdata : { *(.tdata .tdata.*)                                          }\n"
    "    .tbss :  { *(.tbss .tbss.*)                                            }\n"
    "    .preinit_array : {                                                      \n"
#ifdef STATIC
    "        PROVIDE_HIDDEN(__preinit_array_start = .);                          \n"
#endif
    "        KEEP(*(.preinit_array .preinit_array.*))                            \n"
#ifdef STATIC
    "        PROVIDE_HIDDEN(__preinit_array_end = .);                            \n"
#endif
    "     }                                                                      \n"
    "    .init_array : {                                                         \n"
#ifdef STATIC
    "        PROVIDE_HIDDEN(__init_array_start = .);                             \n"
#endif
    "        KEEP(*(.init_array .init_array.*))                                  \n"
#ifdef STATIC
    "        PROVIDE_HIDDEN(__init_array_end = .);                               \n"
#endif
    "    }                                                                       \n"
    "    .fini_array : {                                                         \n"
#ifdef STATIC
    "        PROVIDE_HIDDEN(__fini_array_start = .);                             \n"
#endif
    "        KEEP(*(.fini_array .fini_array.*))                                  \n"
#ifdef STATIC
    "        PROVIDE_HIDDEN(__fini_array_end = .);                               \n"
#endif
    "     }                                                                      \n"
#ifdef SHARED
    "    .dynamic :  { *(.dynamic)                                              }\n"
#endif
    "    .got :      { *(.got)                                                  }\n"
    "    .got.plt:   { *(.got.plt)                                              }\n"
    "    .data :     { *(.data .data.*)                                         }\n"
    "    .bss :      { *(.bss .bss.*) *(COMMON)                                 }\n"
    "                                                                            \n"
    "    .debug_info :   { *(.debug_info)                                       }\n"
    "    .debug_abbrev : { *(.debug_abbrev)                                     }\n"
    "    .debug_line :   { *(.debug_line)                                       }\n"
    "    .debug_str :    { *(.debug_str)                                        }\n"
    "                                                                            \n"
    "    /DISCARD/ : {                                                           \n"
    "         *(.note.GNU-stack)                                                 \n"
    "         *(.debug_info.cwd)                                                 \n"
    "         *(.gnu_debuglink)                                                  \n"
    "         *(.gnu.lto_*)                                                      \n"
    "         *(.comment)                                                        \n"
    "     }                                                                      \n"
    "}                                                                           \n";
