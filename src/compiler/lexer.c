#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int unget_buf = -1;
static int (*get_char_cb)(void) = NULL;
static int current_line = 1;

static char* lexeme = NULL;
#define LEXEME_BUF_SIZE 256
static int lexeme_len = 0;

void lexer_init(int (*getchar_cb)(void)) {
  get_char_cb = getchar_cb;
  unget_buf = -1;
  current_line = 1;
  if (!lexeme) lexeme = (char*)malloc(LEXEME_BUF_SIZE);
}

void lexer_cleanup(void) {
  if (lexeme) {
    free(lexeme);
    lexeme = NULL;
  }
}

static char peek() {
  if (unget_buf == -1) {
    unget_buf = get_char_cb();
  }
  if (unget_buf == EOF) return '\0';
  return (char)unget_buf;
}

static char advance() {
  int c;
  if (unget_buf != -1) {
    c = unget_buf;
    unget_buf = -1;
  } else {
    c = get_char_cb();
  }
  if (c == '\n') current_line++;
  if (c == EOF) return '\0';
  return (char)c;
}

static void lexeme_clear() {
  lexeme_len = 0;
  lexeme[0] = '\0';
}

static void lexeme_append(char c) {
  if (lexeme && lexeme_len < LEXEME_BUF_SIZE - 1) {
    lexeme[lexeme_len++] = c;
    lexeme[lexeme_len] = '\0';
  }
}

static Token make_token(TokenType type, const char* text, int length) {
  Token token;
  token.type = type;
  token.line = current_line;
  token.length = length;
  if (text && length > 0) {
    token.text = (char*)malloc(length + 1);
    memcpy(token.text, text, length);
    token.text[length] = '\0';
  } else {
    token.text = NULL;
  }
  return token;
}

static Token check_keyword_or_id(const char* text, int length) {
  if (length == 3 && strncmp(text, "int", 3) == 0)
    return make_token(TOK_INT, text, length);
  if (length == 4 && strncmp(text, "void", 4) == 0)
    return make_token(TOK_VOID, text, length);
  if (length == 6 && strncmp(text, "return", 6) == 0)
    return make_token(TOK_RETURN, text, length);
  if (length == 2 && strncmp(text, "if", 2) == 0)
    return make_token(TOK_IF, text, length);
  if (length == 4 && strncmp(text, "else", 4) == 0)
    return make_token(TOK_ELSE, text, length);
  if (length == 5 && strncmp(text, "while", 5) == 0)
    return make_token(TOK_WHILE, text, length);
  if (length == 3 && strncmp(text, "for", 3) == 0)
    return make_token(TOK_FOR, text, length);
  if (length == 6 && strncmp(text, "struct", 6) == 0)
    return make_token(TOK_STRUCT, text, length);
  if (length == 7 && strncmp(text, "uint8_t", 7) == 0)
    return make_token(TOK_UINT8_T, text, length);
  if (length == 8 && strncmp(text, "uint16_t", 8) == 0)
    return make_token(TOK_UINT16_T, text, length);
  if (length == 7 && strncmp(text, "__asm__", 7) == 0)
    return make_token(TOK_ASM, text, length);

  return make_token(TOK_IDENTIFIER, text, length);
}

