#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

void codegen_set_emit_cb(void (*cb)(const char*));
void codegen(ASTNode* root);

#endif
