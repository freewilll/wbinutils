#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "list.h"

#include "default.ld.h"
#include "default-shared.ld.h"

#define LINKER_SCRIPT_COMMON_SECTION_NAME         "COMMON"
#define LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME "/DISCARD/"

typedef struct node Node;

// Assigmnent command, used in sections and sections output
typedef struct command_assignment  {
    char *name;
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
typedef struct script_input_section  {
    char *file_pattern;
    List *section_patterns;      // A list of strings, e.g. ".text", ".text.*"
    int keep;                    // Include empty sections
} ScriptInputSection;

// Section commands
typedef enum sections_command_input_type {
    SECTIONS_CMD_INPUT_ASSIGNMENT = 1,    // An assignment,    e.g. . = 0x1000 or PROVIDE(__foo = .)
    SECTIONS_CMD_INPUT_SECTION    = 2,    // An input section, e.g. *(.text .text.foo)
} SectionsCommandInputType;

typedef struct sections_command_output_item  {
    SectionsCommandType type;
    union {
        CommandAssignment assignment;       // SECTIONS_CMD_INPUT_ASSIGNMENT
        ScriptInputSection input_section;   // SECTIONS_CMD_INPUT_SECTION
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
    CMD_GROUP = 3,
} ScriptCommandType;

typedef struct script_command_entry {
    char *symbol;
} ScriptCommandEntry;

typedef struct script_command_section {
    List *commands;
} ScriptCommandSections;

typedef struct input_group_item {
    char *filename;
    int as_needed;
} InputGroupItem;

typedef struct script_command_group {
    List *input_group_items;
} ScriptCommandGroup;

typedef struct script_command {
    ScriptCommandType type;
    union {
        ScriptCommandEntry entry;
        ScriptCommandSections sections;
        ScriptCommandGroup group;
    };
} ScriptCommand;

char *default_linker_script(OutputElfFile *output_elf_file);
void parse_linker_scripts(OutputElfFile *output_elf_file, List *library_paths, List *linker_scripts);

#endif
