#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "wld/expr.h"
#include "wld/lexer.h"
#include "wld/script.h"

List *current_linker_script;

static SectionsCommand *parse_sections_command();
static SectionsCommandOutputItem *parse_sections_command_output_item();

// Parse functions used in a sections assignment statement like PROVIDE()
SectionsCommand *parse_sections_command_flag_function() {
    next();

    consume(TOK_LPAREN, "(");
    SectionsCommand *command = parse_sections_command();
    consume(TOK_RPAREN, ")");
    return command;
}

// Parse the right hand side of an assigment where the cur_token is the eq sign after the identifier
static CommandAssignment parse_assignment_rhs(char *identifier) {
    consume(TOK_EQ, "=");

    Node *node = parse_expression();

    CommandAssignment assignment = {0};
    assignment.name = identifier;
    assignment.node = node;

    return assignment;
}

static SectionsCommand *parse_sections_assignment(char *identifier) {
    CommandAssignment assignment = parse_assignment_rhs(identifier);
    SectionsCommand *command = malloc(sizeof(SectionsCommand));
    command->type = SECTIONS_CMD_ASSIGNMENT;
    command->assignment = assignment;

    return command;
}

// Parse functions used in a sections output statement like KEEP()
SectionsCommandOutputItem *parse_flag_output_function(void) {
    next();
    consume(TOK_LPAREN, "(");
    SectionsCommandOutputItem *sections_command_output_item = parse_sections_command_output_item();
    consume(TOK_RPAREN, ")");
    return sections_command_output_item;
}

static SectionsCommandOutputItem *parse_sections_command_output_item_flag_function() {
    next();

    consume(TOK_LPAREN, "(");
    SectionsCommandOutputItem *output_item = parse_sections_command_output_item();
    consume(TOK_RPAREN, ")");
    return output_item;
}

static SectionsCommandOutputItem *parse_sections_command_output_item() {
    if (cur_token == TOK_KEEP) {
        SectionsCommandOutputItem *sections_command_output_item = parse_flag_output_function();
        sections_command_output_item->input_section.keep = 1;
        return sections_command_output_item;
    }
    else if (cur_token == TOK_PROVIDE) {
        // PROVIDE(assignment)

        SectionsCommandOutputItem *output_item = parse_sections_command_output_item_flag_function();
        output_item->assignment.provide = 1;
        return output_item;
    }
    else if (cur_token == TOK_PROVIDE_HIDDEN) {
        // PROVIDE(assignment)

        SectionsCommandOutputItem *output_item = parse_sections_command_output_item_flag_function();
        output_item->assignment.provide_hidden = 1;
        return output_item;
    }

    char *identifier;

    if (cur_token == TOK_MULTIPLY) {
        // Special case of a single *
        identifier = "*";
        next();
    }
    else {
        expect(TOK_IDENTIFIER, "identifier or filename pattern");
        identifier = strdup(cur_identifier);
        next();
    }

    if (cur_token == TOK_LPAREN) {
        // Input section

        ScriptInputSection input_section = {0};
        input_section.file_pattern = identifier;

        // Input section pattern
        input_section.section_patterns = new_list(8);

        consume(TOK_LPAREN, "(");
        while (1) {
            expect(TOK_IDENTIFIER, "input section pattern");
            append_to_list(input_section.section_patterns, strdup(cur_identifier));
            next();

            if (cur_token == TOK_RPAREN) break;
        }
        consume(TOK_RPAREN, ")");

        SectionsCommandOutputItem *output_item = malloc(sizeof(SectionsCommandOutputItem));
        output_item->type = SECTIONS_CMD_INPUT_SECTION;
        output_item->input_section = input_section;

        return output_item;

    }
    else if (cur_token == TOK_EQ) {
        // Assignment

        SectionsCommandOutputItem *output_item = malloc(sizeof(SectionsCommandOutputItem));
        output_item->type = SECTIONS_CMD_ASSIGNMENT;
        output_item->assignment = parse_assignment_rhs(identifier);
        return output_item;
    }
    else
        error_in_file("Unable to parse section");
}

