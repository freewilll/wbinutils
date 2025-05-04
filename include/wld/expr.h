#ifndef _EXPR_H
#define _EXPR_H

#include "wld/symbols.h"

typedef enum operation {
    OP_ADD       = 1,
    OP_SUBTRACT  = 2,
    OP_MULTIPLY  = 3,
    OP_DIVIDE    = 4,
    OP_EQ        = 5,
    OP_NE        = 6,
    OP_LT        = 7,
    OP_GT        = 8,
    OP_LE        = 9,
    OP_GE        = 10,
    OP_TERNARY   = 11,
    OP_ALIGN     = 12,
    OP_SIZEOF    = 13
} Operation;

typedef struct value {
    Symbol *symbol;  // Optional symbol
    uint64_t number; // Optional number
} Value;

typedef struct node Node;

typedef struct node {
    Value *value;        // Optional value
    Operation operation; // Optional operation
    Node *left;          // Optional expression
    Node *right;         // Optional expression
    Node *condition;     // Optional expression, used in ternary
    char *identifier;    // Optional identifier
} Node;

Node *parse_expression(void);
Value evaluate_node(Node *node, RwElfFile *elf_file);

#endif
