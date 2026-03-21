#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  TOK_EOF = 0,
  TOK_IDENTIFIER,
  TOK_NUMBER,

  // Keywords
  TOK_INT,
  TOK_VOID,
  TOK_RETURN,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,
  TOK_FOR,
  TOK_STRUCT,
  TOK_UINT8_T,
  TOK_UINT16_T,

  // Operators
  TOK_PLUS,     // +
  TOK_MINUS,    // -
  TOK_STAR,     // *
  TOK_SLASH,    // /
  TOK_ASSIGN,   // =
  TOK_EQ,       // ==
  TOK_NEQ,      // !=
  TOK_LT,       // <
  TOK_GT,       // >
  TOK_LE,       // <=
  TOK_GE,       // >=
  TOK_LSHIFT,   // <<
  TOK_RSHIFT,   // >>
  TOK_AMP,      // &
  TOK_BIT_OR,   // |
  TOK_BIT_XOR,  // ^
  TOK_LOG_AND,  // &&
  TOK_LOG_OR,   // ||
  TOK_NOT,      // !
  TOK_INC,      // ++
  TOK_DEC,      // --

  // Punctuation
  TOK_SEMICOLON,  // ;
  TOK_COMMA,      // ,
  TOK_LPAREN,     // (
  TOK_RPAREN,     // )
  TOK_LBRACE,     // {
  TOK_RBRACE,     // }
  TOK_LBRACKET,   // [
  TOK_RBRACKET,   // ]
  TOK_DOT,        // .

  // Error
  TOK_ERROR
} TokenType;

typedef struct {
  TokenType type;
  char* text;  // Null-terminated string
  int length;
  int line;
} Token;

void lexer_init(int (*getchar_cb)(void));
Token lexer_next_token(void);
void lexer_free_token(Token* token);
const char* lexer_token_type_name(TokenType type);

#endif
