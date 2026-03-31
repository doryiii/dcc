#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Token current_token;

static void advance_token() {
  lexer_free_token(&current_token);
  current_token = lexer_next_token();
}

static void match(TokenType type) {
  if (current_token.type == type) {
    advance_token();
  } else {
    fprintf(
        stderr, "Error at line %d: Expected %s, got %s\n", current_token.line,
        lexer_token_type_name(type), lexer_token_type_name(current_token.type)
    );
    exit(1);
  }
}

static bool check(TokenType type) { return current_token.type == type; }

static ASTNode* parse_expr();
static ASTNode* parse_statement();
static ASTNode* parse_declaration();
static TypeInfo* parse_type();

static TypeInfo* parse_type() {
  TypeInfo* t = NULL;
  if (check(TOK_INT)) {
    t = ast_create_type(TYPE_INT);
    advance_token();
  } else if (check(TOK_VOID)) {
    t = ast_create_type(TYPE_VOID);
    advance_token();
  } else if (check(TOK_UINT8_T)) {
    t = ast_create_type(TYPE_UINT8);
    advance_token();
  } else if (check(TOK_UINT16_T)) {
    t = ast_create_type(TYPE_UINT16);
    advance_token();
  } else if (check(TOK_STRUCT)) {
    advance_token();
    t = ast_create_type(TYPE_STRUCT);
    if (check(TOK_IDENTIFIER)) {
      t->struct_name = intern_string(current_token.text);
      advance_token();
    } else {
      fprintf(stderr, "Error: Expected struct name\n");
      exit(1);
    }
  } else {
    return NULL;
  }

  // Parse pointer stars
  while (check(TOK_STAR)) {
    advance_token();
    TypeInfo* ptr = ast_create_type(TYPE_POINTER);
    ptr->ptr_to = t;
    t = ptr;
  }
  return t;
}

static ASTNode* parse_primary() {
  ASTNode* node = NULL;
  if (check(TOK_NUMBER)) {
    node = ast_create_node(AST_NUMBER, current_token.line);
    node->as.number.int_val =
        strtol(current_token.text, NULL, 0);  // Handles 0x
    advance_token();
  } else if (check(TOK_IDENTIFIER)) {
    node = ast_create_node(AST_VAR_ACCESS, current_token.line);
    node->as.var.name = intern_string(current_token.text);
    advance_token();
  } else if (check(TOK_LPAREN)) {
    advance_token();

    // Is it a cast? (type)
    TypeInfo* cast_type = parse_type();
    if (cast_type) {
      match(TOK_RPAREN);
      node = ast_create_node(AST_CAST, current_token.line);
      node->as.single_expr.var_type = cast_type;
      node->as.single_expr.expr = parse_expr();
    } else {
      node = parse_expr();
      match(TOK_RPAREN);
    }
  } else {
    fprintf(
        stderr, "Error at line %d: Expected primary expression\n",
        current_token.line
    );
    exit(1);
  }
  return node;
}

static ASTNode* parse_postfix() {
  ASTNode* node = parse_primary();
  while (true) {
    if (check(TOK_DOT)) {
      advance_token();
      ASTNode* access = ast_create_node(AST_MEMBER_ACCESS, current_token.line);
      access->as.member.expr = node;
      if (check(TOK_IDENTIFIER)) {
        access->as.member.member_name = intern_string(current_token.text);
        advance_token();
      } else {
        fprintf(stderr, "Error: Expected field name after '.'\n");
        exit(1);
      }
      node = access;
    } else if (check(TOK_INC) || check(TOK_DEC)) {
      ASTNode* post = ast_create_node(AST_UNARY_OP, current_token.line);
      post->as.single_expr.op = current_token.type;
      post->as.single_expr.expr = node;
      advance_token();
      node = post;
    } else if (check(TOK_LPAREN)) {
      if (node->type != AST_VAR_ACCESS) {
        fprintf(
            stderr, "Error at line %d: Indirect function calls not supported\n",
            current_token.line
        );
        exit(1);
      }
      ASTNode* call_node = ast_create_node(AST_FUNC_CALL, current_token.line);
      call_node->as.call.name = node->as.var.name;
      ast_free_node(node);
      advance_token();
      if (!check(TOK_RPAREN)) {
        while (true) {
          ASTNode* arg = parse_expr();
          ast_add_child(call_node, arg);
          if (check(TOK_COMMA)) {
            advance_token();
          } else {
            break;
          }
        }
      }
      match(TOK_RPAREN);
      node = call_node;
    } else {
      break;
    }
  }
  return node;
}

