#include <stdio.h>
#include <string.h>

#include "lexer.h"

static const char* test_src;
static int test_getchar() {
  return *test_src ? (unsigned char)*test_src++ : EOF;
}

int test_lexer_basic() {
  const char* source = "int main() { return 1; }";
  test_src = source;
  lexer_init(test_getchar);

  TokenType expected[] = {TOK_INT,    TOK_IDENTIFIER, TOK_LPAREN, TOK_RPAREN,
                          TOK_LBRACE, TOK_RETURN,     TOK_NUMBER, TOK_SEMICOLON,
                          TOK_RBRACE, TOK_EOF};

  for (int i = 0; i < 10; i++) {
    Token t = lexer_next_token();
    if (t.type != expected[i]) {
      fprintf(
          stderr, "FAIL: test_lexer_basic index %d: expected %s, got %s\n", i,
          lexer_token_type_name(expected[i]), lexer_token_type_name(t.type)
      );
      lexer_free_token(&t);
      return 1;
    }
    lexer_free_token(&t);
  }
  printf("PASS: test_lexer_basic\n");
  return 0;
}

int test_lexer_operators() {
  const char* source =
      "+ - * / = == != < > <= >= << >> & | ^ && || ! ++ -- . [ ]";
  test_src = source;
  lexer_init(test_getchar);

  TokenType expected[] = {TOK_PLUS,    TOK_MINUS,  TOK_STAR,     TOK_SLASH,
                          TOK_ASSIGN,  TOK_EQ,     TOK_NEQ,      TOK_LT,
                          TOK_GT,      TOK_LE,     TOK_GE,       TOK_LSHIFT,
                          TOK_RSHIFT,  TOK_AMP,    TOK_BIT_OR,   TOK_BIT_XOR,
                          TOK_LOG_AND, TOK_LOG_OR, TOK_NOT,      TOK_INC,
                          TOK_DEC,     TOK_DOT,    TOK_LBRACKET, TOK_RBRACKET,
                          TOK_EOF};

  for (int i = 0; i < 25; i++) {
    Token t = lexer_next_token();
    if (t.type != expected[i]) {
      fprintf(
          stderr, "FAIL: test_lexer_operators index %d: expected %s, got %s\n",
          i, lexer_token_type_name(expected[i]), lexer_token_type_name(t.type)
      );
      lexer_free_token(&t);
      return 1;
    }
    lexer_free_token(&t);
  }
  printf("PASS: test_lexer_operators\n");
  return 0;
}

int test_lexer_char_literal() {
  const char* source = "uint8_t a = 'a'; '\\n'";
  test_src = source;
  lexer_init(test_getchar);

  TokenType expected[] = {
      TOK_UINT8_T, TOK_IDENTIFIER, TOK_ASSIGN, TOK_NUMBER, TOK_SEMICOLON,
      TOK_NUMBER, TOK_EOF
  };

  for (int i = 0; i < 7; i++) {
    Token t = lexer_next_token();
    if (t.type != expected[i]) {
      fprintf(
          stderr, "FAIL: test_lexer_char_literal index %d: expected %s, got %s\n",
          i, lexer_token_type_name(expected[i]), lexer_token_type_name(t.type)
      );
      lexer_free_token(&t);
      return 1;
    }
    lexer_free_token(&t);
  }
  printf("PASS: test_lexer_char_literal\n");
  return 0;
}

int test_lexer_asm() {
  const char* source = "__asm__(\"nop\" : :);";
  test_src = source;
  lexer_init(test_getchar);

  TokenType expected[] = {
      TOK_ASM, TOK_LPAREN, TOK_STRING, TOK_COLON, TOK_COLON, TOK_RPAREN, TOK_SEMICOLON, TOK_EOF
  };

  for (int i = 0; i < 8; i++) {
    Token t = lexer_next_token();
    if (t.type != expected[i]) {
      fprintf(
          stderr, "FAIL: test_lexer_asm index %d: expected %s, got %s\n",
          i, lexer_token_type_name(expected[i]), lexer_token_type_name(t.type)
      );
      lexer_free_token(&t);
      return 1;
    }
    lexer_free_token(&t);
  }
  printf("PASS: test_lexer_asm\n");
  return 0;
}

int main() {
  int failed = 0;
  failed += test_lexer_basic();
  failed += test_lexer_operators();
  failed += test_lexer_char_literal();
  failed += test_lexer_asm();
  return failed > 0 ? 1 : 0;
}
