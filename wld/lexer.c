#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "error.h"

#include "wld/lexer.h"

static char *input;             // Input file data
static char *input_end;         // Input file data
static char *ip;                // Input pointer to currently lexed char.

char *cur_filename;
int cur_line;                       // Current line

int cur_token;                      // Current token
char *cur_identifier;               // Current identifier
long cur_long;                      // Current integer

void free_lexer(void) {
    free(cur_identifier);
    free(input);
}

static void start_lexer(void) {
    ip = input;
    cur_line = 1;
    cur_identifier = malloc(MAX_IDENTIFIER_SIZE);

    set_error_line(1);

    next();
}

void init_lexer(char *filename) {
    cur_filename = filename;
    set_error_filename(filename);

    FILE *f  = fopen(filename, "r");

    if (f == 0) {
        perror(filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    int input_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    input = malloc(input_size + 1);
    int read = fread(input, 1, input_size, f);
    if (read != input_size) {
        printf("Unable to read input file\n");
        exit(1);
    }

    input[input_size] = 0;
    input_end = input + input_size;
    fclose(f);

    start_lexer();
}

void init_lexer_from_string(char *string) {
    input = string;
    input_end = input + strlen(string);

    start_lexer();
}

static void skip_whitespace(void) {
    while (ip < input_end) {
        if (*ip == '\n') {
            cur_line++;
            set_error_line(cur_line);
        }

        if (*ip == ' ' || *ip == '\t' || *ip == '\f' || *ip == '\v' || *ip == '\n')
            ip++;
        else
            return;
    }
}

static void skip_comments(void) {
    if (ip + 1 < input_end && ip[0] == '/' && ip[1] == '*') {
        ip += 2;
        while (ip + 1 < input_end && ip[0] != '*' && ip[1] != '/') ip++;
        if (ip == input_end) error_in_file("Did not find closing comment");
        ip += 2;
    }
}

static void lex_octal_literal(void) {
    cur_long = 0;
    int c = 0;
    while (*ip >= '0' && *ip <= '7') {
        cur_long = cur_long * 8 + *ip - '0';
        ip++;
        c++;
        if (c == 3) break;
    }
}

static void lex_integer(void) {
    cur_token = TOK_INTEGER;

    int is_octal = 0;
    int is_decimal = 0;
    int is_hex = 0;
    int base;

    if (*ip == '0') {
        if (ip[1] == 'x') {
            ip += 2;
            is_hex = 1;
            base = 16;
        }
        else {
            is_octal = 1;
            ip += 1;
            base = 8;
        }
    }
    else {
        is_decimal = 1;
        base = 10;
    }

    cur_long = 0;
    while (
        ip < input_end &&
            (is_octal && (*ip >= '0' && *ip <= '7')) ||
            (is_decimal && (*ip >= '0' && *ip <= '9')) ||
            (is_hex && ((*ip >= '0' && *ip <= '9') || (*ip >= 'a' && *ip <= 'f') || (*ip >= 'A' && *ip <= 'Z')))
        ) {

        int digit = is_hex
            ? (
                *ip >= 'a'
                ? *ip - 'a' + 10
                : *ip >= 'A'
                    ? *ip - 'A' + 10
                : *ip - '0'
              )
            : *ip - '0';

        cur_long = cur_long * base + digit;

        ip++;
    }
}

// Lexer. Lex a next token or TOK_EOF if the file is ended
void next(void) {
    while (ip < input_end) {
        skip_whitespace();
        skip_comments();
        skip_whitespace();

        if (ip >= input_end) break;

        char c1 = ip[0];
        char c2 = ip[1];

             if (c1 == ';'  )  { ip += 1;  cur_token = TOK_SEMICOLON; }
        else if (c1 == '('  )  { ip += 1;  cur_token = TOK_LPAREN;    }
        else if (c1 == ')'  )  { ip += 1;  cur_token = TOK_RPAREN;    }
        else if (c1 == ','  )  { ip += 1;  cur_token = TOK_COMMA;     }
        else if (c1 == '+'  )  { ip += 1;  cur_token = TOK_PLUS;      }
        else if (c1 == '-'  )  { ip += 1;  cur_token = TOK_MINUS;     }
        else if (c1 == '*'  )  { ip += 1;  cur_token = TOK_MULTIPLY;  }
        else if (c1 == '/'  )  { ip += 1;  cur_token = TOK_DIVIDE;    }

        // Newline
        else if (c1 == '\n') {
            ip += 1;
            cur_token = TOK_EOL;
        }

        // Decimal literal
        else if (c1 >= '0' && c1 <= '9') {
            lex_integer();
        }

        // Identifier
        else if (((c1 >= 'a' && c1 <= 'z') || (c1 >= 'A' && c1 <= 'Z') || c1 == '_' || c1 == '.')) {
            int j = 0;
            while (
                    ((*ip >= 'a' && *ip <= 'z') ||
                        (*ip >= 'A' && *ip <= 'Z') ||
                        (*ip >= '0' && *ip <= '9') ||
                        (*ip == '_' || *ip == '-' ||
                        (*ip == '.'))) && ip < input_end) {

                if (j == MAX_IDENTIFIER_SIZE) panic("Exceeded maximum identifier size %d", MAX_IDENTIFIER_SIZE);
                cur_identifier[j] = *ip;
                j++;
                ip++;
            }

            if (!j) panic("cur_identifier is unexpectedly empty");

            cur_identifier[j] = 0;
            cur_token = TOK_IDENTIFIER;

            if (!strcmp(cur_identifier, "ENTRY")) { cur_token = TOK_ENTRY; }
        }

        else
            error_in_file("Unknown token %c (%d)", *ip, *ip);

        return;
    }

    cur_token = TOK_EOF;
}

void expect(int token, char *what) {
    if (cur_token != token) error_in_file("Expected %s", what);
}

void consume(int token, char *what) {
    expect(token, what);
    next();
}