static ASTNode* parse_unary() {
  if (check(TOK_STAR) || check(TOK_AMP) || check(TOK_INC) || check(TOK_DEC) ||
      check(TOK_MINUS) || check(TOK_NOT)) {
    ASTNode* node = ast_create_node(AST_UNARY_OP, current_token.line);
    node->as.single_expr.op = current_token.type;
    advance_token();
    node->as.single_expr.expr = parse_unary();
    return node;
  }
  return parse_postfix();
}

static ASTNode* parse_multiplicative() {
  ASTNode* left = parse_unary();
  while (check(TOK_STAR) || check(TOK_SLASH)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_unary();
    left = node;
  }
  return left;
}

static ASTNode* parse_additive() {
  ASTNode* left = parse_multiplicative();
  while (check(TOK_PLUS) || check(TOK_MINUS)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_multiplicative();
    left = node;
  }
  return left;
}

static ASTNode* parse_shift() {
  ASTNode* left = parse_additive();
  while (check(TOK_LSHIFT) || check(TOK_RSHIFT)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_additive();
    left = node;
  }
  return left;
}

static ASTNode* parse_relational() {
  ASTNode* left = parse_shift();
  while (check(TOK_LT) || check(TOK_GT) || check(TOK_LE) || check(TOK_GE)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_shift();
    left = node;
  }
  return left;
}

static ASTNode* parse_equality() {
  ASTNode* left = parse_relational();
  while (check(TOK_EQ) || check(TOK_NEQ)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_relational();
    left = node;
  }
  return left;
}

static ASTNode* parse_bitwise_and() {
  ASTNode* left = parse_equality();
  while (check(TOK_AMP)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_equality();
    left = node;
  }
  return left;
}

static ASTNode* parse_bitwise_xor() {
  ASTNode* left = parse_bitwise_and();
  while (check(TOK_BIT_XOR)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_bitwise_and();
    left = node;
  }
  return left;
}

static ASTNode* parse_bitwise_or() {
  ASTNode* left = parse_bitwise_xor();
  while (check(TOK_BIT_OR)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_bitwise_xor();
    left = node;
  }
  return left;
}

static ASTNode* parse_logical_and() {
  ASTNode* left = parse_bitwise_or();
  while (check(TOK_LOG_AND)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_bitwise_or();
    left = node;
  }
  return left;
}

static ASTNode* parse_logical_or() {
  ASTNode* left = parse_logical_and();
  while (check(TOK_LOG_OR)) {
    ASTNode* node = ast_create_node(AST_BINARY_OP, current_token.line);
    node->as.binary.op = current_token.type;
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_logical_and();
    left = node;
  }
  return left;
}

static ASTNode* parse_assignment() {
  ASTNode* left = parse_logical_or();
  if (check(TOK_ASSIGN)) {
    ASTNode* node = ast_create_node(AST_ASSIGN, current_token.line);
    node->as.binary.left = left;
    advance_token();
    node->as.binary.right = parse_assignment();  // right-associative
    return node;
  }
  return left;
}

static ASTNode* parse_expr() { return parse_assignment(); }

