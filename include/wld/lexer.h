#ifndef _LEXER_H
#define _LEXER_H

#define MAX_IDENTIFIER_SIZE           1024
#define MAX_STRING_LITERAL_SIZE       4095

enum {
    TOK_EOF = 1,
    TOK_EOL,
    TOK_INTEGER,
    TOK_STRING_LITERAL,
    TOK_LABEL,
    TOK_IDENTIFIER,
    TOK_ENTRY,
    TOK_DOT_SYMBOL,
    TOK_SEMICOLON,
    TOK_RPAREN,
    TOK_LPAREN,
    TOK_COMMA,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
};

extern int cur_token;                       // Current token
extern char *cur_identifier;                // Current identifier
extern long cur_long;                       // Current integer

void free_lexer(void);
void init_lexer(char *filename);
void init_lexer_from_string(char *string);
void next(void);
void expect(int token, char *what);
void consume(int token, char *what);

#endif
