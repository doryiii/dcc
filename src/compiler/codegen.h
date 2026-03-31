#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

void codegen_set_emit_cb(void (*cb)(const char*));
void codegen_prologue(void);
void codegen_top_level(ASTNode* node);
void codegen_cleanup(void);
void codegen(ASTNode* root);

#endif