static ASTNode* parse_block() {
  ASTNode* node = ast_create_node(AST_BLOCK, current_token.line);
  match(TOK_LBRACE);
  while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
    TypeInfo* t = parse_type();
    if (t) {
      // Var decl
      ASTNode* decl = ast_create_node(AST_VAR_DECL, current_token.line);
      decl->as.decl.var_type = t;
      if (check(TOK_IDENTIFIER)) {
        decl->as.decl.name = intern_string(current_token.text);
        advance_token();
      } else {
        fprintf(stderr, "Error: Expected identifier in var decl\n");
        exit(1);
      }
      if (check(TOK_ASSIGN)) {
        advance_token();
        ASTNode* assign = ast_create_node(AST_ASSIGN, current_token.line);
        ASTNode* var_access =
            ast_create_node(AST_VAR_ACCESS, current_token.line);
        var_access->as.var.name = intern_string(decl->as.decl.name);
        assign->as.binary.left = var_access;
        assign->as.binary.right = parse_expr();
        ast_add_child(
            decl, assign
        );  // Store assignment in child of var decl for now
      }
      match(TOK_SEMICOLON);
      ast_add_child(node, decl);
    } else {
      ast_add_child(node, parse_statement());
    }
  }
  match(TOK_RBRACE);
  return node;
}

static ASTNode* parse_statement() {
  if (check(TOK_IF)) {
    ASTNode* node = ast_create_node(AST_IF, current_token.line);
    advance_token();
    match(TOK_LPAREN);
    node->as.if_stmt.cond = parse_expr();
    match(TOK_RPAREN);
    node->as.if_stmt.then_branch = parse_statement();
    if (check(TOK_ELSE)) {
      advance_token();
      node->as.if_stmt.else_branch = parse_statement();
    }
    return node;
  } else if (check(TOK_WHILE)) {
    ASTNode* node = ast_create_node(AST_WHILE, current_token.line);
    advance_token();
    match(TOK_LPAREN);
    node->as.loop.cond = parse_expr();
    match(TOK_RPAREN);
    node->as.loop.body = parse_statement();
    return node;
  } else if (check(TOK_FOR)) {
    ASTNode* node = ast_create_node(AST_FOR, current_token.line);
    advance_token();
    match(TOK_LPAREN);

    TypeInfo* init_type = parse_type();
    if (init_type) {
      // For loop init is a declaration
      ASTNode* decl = ast_create_node(AST_VAR_DECL, current_token.line);
      decl->as.decl.var_type = init_type;
      decl->as.decl.name = intern_string(current_token.text);
      match(TOK_IDENTIFIER);
      if (check(TOK_ASSIGN)) {
        advance_token();
        ASTNode* assign = ast_create_node(AST_ASSIGN, current_token.line);
        ASTNode* var_access =
            ast_create_node(AST_VAR_ACCESS, current_token.line);
        var_access->as.var.name = intern_string(decl->as.decl.name);
        assign->as.binary.left = var_access;
        assign->as.binary.right = parse_expr();
        ast_add_child(decl, assign);
      }
      node->as.loop.init = decl;
      match(TOK_SEMICOLON);
    } else if (!check(TOK_SEMICOLON)) {
      node->as.loop.init = parse_expr();
      match(TOK_SEMICOLON);
    } else {
      match(TOK_SEMICOLON);
    }

    if (!check(TOK_SEMICOLON)) node->as.loop.cond = parse_expr();
    match(TOK_SEMICOLON);

    if (!check(TOK_RPAREN)) node->as.loop.inc = parse_expr();
    match(TOK_RPAREN);

    node->as.loop.body = parse_statement();
    return node;
  } else if (check(TOK_RETURN)) {
    ASTNode* node = ast_create_node(AST_RETURN, current_token.line);
    advance_token();
    if (!check(TOK_SEMICOLON)) {
      node->as.single_expr.expr = parse_expr();
    }
    match(TOK_SEMICOLON);
    return node;
  } else if (check(TOK_LBRACE)) {
    return parse_block();
  } else if (check(TOK_SEMICOLON)) {
    advance_token();
    return ast_create_node(AST_BLOCK, current_token.line);  // Empty statement
  } else {
    ASTNode* node = ast_create_node(AST_EXPR_STMT, current_token.line);
    node->as.single_expr.expr = parse_expr();
    match(TOK_SEMICOLON);
    return node;
  }
}

