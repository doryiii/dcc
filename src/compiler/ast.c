#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ASTNode* ast_create_node(ASTNodeType type, int line) {
  ASTNode* node = (ASTNode*)calloc(1, sizeof(ASTNode));
  node->type = type;
  node->line = line;
  return node;
}

void ast_add_child(ASTNode* parent, ASTNode* child) {
  if (parent->capacity == 0) {
    parent->capacity = 4;
    parent->children = (ASTNode**)malloc(sizeof(ASTNode*) * parent->capacity);
  } else if (parent->num_children == parent->capacity) {
    parent->capacity *= 2;
    parent->children = (ASTNode**)realloc(
        parent->children, sizeof(ASTNode*) * parent->capacity
    );
  }
  parent->children[parent->num_children++] = child;
}

TypeInfo* ast_create_type(BaseType base) {
  TypeInfo* t = (TypeInfo*)calloc(1, sizeof(TypeInfo));
  t->base = base;
  return t;
}

void ast_free_type(TypeInfo* type) {
  if (!type) return;
  if (type->struct_name) free(type->struct_name);
  if (type->ptr_to) ast_free_type(type->ptr_to);
  free(type);
}

void ast_free_node(ASTNode* node) {
  if (!node) return;

  if (node->name) free(node->name);
  if (node->member_name) free(node->member_name);
  if (node->var_type) ast_free_type(node->var_type);

  for (int i = 0; i < node->num_children; i++) {
    ast_free_node(node->children[i]);
  }
  if (node->children) free(node->children);

  if (node->body) ast_free_node(node->body);
  if (node->cond) ast_free_node(node->cond);
  if (node->then_branch) ast_free_node(node->then_branch);
  if (node->else_branch) ast_free_node(node->else_branch);
  if (node->init) ast_free_node(node->init);
  if (node->inc) ast_free_node(node->inc);
  if (node->expr) ast_free_node(node->expr);
  if (node->left) ast_free_node(node->left);
  if (node->right) ast_free_node(node->right);

  free(node);
}

static const char* type_to_string(TypeInfo* t) {
  if (!t) return "unknown";
  switch (t->base) {
    case TYPE_VOID:
      return "void";
    case TYPE_INT:
      return "int";
    case TYPE_UINT8:
      return "uint8_t";
    case TYPE_UINT16:
      return "uint16_t";
    case TYPE_STRUCT:
      return t->struct_name ? t->struct_name : "struct";
    case TYPE_POINTER:
      return "pointer";
  }
  return "unknown";
}

void ast_print(ASTNode* node, int indent) {
  if (!node) return;
  for (int i = 0; i < indent; i++) printf("  ");

  switch (node->type) {
    case AST_PROGRAM:
      printf("Program\n");
      break;
    case AST_VAR_DECL:
      printf("VarDecl: %s %s\n", type_to_string(node->var_type), node->name);
      break;
    case AST_STRUCT_DECL:
      printf("StructDecl: %s\n", node->name);
      break;
    case AST_FUNC_DECL:
      printf("FuncDecl: %s %s\n", type_to_string(node->var_type), node->name);
      break;
    case AST_BLOCK:
      printf("Block\n");
      break;
    case AST_IF:
      printf("If\n");
      break;
    case AST_WHILE:
      printf("While\n");
      break;
    case AST_FOR:
      printf("For\n");
      break;
    case AST_RETURN:
      printf("Return\n");
      break;
    case AST_EXPR_STMT:
      printf("ExprStmt\n");
      break;
    case AST_ASSIGN:
      printf("Assign\n");
      break;
    case AST_BINARY_OP:
      printf("BinaryOp: %d\n", node->op);
      break;
    case AST_UNARY_OP:
      printf("UnaryOp: %d\n", node->op);
      break;
    case AST_MEMBER_ACCESS:
      printf("MemberAccess: .%s\n", node->member_name);
      break;
    case AST_VAR_ACCESS:
      printf("VarAccess: %s\n", node->name);
      break;
    case AST_NUMBER:
      printf("Number: %d\n", node->int_val);
      break;
    case AST_CAST:
      printf("Cast: %s\n", type_to_string(node->var_type));
      break;
  }

  for (int i = 0; i < node->num_children; i++) {
    ast_print(node->children[i], indent + 1);
  }

  if (node->cond) {
    for (int i = 0; i < indent + 1; i++) {
      printf("  ");
    }
    printf("Cond:\n");
    ast_print(node->cond, indent + 2);
  }
  if (node->then_branch) {
    for (int i = 0; i < indent + 1; i++) {
      printf("  ");
    }
    printf("Then:\n");
    ast_print(node->then_branch, indent + 2);
  }
  if (node->else_branch) {
    for (int i = 0; i < indent + 1; i++) {
      printf("  ");
    }
    printf("Else:\n");
    ast_print(node->else_branch, indent + 2);
  }
  if (node->init) {
    for (int i = 0; i < indent + 1; i++) {
      printf("  ");
    }
    printf("Init:\n");
    ast_print(node->init, indent + 2);
  }
  if (node->inc) {
    for (int i = 0; i < indent + 1; i++) {
      printf("  ");
    }
    printf("Inc:\n");
    ast_print(node->inc, indent + 2);
  }
  if (node->expr) ast_print(node->expr, indent + 1);
  if (node->left) ast_print(node->left, indent + 1);
  if (node->right) ast_print(node->right, indent + 1);
  if (node->body) {
    for (int i = 0; i < indent + 1; i++) {
      printf("  ");
    }
    printf("Body:\n");
    ast_print(node->body, indent + 2);
  }
}