static SectionsCommand *parse_sections_output(char *identifier) {
    next(); // Colon

    SectionsCommand *command = malloc(sizeof(SectionsCommand));
    command->type = SECTIONS_CMD_OUTPUT;
    command->output.output_section_name = identifier; // Output section name

    List *output_items = new_list(8);
    command->output.output_items = output_items;

    consume(TOK_LCURLY, "{");

    // Loop over output items
    while (1) {
        SectionsCommandOutputItem *output_item = parse_sections_command_output_item(command);
        append_to_list(output_items, output_item);

        while (cur_token == TOK_SEMICOLON) next();

        if (cur_token == TOK_RCURLY) break;
    }

    consume(TOK_RCURLY, "}");

    return command;
}

static SectionsCommand *parse_sections_command() {
    if (cur_token == TOK_PROVIDE) {
        SectionsCommand *command = parse_sections_command_flag_function();
        command->assignment.provide = 1;
        return command;
    }

    if (cur_token == TOK_PROVIDE_HIDDEN) {
        SectionsCommand *command = parse_sections_command_flag_function();
        command->assignment.provide_hidden = 1;
        return command;
    }

    if (cur_token == TOK_IDENTIFIER) {
        char *identifier = strdup(cur_identifier);
        next();

        if (cur_token == TOK_EQ) {
            // Assignment
            SectionsCommand *command = parse_sections_assignment(identifier);
            return command;
        }
        else if (cur_token == TOK_COLON) {
            // Output
            SectionsCommand *command = parse_sections_output(identifier);
            return command;
        }
        else
            error_in_file("Expected an assignment expression");
    }

    else if (cur_token == TOK_DISCARD) {
        next();
        expect(TOK_COLON, ":");
        SectionsCommand *command = parse_sections_output(LINKER_SCRIPT_DISCARD_OUTPUT_SECTION_NAME);
        return command;
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
    append_to_list(current_linker_script, script_command);

    while (1) {
        while (cur_token == TOK_SEMICOLON) next();
        if (cur_token == TOK_RCURLY) break;
        SectionsCommand *command = parse_sections_command(section_commands);
        append_to_list(section_commands, command);
    }

    consume(TOK_RCURLY, "}");
}

static void parse_group(void) {
    next();
    consume(TOK_LPAREN, "(");

    List *filenames = new_list(8);

    while (1) {
        if (cur_token == TOK_RPAREN || cur_token == TOK_EOF) break;

        if (cur_token == TOK_FILENAME)
            append_to_list(filenames, strdup(cur_identifier));

        consume(TOK_FILENAME, "filename");

        while (cur_token == TOK_COMMA) next();
    }

    consume(TOK_RPAREN, ")");

    ScriptCommand *command = malloc(sizeof(ScriptCommand));
    command->type = CMD_GROUP;
    command->group.filenames = filenames;
    append_to_list(current_linker_script, command);
}

// Parse a linker script, returning a list of commands
List *parse(void) {
    current_linker_script = new_list(1);

    while (cur_token != TOK_EOF) {
        while (cur_token == TOK_SEMICOLON) next();

        if (cur_token == TOK_ENTRY) {
            next();
            consume(TOK_LPAREN, "(");
            consume(TOK_IDENTIFIER, "symbol");

            ScriptCommand *command = malloc(sizeof(ScriptCommand));
            command->type = CMD_ENTRY;
            command->entry.symbol = strdup(cur_identifier);;
            append_to_list(current_linker_script, command);

            consume(TOK_RPAREN, ")");
        }

        else if (cur_token == TOK_SECTIONS) {
            parse_sections();
        }

        else if (cur_token == TOK_OUTPUT_FORMAT) {
            // Ignore OUTPUT_FORMAT(...)
            while (cur_token != TOK_EOF && cur_token != TOK_RPAREN) next();
            if (cur_token == TOK_RPAREN) next();
        }

        else if (cur_token == TOK_GROUP) {
            parse_group();
        }

        else {
            error_in_file("Unknown command");
        }
    }

    return current_linker_script;
}
