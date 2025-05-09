#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "list.h"

#include "wld/expr.h"

#define LINKER_SCRIPT_COMMON_SECTION_NAME         "COMMON"
#define LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME "/DISCARD/"

// Section commands
typedef enum sections_command_type {
    SECTIONS_CMD_ASSIGNMENT = 1,    // An assignment,     e.g. . = 0x1000
    SECTIONS_CMD_OUTPUT,            // An output section, e.g. .text : { *(.text) }
} SectionsCommandType;

// Assigmnent section command
typedef struct sections_command_assignment  {
    char *symbol;
    Node *node;
} SectionsCommandAssignment;

// Output section command
typedef struct input_section  {
    char *file_pattern;
    List *section_patterns;      // A list of strings, e.g. ".text", "".text.*""
    int keep;                    // Include empty sections
} InputSection;

typedef struct sections_command_output  {
    char *output_section_name;
    List *input_sections;           // A list of InputSection
} SectionsCommandOutput;

typedef struct sections_command  {
    SectionsCommandType type;
    union {
        SectionsCommandAssignment assignment;
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
    "    . = 0x400000 + SIZEOF_HEADERS;\n"
    "    . = ALIGN(CONSTANT (MAXPAGESIZE));\n"
    "    .init :           { *(.init            .init.*)                        }\n"
    "    .text :           { *(.text            .text.*)                        }\n"
    "    .fini :           { *(.fini            .fini.*)                        }\n"
    "\n"
    "    . = ALIGN(CONSTANT (MAXPAGESIZE));\n"
    "    .rodata :         { *(.rodata          .rodata.*)                      }\n"
    "    .eh_frame :       { *(.eh_frame        .eh_frame.*)                    }\n"
    "\n"
    "    . = ALIGN(CONSTANT (MAXPAGESIZE));\n"
    "    .tdata :          { *(.tdata           .tdata.*)                       }\n"
    "    .tbss :           { *(.tbss            .tbss.*)                        }\n"
    "    .preinit_array :  { KEEP( *(.preinit_array   .preinit_array.*))        }\n"
    "    .init_array :     { KEEP( *(.init_array      .init_array.*))           }\n"
    "    .fini_array :     { KEEP( *(.fini_array      .fini_array.*))           }\n"
    "    .got :            { *(.got             .got.*)                         }\n"
    "    .data :           { *(.data            .data.*)                        }\n"
    "    .bss :            { *(.bss             .bss.*) *(COMMON)               }\n"
    "\n"
    "     /DISCARD/ : {\n"
    "         *(.note.GNU-stack)\n"
    "         *(.gnu_debuglink)\n"
    "         *(.gnu.lto_*)\n"
    "         *(.debug*)\n"
    "         *(.comment)\n"
    "     }\n"
    "}\n";

void parse_linker_scripts(List *library_paths, List *linker_scripts);

#endif
