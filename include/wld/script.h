#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "list.h"

typedef enum script_command_type  {
    CMD_ENTRY = 1,
} ScriptCommandType;

typedef struct script_command_entry  {
    char *symbol;
} ScriptCommandEntry;

typedef struct script_command  {
    ScriptCommandType type;
    union {
        ScriptCommandEntry entry;
    };
} ScriptCommand;

extern List *linker_script;

void parse_linker_scripts(List *library_paths, List *linker_scripts);

#endif
