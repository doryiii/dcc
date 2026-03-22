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
  if (!parent->first_child) {
    parent->first_child = child;
  } else {
    ASTNode* current = parent->first_child;
    while (current->next_sibling) {
      current = current->next_sibling;
    }
    current->next_sibling = child;
  }
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

  // Free children (intrusive linked list)
  ASTNode* child = node->first_child;
  while (child) {
    ASTNode* next = child->next_sibling;
    ast_free_node(child);
    child = next;
  }

  // Free node specific data
  switch (node->type) {
    case AST_VAR_DECL:
    case AST_FUNC_DECL:
    case AST_STRUCT_DECL:
      if (node->as.decl.name) free(node->as.decl.name);
      if (node->as.decl.var_type) ast_free_type(node->as.decl.var_type);
      if (node->as.decl.body) ast_free_node(node->as.decl.body);
      break;

    case AST_IF:
      if (node->as.if_stmt.cond) ast_free_node(node->as.if_stmt.cond);
      if (node->as.if_stmt.then_branch) ast_free_node(node->as.if_stmt.then_branch);
      if (node->as.if_stmt.else_branch) ast_free_node(node->as.if_stmt.else_branch);
      break;

    case AST_WHILE:
    case AST_FOR:
      if (node->as.loop.init) ast_free_node(node->as.loop.init);
      if (node->as.loop.cond) ast_free_node(node->as.loop.cond);
      if (node->as.loop.inc) ast_free_node(node->as.loop.inc);
      if (node->as.loop.body) ast_free_node(node->as.loop.body);
      break;

    case AST_RETURN:
    case AST_EXPR_STMT:
    case AST_UNARY_OP:
    case AST_CAST:
      if (node->as.single_expr.expr) ast_free_node(node->as.single_expr.expr);
      if (node->as.single_expr.var_type) ast_free_type(node->as.single_expr.var_type);
      break;

    case AST_ASSIGN:
    case AST_BINARY_OP:
      if (node->as.binary.left) ast_free_node(node->as.binary.left);
      if (node->as.binary.right) ast_free_node(node->as.binary.right);
      break;

    case AST_MEMBER_ACCESS:
      if (node->as.member.expr) ast_free_node(node->as.member.expr);
      if (node->as.member.member_name) free(node->as.member.member_name);
      break;

    case AST_VAR_ACCESS:
      if (node->as.var.name) free(node->as.var.name);
      break;

    case AST_PROGRAM:
    case AST_BLOCK:
    case AST_NUMBER:
      // Handled by generic children or no dynamic memory
      break;
  }

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
      printf("VarDecl: %s %s\n", type_to_string(node->as.decl.var_type), node->as.decl.name);
      break;
    case AST_STRUCT_DECL:
      printf("StructDecl: %s\n", node->as.decl.name);
      break;
    case AST_FUNC_DECL:
      printf("FuncDecl: %s %s\n", type_to_string(node->as.decl.var_type), node->as.decl.name);
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
      printf("BinaryOp: %d\n", node->as.binary.op);
      break;
    case AST_UNARY_OP:
      printf("UnaryOp: %d\n", node->as.single_expr.op);
      break;
    case AST_MEMBER_ACCESS:
      printf("MemberAccess: .%s\n", node->as.member.member_name);
      break;
    case AST_VAR_ACCESS:
      printf("VarAccess: %s\n", node->as.var.name);
      break;
    case AST_NUMBER:
      printf("Number: %d\n", node->as.number.int_val);
      break;
    case AST_CAST:
      printf("Cast: %s\n", type_to_string(node->as.single_expr.var_type));
      break;
  }

  ASTNode* child = node->first_child;
  while (child) {
    ast_print(child, indent + 1);
    child = child->next_sibling;
  }

  if (node->type == AST_IF) {
    if (node->as.if_stmt.cond) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Cond:\n");
      ast_print(node->as.if_stmt.cond, indent + 2);
    }
    if (node->as.if_stmt.then_branch) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Then:\n");
      ast_print(node->as.if_stmt.then_branch, indent + 2);
    }
    if (node->as.if_stmt.else_branch) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Else:\n");
      ast_print(node->as.if_stmt.else_branch, indent + 2);
    }
  }

  if (node->type == AST_WHILE || node->type == AST_FOR) {
    if (node->as.loop.init) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Init:\n");
      ast_print(node->as.loop.init, indent + 2);
    }
    if (node->as.loop.cond) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Cond:\n");
      ast_print(node->as.loop.cond, indent + 2);
    }
    if (node->as.loop.inc) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Inc:\n");
      ast_print(node->as.loop.inc, indent + 2);
    }
    if (node->as.loop.body) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Body:\n");
      ast_print(node->as.loop.body, indent + 2);
    }
  }

  if (node->type == AST_RETURN || node->type == AST_EXPR_STMT || node->type == AST_UNARY_OP || node->type == AST_CAST) {
    if (node->as.single_expr.expr) ast_print(node->as.single_expr.expr, indent + 1);
  }

  if (node->type == AST_ASSIGN || node->type == AST_BINARY_OP) {
    if (node->as.binary.left) ast_print(node->as.binary.left, indent + 1);
    if (node->as.binary.right) ast_print(node->as.binary.right, indent + 1);
  }

  if (node->type == AST_MEMBER_ACCESS) {
    if (node->as.member.expr) ast_print(node->as.member.expr, indent + 1);
  }

  if (node->type == AST_FUNC_DECL) {
    if (node->as.decl.body) {
      for (int i = 0; i < indent + 1; i++) printf("  ");
      printf("Body:\n");
      ast_print(node->as.decl.body, indent + 2);
    }
  }
}