Token lexer_next_token(void) {
  while (1) {
    // skip whitespace
    while (isspace(peek())) {
      advance();
    }

    lexeme_clear();
    char c = peek();

    if (c == '\0') {
      return make_token(TOK_EOF, NULL, 0);
    }

    if (c == '/') {
      advance();  // consume /
      if (peek() == '/') {
        // Line comment
        while (peek() != '\n' && peek() != '\0') {
          advance();
        }
        continue;
      } else if (peek() == '*') {
        // Block comment
        advance();  // skip *
        while (peek() != '\0') {
          if (peek() == '*') {
            advance();
            if (peek() == '/') {
              advance();  // skip /
              break;
            }
          } else {
            advance();
          }
        }
        continue;
      } else {
        // Just a slash
        lexeme_append('/');
        return make_token(TOK_SLASH, lexeme, 1);
      }
    }
    break;
  }

  char c = advance();
  lexeme_append(c);

  if (isalpha(c) || c == '_') {
    while (isalnum(peek()) || peek() == '_') {
      lexeme_append(advance());
    }
    return check_keyword_or_id(lexeme, lexeme_len);
  }

  if (isdigit(c)) {
    if (c == '0' && (peek() == 'x' || peek() == 'X')) {
      lexeme_append(advance());  // skip x/X
      while (isxdigit(peek())) lexeme_append(advance());
    } else {
      while (isdigit(peek())) lexeme_append(advance());
    }
    return make_token(TOK_NUMBER, lexeme, lexeme_len);
  }

  if (c == '\'') {
    char char_val = 0;
    if (peek() == '\\') {
      advance(); // skip backslash
      char esc = advance();
      switch (esc) {
        case 'n': char_val = '\n'; break;
        case 'r': char_val = '\r'; break;
        case 't': char_val = '\t'; break;
        case '0': char_val = '\0'; break;
        case '\\': char_val = '\\'; break;
        case '\'': char_val = '\''; break;
        case '\"': char_val = '\"'; break;
        default: char_val = esc; break; // handle unknown gracefully
      }
    } else {
      char_val = advance();
    }
    if (peek() == '\'') advance(); // skip closing quote

    lexeme_clear();
    lexeme_len = sprintf(lexeme, "%u", (unsigned char)char_val);
    return make_token(TOK_NUMBER, lexeme, lexeme_len);
  }

  if (c == '"') {
    lexeme_clear();  // Clear starting "
    while (peek() != '"' && peek() != '\0') {
      lexeme_append(advance());
    }
    if (peek() == '"') advance();  // skip closing "
    return make_token(TOK_STRING, lexeme, lexeme_len);
  }

  switch (c) {
    case '+':
      if (peek() == '+') {
        lexeme_append(advance());
        return make_token(TOK_INC, lexeme, 2);
      }
      return make_token(TOK_PLUS, lexeme, 1);
    case '-':
      if (peek() == '-') {
        lexeme_append(advance());
        return make_token(TOK_DEC, lexeme, 2);
      }
      return make_token(TOK_MINUS, lexeme, 1);
    case '*':
      return make_token(TOK_STAR, lexeme, 1);
    case '=':
      if (peek() == '=') {
        lexeme_append(advance());
        return make_token(TOK_EQ, lexeme, 2);
      }
      return make_token(TOK_ASSIGN, lexeme, 1);
    case '!':
      if (peek() == '=') {
        lexeme_append(advance());
        return make_token(TOK_NEQ, lexeme, 2);
      }
      return make_token(TOK_NOT, lexeme, 1);
    case '<':
      if (peek() == '<') {
        lexeme_append(advance());
        return make_token(TOK_LSHIFT, lexeme, 2);
      }
      if (peek() == '=') {
        lexeme_append(advance());
        return make_token(TOK_LE, lexeme, 2);
      }
      return make_token(TOK_LT, lexeme, 1);
    case '>':
      if (peek() == '>') {
        lexeme_append(advance());
        return make_token(TOK_RSHIFT, lexeme, 2);
      }
      if (peek() == '=') {
        lexeme_append(advance());
        return make_token(TOK_GE, lexeme, 2);
      }
      return make_token(TOK_GT, lexeme, 1);
    case '&':
      if (peek() == '&') {
        lexeme_append(advance());
        return make_token(TOK_LOG_AND, lexeme, 2);
      }
      return make_token(TOK_AMP, lexeme, 1);
    case '|':
      if (peek() == '|') {
        lexeme_append(advance());
        return make_token(TOK_LOG_OR, lexeme, 2);
      }
      return make_token(TOK_BIT_OR, lexeme, 1);
    case '^':
      return make_token(TOK_BIT_XOR, lexeme, 1);
    case ';':
      return make_token(TOK_SEMICOLON, lexeme, 1);
    case ':':
      return make_token(TOK_COLON, lexeme, 1);
    case ',':
      return make_token(TOK_COMMA, lexeme, 1);
    case '(':
      return make_token(TOK_LPAREN, lexeme, 1);
    case ')':
      return make_token(TOK_RPAREN, lexeme, 1);
    case '{':
      return make_token(TOK_LBRACE, lexeme, 1);
    case '}':
      return make_token(TOK_RBRACE, lexeme, 1);
    case '[':
      return make_token(TOK_LBRACKET, lexeme, 1);
    case ']':
      return make_token(TOK_RBRACKET, lexeme, 1);
    case '.':
      return make_token(TOK_DOT, lexeme, 1);
  }

  return make_token(TOK_ERROR, lexeme, 1);
}

void lexer_free_token(Token* token) {
  if (token->text) {
    free(token->text);
    token->text = NULL;
  }
}

const char* lexer_token_type_name(TokenType type) {
  switch (type) {
    case TOK_EOF:
      return "EOF";
    case TOK_IDENTIFIER:
      return "IDENTIFIER";
    case TOK_NUMBER:
      return "NUMBER";
    case TOK_INT:
      return "INT";
    case TOK_VOID:
      return "VOID";
    case TOK_RETURN:
      return "RETURN";
    case TOK_IF:
      return "IF";
    case TOK_ELSE:
      return "ELSE";
    case TOK_WHILE:
      return "WHILE";
    case TOK_FOR:
      return "FOR";
    case TOK_STRUCT:
      return "STRUCT";
    case TOK_UINT8_T:
      return "UINT8_T";
    case TOK_UINT16_T:
      return "UINT16_T";
    case TOK_ASM:
      return "ASM";
    case TOK_STRING:
      return "STRING";
    case TOK_PLUS:
      return "PLUS";
    case TOK_MINUS:
      return "MINUS";
    case TOK_STAR:
      return "STAR";
    case TOK_SLASH:
      return "SLASH";
    case TOK_ASSIGN:
      return "ASSIGN";
    case TOK_EQ:
      return "EQ";
    case TOK_NEQ:
      return "NEQ";
    case TOK_LT:
      return "LT";
    case TOK_GT:
      return "GT";
    case TOK_LE:
      return "LE";
    case TOK_GE:
      return "GE";
    case TOK_LSHIFT:
      return "LSHIFT";
    case TOK_RSHIFT:
      return "RSHIFT";
    case TOK_AMP:
      return "AMP";
    case TOK_BIT_OR:
      return "BIT_OR";
    case TOK_BIT_XOR:
      return "BIT_XOR";
    case TOK_LOG_AND:
      return "LOG_AND";
    case TOK_LOG_OR:
      return "LOG_OR";
    case TOK_NOT:
      return "NOT";
    case TOK_INC:
      return "INC";
    case TOK_DEC:
      return "DEC";
    case TOK_SEMICOLON:
      return "SEMICOLON";
    case TOK_COLON:
      return "COLON";
    case TOK_COMMA:
      return "COMMA";
    case TOK_LPAREN:
      return "LPAREN";
    case TOK_RPAREN:
      return "RPAREN";
    case TOK_LBRACE:
      return "LBRACE";
    case TOK_RBRACE:
      return "RBRACE";
    case TOK_LBRACKET:
      return "LBRACKET";
    case TOK_RBRACKET:
      return "RBRACKET";
    case TOK_DOT:
      return "DOT";
    case TOK_ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}
