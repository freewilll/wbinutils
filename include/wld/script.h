#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "list.h"

#include "wld/expr.h"

// Section commands
typedef enum sections_command_type {
    SECTIONS_CMD_ASSIGNMENT = 1,
} SectionsCommandType;

typedef struct sections_command_assignment  {
    char *symbol;
    Node *node;
} SectionsCommandAssignment;

typedef struct sections_command  {
    SectionsCommandType type;
    union {
        SectionsCommandAssignment assignment;
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

void parse_linker_scripts(List *library_paths, List *linker_scripts);

#endif
