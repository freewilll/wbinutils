#ifndef _LEXER_H
#define _LEXER_H

#define MAX_IDENTIFIER_SIZE           1024
#define MAX_STRING_LITERAL_SIZE       4095

// Tokens in order of precedence
enum {
    TOK_EOF = 1,
    TOK_EOL,
    TOK_INTEGER,
    TOK_STRING_LITERAL,
    TOK_LABEL,
    TOK_IDENTIFIER,
    TOK_FILENAME,
    TOK_OUTPUT_FORMAT,
    TOK_GROUP,
    TOK_AS_NEEDED,
    TOK_ENTRY,
    TOK_SECTIONS,
    TOK_CONSTANT,
    TOK_MAXPAGESIZE,
    TOK_COMMONPAGESIZE,
    TOK_ALIGN,
    TOK_SIZEOF,
    TOK_SIZEOF_HEADERS,
    TOK_KEEP,
    TOK_PROVIDE,
    TOK_PROVIDE_HIDDEN,
    TOK_DISCARD,
    TOK_SEMICOLON,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_RCURLY,
    TOK_LCURLY,
    TOK_COMMA,
    TOK_EQ,
    TOK_TERNARY,
    TOK_COLON,
    TOK_OR,
    TOK_AND,
    TOK_XOR,
    TOK_DBL_EQ,
    TOK_NOT_EQ,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
};

extern int cur_token;                       // Current token
extern char *cur_identifier;                // Current identifier
extern long cur_long;                       // Current integer

void free_lexer(void);
void init_lexer(const char *filename);
void init_lexer_from_string(char *string);
void next(void);
void expect(int token, char *what);
void consume(int token, char *what);

#endif
