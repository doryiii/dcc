#ifndef AST_H
#define AST_H

#include <stdbool.h>

typedef enum {
  TYPE_VOID,
  TYPE_INT,
  TYPE_UINT8,
  TYPE_UINT16,
  TYPE_STRUCT,
  TYPE_POINTER
} BaseType;

typedef struct TypeInfo {
  BaseType base;
  char* struct_name;        // if base == TYPE_STRUCT
  struct TypeInfo* ptr_to;  // if base == TYPE_POINTER
} TypeInfo;

typedef enum {
  AST_PROGRAM,
  AST_VAR_DECL,
  AST_STRUCT_DECL,
  AST_FUNC_DECL,
  AST_BLOCK,
  AST_IF,
  AST_WHILE,
  AST_FOR,
  AST_RETURN,
  AST_EXPR_STMT,
  AST_ASSIGN,
  AST_BINARY_OP,
  AST_UNARY_OP,
  AST_MEMBER_ACCESS,
  AST_VAR_ACCESS,
  AST_NUMBER,
  AST_CAST
} ASTNodeType;

typedef struct ASTNode {
  ASTNodeType type;
  int line;

  // For AST_VAR_DECL, AST_FUNC_DECL, AST_STRUCT_DECL
  char* name;
  TypeInfo* var_type;

  // For AST_PROGRAM, AST_BLOCK, AST_STRUCT_DECL (members), AST_FUNC_DECL
  // (params)
  struct ASTNode** children;
  int num_children;
  int capacity;

  // For AST_FUNC_DECL
  struct ASTNode* body;

  // For AST_IF
  struct ASTNode* cond;
  struct ASTNode* then_branch;
  struct ASTNode* else_branch;

  // For AST_WHILE, AST_FOR
  // cond and body reused from above
  struct ASTNode* init;
  struct ASTNode* inc;

  // For AST_RETURN, AST_EXPR_STMT, AST_UNARY_OP, AST_CAST
  struct ASTNode* expr;
  int op;  // Unary/Binary op token type (e.g., TOK_STAR, TOK_PLUS)

  // For AST_ASSIGN, AST_BINARY_OP
  struct ASTNode* left;
  struct ASTNode* right;

  // For AST_MEMBER_ACCESS
  char* member_name;

  // For AST_VAR_ACCESS
  // name reused

  // For AST_NUMBER
  int int_val;

  // For arrays in structs
  int array_size;

} ASTNode;

// Node creation
ASTNode* ast_create_node(ASTNodeType type, int line);
void ast_add_child(ASTNode* parent, ASTNode* child);
TypeInfo* ast_create_type(BaseType base);

// Memory management
void ast_free_node(ASTNode* node);
void ast_free_type(TypeInfo* type);

// Debug
void ast_print(ASTNode* node, int indent);

#endif
