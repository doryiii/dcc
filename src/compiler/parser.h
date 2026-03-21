#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

ASTNode* parse_program(int (*getchar_cb)(void));

#endif
