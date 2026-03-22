#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stdint.h>

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

typedef enum : uint8_t {
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
  struct ASTNode* first_child;
  struct ASTNode* next_sibling;

  uint16_t line;
  ASTNodeType type;

  union {
    // For AST_VAR_DECL, AST_FUNC_DECL, AST_STRUCT_DECL
    struct {
      char* name;
      TypeInfo* var_type;
      struct ASTNode* body; // For AST_FUNC_DECL
      int array_size;       // For arrays in structs
    } decl;

    // For AST_IF
    struct {
      struct ASTNode* cond;
      struct ASTNode* then_branch;
      struct ASTNode* else_branch;
    } if_stmt;

    // For AST_WHILE, AST_FOR
    struct {
      struct ASTNode* init;
      struct ASTNode* cond;
      struct ASTNode* inc;
      struct ASTNode* body;
    } loop;

    // For AST_RETURN, AST_EXPR_STMT, AST_UNARY_OP, AST_CAST
    struct {
      struct ASTNode* expr;
      int op;             // Unary/Binary op token type
      TypeInfo* var_type; // For AST_CAST
    } single_expr;

    // For AST_ASSIGN, AST_BINARY_OP
    struct {
      struct ASTNode* left;
      struct ASTNode* right;
      int op;
    } binary;

    // For AST_MEMBER_ACCESS
    struct {
      struct ASTNode* expr; // The object being accessed
      char* member_name;
    } member;

    // For AST_VAR_ACCESS
    struct {
      char* name;
    } var;

    // For AST_NUMBER
    struct {
      int int_val;
    } number;
  } as;
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
