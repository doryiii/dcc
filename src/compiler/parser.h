#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

void parser_init(int (*getchar_cb)(void));
void parser_cleanup(void);
ASTNode* parse_top_level_declaration();
ASTNode* parse_program(int (*getchar_cb)(void));

#endif
