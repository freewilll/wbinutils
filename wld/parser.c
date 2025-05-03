#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "wld/lexer.h"
#include "wld/script.h"

void parse(void) {
    while (cur_token != TOK_EOF) {
        while (cur_token == TOK_SEMICOLON) next();

        if (cur_token == TOK_ENTRY) {
            next();
            consume(TOK_LPAREN, ")");

            consume(TOK_IDENTIFIER, "symbol");

            ScriptCommand *command = malloc(sizeof(ScriptCommand));
            command->type = CMD_ENTRY;
            command->entry.symbol = strdup(cur_identifier);;
            append_to_list(linker_script, command);

            consume(TOK_RPAREN, ")");
        }
        else {
            error_in_file("Unknown command");
        }
    }
}
