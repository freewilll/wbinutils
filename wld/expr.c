#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include  "error.h"
#include  "elf.h"

#include "wld/expr.h"
#include "wld/lexer.h"
#include "wld/wld.h"

#define NODE_IS_NUMERIC(node) ((node)->value && !(node)->value->symbol)
#define VALUE(value) ((value).symbol ? (value).symbol->dst_value : (value).number)

static Node *parse(int level);

static Node *make_integer_node(long value) {
    Node *node = calloc(1, sizeof(Node));
    node->value = calloc(1, sizeof(Value));
    node->value->number = value;
    return node;
}

// Make a node with a value with a symbol in it
static Node *make_symbol_node(void) {
    Node *node = calloc(1, sizeof(Node));
    node->value = calloc(1, sizeof(Value));
    node->value->symbol = get_or_add_linker_script_symbol(strdup(cur_identifier));
    return node;
}

static Node *parse_binary_expression(Node *left, Operation operation, int token) {
    Node *node;

    next();
    Node *right = parse(token);

    if (NODE_IS_NUMERIC(left) && NODE_IS_NUMERIC(right) ) {
        // Evaluate two numbers

        node = left;

        switch (operation)  {
            case OP_ADD:      node->value->number = left->value->number +  right->value->number; break;
            case OP_SUBTRACT: node->value->number = left->value->number -  right->value->number; break;
            case OP_MULTIPLY: node->value->number = left->value->number *  right->value->number; break;
            case OP_DIVIDE:   node->value->number = left->value->number /  right->value->number; break;
            case OP_EQ:       node->value->number = left->value->number == right->value->number; break;
            case OP_NE:       node->value->number = left->value->number != right->value->number; break;
            case OP_LT:       node->value->number = left->value->number <  right->value->number; break;
            case OP_GT:       node->value->number = left->value->number >  right->value->number; break;
            case OP_LE:       node->value->number = left->value->number <= right->value->number; break;
            case OP_GE:       node->value->number = left->value->number >= right->value->number; break;

            default:
                panic("Unknown operation in direct numeric evaluation %d", operation);
        }
    }

    else {
        // Create an operation node
        node = calloc(1, sizeof(Node));
        node->operation = operation;
        node->left = left;
        node->right = right;
    }

    return node;
}

static Node *parse_ternary_expression(Node *left) {
    Node *condition = left;

    Node *node = calloc(1, sizeof(Node));
    node->operation = OP_TERNARY;
    node->condition = left;
    consume(TOK_TERNARY, "?");
    node->left = parse(TOK_COMMA);
    consume(TOK_COLON, ":");
    node->right = parse(TOK_COMMA);

    return node;
}

static Node *parse_align_expression() {
    next();
    consume(TOK_LPAREN, "(");

    Node *node = calloc(1, sizeof(Node));
    node->operation = OP_ALIGN;
    Node *expr = parse(TOK_COMMA);

    // Left is an optional expression. If NULL, . is used
    // Right is the alignment

    if (cur_token == TOK_COMMA) {
        next();
        node->left = expr; // An expression to align
        node->right = parse(TOK_COMMA); // The alignment
    }
    else {
        node->right = expr; // The alignment
    }

    consume(TOK_RPAREN, ")");

    return node;
}