static ASTNode* parse_declaration() {
  if (check(TOK_STRUCT)) {
    TypeInfo* t = parse_type();
    if (check(TOK_LBRACE)) {
      // Struct declaration
      ASTNode* node = ast_create_node(AST_STRUCT_DECL, current_token.line);
      node->as.decl.name = intern_string(t->struct_name);
      ast_free_type(t);
      match(TOK_LBRACE);
      while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        TypeInfo* mem_type = parse_type();
        if (!mem_type) {
          fprintf(stderr, "Error: expected type in struct\n");
          exit(1);
        }
        ASTNode* mem = ast_create_node(AST_VAR_DECL, current_token.line);
        mem->as.decl.var_type = mem_type;
        if (check(TOK_IDENTIFIER)) {
          mem->as.decl.name = intern_string(current_token.text);
          advance_token();
        } else {
          fprintf(stderr, "Error: expected identifier in struct member\n");
          exit(1);
        }

        if (check(TOK_LBRACKET)) {
          advance_token();
          if (check(TOK_NUMBER)) {
            mem->as.decl.array_size = strtol(current_token.text, NULL, 0);
            advance_token();
          }
          match(TOK_RBRACKET);
        } else {
          mem->as.decl.array_size = 1;
        }

        match(TOK_SEMICOLON);
        ast_add_child(node, mem);
      }
      match(TOK_RBRACE);
      match(TOK_SEMICOLON);
      return node;
    } else {
      fprintf(stderr, "Error: Only struct definitions supported right now\n");
      exit(1);
    }
  }

  TypeInfo* type = parse_type();
  if (!type) {
    fprintf(
        stderr, "Error at line %d: Expected declaration\n", current_token.line
    );
    exit(1);
  }

  if (check(TOK_IDENTIFIER)) {
    char* name = intern_string(current_token.text);
    advance_token();

    if (check(TOK_LPAREN)) {
      // Function declaration
      ASTNode* node = ast_create_node(AST_FUNC_DECL, current_token.line);
      node->as.decl.var_type = type;
      node->as.decl.name = name;
      match(TOK_LPAREN);

      // params
      if (check(TOK_VOID) &&
          current_token.length == 4) {  // small hack: need to peek if it's void
                                        // parameter or just a bad type.
        advance_token();
      } else {
        while (!check(TOK_RPAREN) && !check(TOK_EOF)) {
          TypeInfo* ptype = parse_type();
          ASTNode* pdecl = ast_create_node(AST_VAR_DECL, current_token.line);
          pdecl->as.decl.var_type = ptype;
          if (check(TOK_IDENTIFIER)) {
            pdecl->as.decl.name = intern_string(current_token.text);
            advance_token();
          } else {
            fprintf(stderr, "Error: expected parameter name\n");
            exit(1);
          }
          ast_add_child(node, pdecl);
          if (check(TOK_COMMA)) advance_token();
        }
      }
      match(TOK_RPAREN);
      node->as.decl.body = parse_block();
      return node;
    } else {
      // Global Var
      ASTNode* node = ast_create_node(AST_VAR_DECL, current_token.line);
      node->as.decl.var_type = type;
      node->as.decl.name = name;
      match(TOK_SEMICOLON);
      return node;
    }
  }
  fprintf(stderr, "Error: expected identifier\n");
  exit(1);
}

void parser_cleanup(void) {
  lexer_free_token(&current_token);
  lexer_cleanup();
}

void parser_init(int (*getchar_cb)(void)) {
  lexer_init(getchar_cb);
  current_token = lexer_next_token();
}

ASTNode* parse_top_level_declaration(void) {
  if (current_token.type == TOK_EOF) return NULL;
  return parse_declaration();
}

ASTNode* parse_program(int (*getchar_cb)(void)) {
  parser_init(getchar_cb);

  ASTNode* prog = ast_create_node(AST_PROGRAM, 0);
  while (current_token.type != TOK_EOF) {
    ast_add_child(prog, parse_declaration());
  }
  return prog;
}
