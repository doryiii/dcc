#include "lexer.h"
#include <stdio.h>
#include <string.h>

static const char *test_src;
static int test_getchar() {
  return *test_src ? (unsigned char)*test_src++ : EOF;
}

int test_lexer_basic() {
  const char *source = "int main() { return 1; }";
  test_src = source;
  lexer_init(test_getchar);

  TokenType expected[] = {TOK_INT,    TOK_IDENTIFIER, TOK_LPAREN, TOK_RPAREN,
                          TOK_LBRACE, TOK_RETURN,     TOK_NUMBER, TOK_SEMICOLON,
                          TOK_RBRACE, TOK_EOF};

  for (int i = 0; i < 10; i++) {
    Token t = lexer_next_token();
    if (t.type != expected[i]) {
      fprintf(stderr, "FAIL: test_lexer_basic index %d: expected %s, got %s\n",
              i, lexer_token_type_name(expected[i]),
              lexer_token_type_name(t.type));
      lexer_free_token(&t);
      return 1;
    }
    lexer_free_token(&t);
  }
  printf("PASS: test_lexer_basic\n");
  return 0;
}

int test_lexer_operators() {
  const char *source =
      "+ - * / = == != < > <= >= << >> & | ^ && || ! ++ -- . [ ]";
  test_src = source;
  lexer_init(test_getchar);

  TokenType expected[] = {
      TOK_PLUS,    TOK_MINUS,   TOK_STAR,     TOK_SLASH,    TOK_ASSIGN,
      TOK_EQ,      TOK_NEQ,     TOK_LT,       TOK_GT,       TOK_LE,
      TOK_GE,      TOK_LSHIFT,  TOK_RSHIFT,   TOK_AMP,      TOK_BIT_OR,
      TOK_BIT_XOR, TOK_LOG_AND, TOK_LOG_OR,   TOK_NOT,      TOK_INC,
      TOK_DEC,     TOK_DOT,     TOK_LBRACKET, TOK_RBRACKET, TOK_EOF};

  for (int i = 0; i < 25; i++) {
    Token t = lexer_next_token();
    if (t.type != expected[i]) {
      fprintf(
          stderr, "FAIL: test_lexer_operators index %d: expected %s, got %s\n",
          i, lexer_token_type_name(expected[i]), lexer_token_type_name(t.type));
      lexer_free_token(&t);
      return 1;
    }
    lexer_free_token(&t);
  }
  printf("PASS: test_lexer_operators\n");
  return 0;
}

int main() {
  int failed = 0;
  failed += test_lexer_basic();
  failed += test_lexer_operators();
  return failed > 0 ? 1 : 0;
}
