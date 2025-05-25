#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "list.h"

#include "wld/expr.h"

#define LINKER_SCRIPT_COMMON_SECTION_NAME         "COMMON"
#define LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME "/DISCARD/"

// Assigmnent command, used in sections and sections output
typedef struct command_assignment  {
    char *symbol;
    Node *node;
    int provide;                 // Provide a regular symbol
    int provide_hidden;          // Provide a hidden symbol
} CommandAssignment;

// Section commands
typedef enum sections_command_type {
    SECTIONS_CMD_ASSIGNMENT = 1,    // An assignment,     e.g. . = 0x1000
    SECTIONS_CMD_OUTPUT,            // An output section, e.g. .text : { *(.text) }
} SectionsCommandType;

// Output section command
typedef struct input_section  {
    char *file_pattern;
    List *section_patterns;      // A list of strings, e.g. ".text", ".text.*"
    int keep;                    // Include empty sections
} InputSection;

// Section commands
typedef enum sections_command_input_type {
    SECTIONS_CMD_INPUT_ASSIGNMENT = 1,    // An assignment,    e.g. . = 0x1000 or PROVIDE(__foo = .)
    SECTIONS_CMD_INPUT_SECTION    = 2,    // An input section, e.g. *(.text .text.foo)
} SectionsCommandInputType;

typedef struct sections_command_output_item  {
    SectionsCommandType type;
    union {
        CommandAssignment assignment;   // SECTIONS_CMD_INPUT_ASSIGNMENT
        InputSection input_section;     // SECTIONS_CMD_INPUT_SECTION
    };
} SectionsCommandOutputItem;

typedef struct sections_command_output  {
    char *output_section_name;
    List *output_items;             // A list of SectionsCommandOutputItem
} SectionsCommandOutput;

typedef struct sections_command  {
    SectionsCommandType type;
    union {
        CommandAssignment assignment;
        SectionsCommandOutput output;
    };
} SectionsCommand;

// Script commands
typedef enum script_command_type {
    CMD_ENTRY    = 1,
    CMD_SECTIONS = 2,
} ScriptCommandType;

typedef struct script_command_entry {
    char *symbol;
} ScriptCommandEntry;

typedef struct script_command_section {
    List *commands;
} ScriptCommandSections;

typedef struct script_command {
    ScriptCommandType type;
    union {
        ScriptCommandEntry entry;
        ScriptCommandSections sections;
    };
} ScriptCommand;

extern List *linker_script;

static char *DEFAULT_LINKER_SCRIPT =
    "ENTRY(_start)"
    "SECTIONS {"
    "    . = 0x400000 + SIZEOF_HEADERS;                                          \n"
    "    . = ALIGN(CONSTANT (MAXPAGESIZE));                                      \n"
    "    .rela.plt : {                                                           \n"
    "        *(.rela.plt)                                                        \n"
    "        PROVIDE(__rela_iplt_start = .);                                     \n"
    "        *(.rela.iplt)                                                       \n"
    "        PROVIDE(__rela_iplt_end = .);                                       \n"
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
    "        __preinit_array_start = .;                                          \n"
    "        KEEP(*(.preinit_array .preinit_array.*))                            \n"
    "        __preinit_array_end = .;                                            \n"
    "     }                                                                      \n"
    "    .init_array : {                                                         \n"
    "        __init_array_start = .;                                             \n"
    "        KEEP(*(.init_array .init_array.*))                                  \n"
    "        __init_array_end = .;                                               \n"
    "    }                                                                       \n"
    "    .fini_array : {                                                         \n"
    "        __fini_array_start = .;                                             \n"
    "        KEEP(*(.fini_array .fini_array.*))                                  \n"
    "        __fini_array_end = .;                                               \n"
    "     }                                                                      \n"
    "    .got :    { *(.got)                                                    }\n"
    "    .got.plt: { *(.got.plt)                                                }\n"
    "    .data :   { *(.data .data.*)                                           }\n"
    "    .bss :    { *(.bss .bss.*) *(COMMON)                                   }\n"
    "                                                                            \n"
    "     /DISCARD/ : {                                                          \n"
    "         *(.note.GNU-stack)                                                 \n"
    "         *(.gnu_debuglink)                                                  \n"
    "         *(.gnu.lto_*)                                                      \n"
    "         *(.debug*)                                                         \n"
    "         *(.comment)                                                        \n"
    "     }                                                                      \n"
    "}                                                                           \n";

void parse_linker_scripts(List *library_paths, List *linker_scripts);

#endif
