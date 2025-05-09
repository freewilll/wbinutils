#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "wld/expr.h"
#include "wld/lexer.h"
#include "wld/script.h"

static SectionsCommand *parse_sections_assignment(char *identifier) {
    next();
    Node *node = parse_expression();

    SectionsCommand *command = malloc(sizeof(SectionsCommand));
    command->type = SECTIONS_CMD_ASSIGNMENT;
    command->assignment.symbol = identifier;
    command->assignment.node = node;

    return command;
}

static InputSection *parse_input_section() {
    if (cur_token == TOK_KEEP) {
        next();
        consume(TOK_LPAREN, "(");
        InputSection *input_section = parse_input_section();
        input_section->keep = 1;
        consume(TOK_RPAREN, ")");
        return input_section;
    }

    InputSection *input_section = calloc(1, sizeof(InputSection));

    if (cur_token == TOK_MULTIPLY) {
        // Special case of a single *
        input_section->file_pattern = "*";
        next();
    }
    else {
        expect(TOK_IDENTIFIER, "filename pattern");
        input_section->file_pattern = strdup(cur_identifier);
        next();
    }

    input_section->section_patterns = new_list(8);

    consume(TOK_LPAREN, "(");
    while (1) {
        expect(TOK_IDENTIFIER, "input section pattern");
        append_to_list(input_section->section_patterns, strdup(cur_identifier));
        next();

        if (cur_token == TOK_RPAREN) break;
    }
    consume(TOK_RPAREN, ")");

    return input_section;
}

static SectionsCommand *parse_sections_output(List *section_commands, char *identifier) {
    next(); // Colon

    SectionsCommand *command = malloc(sizeof(SectionsCommand));
    command->type = SECTIONS_CMD_OUTPUT;
    command->output.output_section_name = identifier; // Output section name

    List *input_sections = new_list(8);;
    command->output.input_sections = input_sections;

    consume(TOK_LCURLY, "{");

    // Loop over input sections
    while (1) {
        InputSection *input_section = parse_input_section(command);
        append_to_list(command->output.input_sections, input_section);

        if (cur_token == TOK_RCURLY) break;
    }

    consume(TOK_RCURLY, "}");

    return command;
}

static void parse_sections_command(List *section_commands) {
    if (cur_token == TOK_IDENTIFIER) {
        char *identifier = strdup(cur_identifier);
        next();

        if (cur_token == TOK_EQ) {
            // Assignment
            append_to_list(section_commands, parse_sections_assignment(identifier));

        }
        else if (cur_token == TOK_COLON) {
            // Output
            append_to_list(section_commands, parse_sections_output(section_commands, identifier));
        }
        else
            error_in_file("Expected an assignment expression");
    }

    else if (cur_token == TOK_DISCARD) {
        next();
        expect(TOK_COLON, ":");
        append_to_list(section_commands, parse_sections_output(section_commands, LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME));
    }
    else
        error_in_file("Expected identifier");
}

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
        if (cur_token == TOK_RCURLY) break;
        parse_sections_command(section_commands);
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