// Returns a malloc'd tree of nodes & values
static Node *parse(int level) {
    Node *node;

    switch (cur_token) {
        case TOK_INTEGER: {
            node = make_integer_node(cur_long);
            next();
            break;
        }

        case TOK_CONSTANT: {
            next();
            consume(TOK_LPAREN, "(");

            switch (cur_token) {
                case TOK_MAXPAGESIZE:
                    node = make_integer_node(MAXPAGESIZE);
                    next();
                    break;
                case TOK_COMMONPAGESIZE:
                    node = make_integer_node(COMMONPAGESIZE);
                    next();
                    break;
                default:
                    error_in_file("Unknown constant");

            }

            consume(TOK_RPAREN, ")");
            break;
        }

        case TOK_IDENTIFIER: {
            node = make_symbol_node();
            next();
            break;
        }

        case TOK_LPAREN:
            next();
            node = parse(TOK_PLUS);
            consume(TOK_RPAREN, ")");
            break;

        case TOK_ALIGN:
            node = parse_align_expression();
            break;

        case TOK_SIZEOF:
            next();
            consume(TOK_LPAREN, "(");
            expect(TOK_IDENTIFIER, "section name");

            node = calloc(1, sizeof(Node));
            node->operation = OP_SIZEOF;
            node->identifier = strdup(cur_identifier);
            next();
            consume(TOK_RPAREN, ")");
            break;

            case TOK_SIZEOF_HEADERS:
                next();
                node = calloc(1, sizeof(Node));
                node->operation = OP_SIZEOF_HEADERS;
                    break;

        default:
            error_in_file("Unexpected token %d in expression", cur_token);
    }

    while (cur_token >= level) {
        switch (cur_token) {
            // In order of precedence
            case TOK_MULTIPLY: node = parse_binary_expression(node, OP_MULTIPLY, TOK_MULTIPLY); break;
            case TOK_DIVIDE:   node = parse_binary_expression(node, OP_DIVIDE,   TOK_MULTIPLY); break;
            case TOK_PLUS:     node = parse_binary_expression(node, OP_ADD,      TOK_MULTIPLY); break;
            case TOK_MINUS:    node = parse_binary_expression(node, OP_SUBTRACT, TOK_MULTIPLY); break;
            case TOK_LT:       node = parse_binary_expression(node, OP_LT,       TOK_PLUS);     break;
            case TOK_GT:       node = parse_binary_expression(node, OP_GT,       TOK_PLUS);     break;
            case TOK_LE:       node = parse_binary_expression(node, OP_LE,       TOK_PLUS);     break;
            case TOK_GE:       node = parse_binary_expression(node, OP_GE,       TOK_PLUS);     break;
            case TOK_DBL_EQ:   node = parse_binary_expression(node, OP_EQ,       TOK_LT);       break;
            case TOK_NOT_EQ:   node = parse_binary_expression(node, OP_NE,       TOK_LT);       break;
            case TOK_TERNARY:  node = parse_ternary_expression(node);                           break;

            default:
                return node; // Bail once we hit something unknown
        }
    }

    return node;
}

Node *parse_expression() {
    return parse(TOK_COMMA);
}

Value evaluate_node(Node *node, RwElfFile *elf_file) {
    if (node->value) return *node->value;

    Value result = {0};
    Value left = {0};
    Value right = {0};

    if (node->left)
        left = evaluate_node(node->left, elf_file);

    if (node->right)
        right = evaluate_node(node->right, elf_file);

    switch (node->operation) {
        case OP_ADD:      result.number = VALUE(left) +  VALUE(right); break;
        case OP_SUBTRACT: result.number = VALUE(left) -  VALUE(right); break;
        case OP_MULTIPLY: result.number = VALUE(left) *  VALUE(right); break;
        case OP_DIVIDE:   result.number = VALUE(left) /  VALUE(right); break;
        case OP_EQ:       result.number = VALUE(left) == VALUE(right); break;
        case OP_NE:       result.number = VALUE(left) != VALUE(right); break;
        case OP_LT:       result.number = VALUE(left) <  VALUE(right); break;
        case OP_GT:       result.number = VALUE(left) >  VALUE(right); break;
        case OP_LE:       result.number = VALUE(left) <= VALUE(right); break;
        case OP_GE:       result.number = VALUE(left) >= VALUE(right); break;

        case OP_TERNARY:  {
            result.number = VALUE(evaluate_node(node->condition, elf_file)) ? VALUE(left) : VALUE(right);
            break;
        }

        case OP_ALIGN:  {
            if (!node->left)
                left.number = must_get_global_defined_symbol(".")->dst_value;

            uint64_t align = VALUE(right);
            uint64_t value = VALUE(left);

            result.number = ((value + align - 1) / align) * align;
            break;
        }

        case OP_SIZEOF: {
            RwSection *section = get_rw_section(elf_file, node->identifier);
            if (!section) error_in_file("Unknown section %s", node->identifier);

            result.number = section->size;
            break;
        }

        case OP_SIZEOF_HEADERS:
            result.number = headers_size(elf_file);
            break;


        default:
            panic("Unknown operation in lazy evaluation %d", node->operation);
    }

    return result;
}
