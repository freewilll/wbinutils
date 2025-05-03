#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "wld/expr.h"
#include "wld/lexer.h"
#include "wld/script.h"

static void parse_sections(void) {
    next();
    consume(TOK_LCURLY, "{");

    List *section_commands = new_list(32);
    ScriptCommand *script_command = malloc(sizeof(ScriptCommand));
    script_command->type = CMD_SECTIONS;
    script_command->sections.commands = section_commands;
    append_to_list(linker_script, script_command);

    while (1) {
        while (cur_token == TOK_SEMICOLON) next();

        if (cur_token == TOK_IDENTIFIER) {
            char *symbol = strdup(cur_identifier);
            next();

            if (cur_token == TOK_EQ) {
                next();
                Node *node = parse_expression();

                SectionsCommand *command = malloc(sizeof(SectionsCommand));
                command->type = SECTIONS_CMD_ASSIGNMENT;
                command->assignment.symbol = symbol;
                command->assignment.node = node;
                append_to_list(section_commands, command);
            }
            else
                error_in_file("Expected an assignment expression");
        }

        if (cur_token == TOK_RCURLY) break;
    }

    consume(TOK_RCURLY, "}");
}

void parse(void) {
    while (cur_token != TOK_EOF) {
        while (cur_token == TOK_SEMICOLON) next();

        if (cur_token == TOK_ENTRY) {
            next();
            consume(TOK_LPAREN, "(");
            consume(TOK_IDENTIFIER, "symbol");

            ScriptCommand *command = malloc(sizeof(ScriptCommand));
            command->type = CMD_ENTRY;
            command->entry.symbol = strdup(cur_identifier);;
            append_to_list(linker_script, command);

            consume(TOK_RPAREN, ")");
        }

        else if (cur_token == TOK_SECTIONS) {
            parse_sections();
        }

        else {
            error_in_file("Unknown command");
        }
    }
}
