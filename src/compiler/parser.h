#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

void parser_init(int (*getchar_cb)(void));
ASTNode* parse_top_level_declaration(void);
ASTNode* parse_program(int (*getchar_cb)(void));

#endif
