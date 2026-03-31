#include "dasm.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  ASM_TOK_EOF,
  ASM_TOK_LABEL,
  ASM_TOK_IDENTIFIER,
  ASM_TOK_NUMBER,
  ASM_TOK_COMMA,
  ASM_TOK_PLUS,
  ASM_TOK_MINUS,
  ASM_TOK_DIRECTIVE,
  ASM_TOK_NEWLINE,
  ASM_TOK_LPAREN,
  ASM_TOK_RPAREN,
  ASM_TOK_EQUALS
} AsmTokenType;

typedef struct {
  AsmTokenType type;
  char text[32];
  int value;
} AsmToken;

typedef struct Symbol {
  struct Symbol* next;
  uint32_t address;
  char name[];
} Symbol;

typedef struct Fixup {
  struct Fixup* next;
  uint32_t patch_address;
  uint8_t type;
  char name[];
} Fixup;

static Symbol* symbols = NULL;
static Fixup* fixups = NULL;
static uint32_t dasm_pc = 0;

void dasm_add_symbol(const char* name, uint32_t address) {
  Symbol* s = (Symbol*)malloc(sizeof(Symbol) + strlen(name) + 1);
  strcpy(s->name, name);
  s->address = address;
  s->next = symbols;
  symbols = s;
}

static void add_fixup(const char* name, uint32_t patch_address, uint8_t type) {
  Fixup* f = (Fixup*)malloc(sizeof(Fixup) + strlen(name) + 1);
  strcpy(f->name, name);
  f->patch_address = patch_address;
  f->type = type;
  f->next = fixups;
  fixups = f;
}

static uint32_t find_symbol(const char* name);

void dasm_apply_fixups(
    dasm_read_page_fn read_page, dasm_write_page_fn write_page
) {
  Fixup* f = fixups;
  uint8_t page_buf[512];
  uint32_t current_page = 0xFFFFFFFF;
  bool page_dirty = false;

  while (f) {
    uint32_t address;
    if (strcmp(f->name, ".") == 0) {
      address = f->patch_address + 2;
    } else {
      address = find_symbol(f->name);
    }

    if (address != 0xFFFFFFFF) {
      uint32_t patch_page = f->patch_address & ~511;
      if (patch_page != current_page) {
        if (page_dirty && write_page) {
          write_page(current_page, page_buf);
        }
        current_page = patch_page;
        if (read_page) {
          read_page(current_page, page_buf);
        }
        page_dirty = false;
      }

      uint32_t offset = f->patch_address - current_page;
      uint16_t op = page_buf[offset] | (page_buf[offset + 1] << 8);

      if (f->type == 0) {  // RJMP/RCALL (12-bit relative)
        int k = (address - (f->patch_address + 2)) / 2;
        op |= (k & 0x0FFF);
      } else if (f->type == 1) {  // BRLO/BRNE (7-bit relative)
        int k = (address - (f->patch_address + 2)) / 2;
        op |= ((k & 0x7F) << 3);
      } else if (f->type == 2) {  // LDI LO8
        uint8_t k = address & 0xFF;
        op |= ((k & 0xF0) << 4) | (k & 0x0F);
      } else if (f->type == 3) {  // LDI HI8
        uint8_t k = (address >> 8) & 0xFF;
        op |= ((k & 0xF0) << 4) | (k & 0x0F);
      } else if (f->type == 4) {  // LDI PM_LO8
        int offset = (address - (f->patch_address - 4)) / 2;
        uint8_t k = offset & 0xFF;
        op |= ((k & 0xF0) << 4) | (k & 0x0F);
      } else if (f->type == 5) {  // LDI PM_HI8
        int offset = (address - (f->patch_address - 6)) / 2;
        uint8_t k = (offset >> 8) & 0xFF;
        op |= ((k & 0xF0) << 4) | (k & 0x0F);
      }

      page_buf[offset] = op & 0xFF;
      page_buf[offset + 1] = (op >> 8) & 0xFF;
      page_dirty = true;
    } else {
      fprintf(stderr, "Error: Undefined symbol '%s'\n", f->name);
    }
    f = f->next;
  }

  if (page_dirty && write_page) {
    write_page(current_page, page_buf);
  }
}

static uint32_t find_symbol(const char* name) {
  Symbol* s = symbols;
  while (s) {
    if (strcmp(s->name, name) == 0) return s->address;
    s = s->next;
  }
  return 0xFFFFFFFF;
}

static AsmToken next_asm_token(const char** src) {
  while (**src && isspace(**src) && **src != '\n') (*src)++;
  AsmToken t = {ASM_TOK_EOF, "", 0};
  if (**src == '\0') return t;
  if (**src == '\n') {
    t.type = ASM_TOK_NEWLINE;
    (*src)++;
    return t;
  }
  if (**src == ';') {
    while (**src != '\n' && **src != '\0') (*src)++;
    return next_asm_token(src);
  }
  if (**src == '/' && *(*src + 1) == '*') {
    (*src) += 2;
    while (**src && !(**src == '*' && *(*src + 1) == '/')) (*src)++;
    if (**src) (*src) += 2;
    return next_asm_token(src);
  }

  const char* start = *src;
  if (**src == '.') {
    (*src)++;
    while (isalnum(**src) || **src == '_') (*src)++;
    int len = *src - start;
    if (len > 31) len = 31;
    strncpy(t.text, start, len);
    t.text[len] = '\0';
    if (**src == ':') {
      t.type = ASM_TOK_LABEL;
      (*src)++;
      return t;
    }
    t.type = ASM_TOK_DIRECTIVE;
    return t;
  }
  if (isalpha(**src) || **src == '_') {
    while (isalnum(**src) || **src == '_') (*src)++;
    int len = *src - start;
    if (len > 31) len = 31;
    strncpy(t.text, start, len);
    t.text[len] = '\0';
    if (**src == ':') {
      t.type = ASM_TOK_LABEL;
      (*src)++;
      return t;
    }
    t.type = ASM_TOK_IDENTIFIER;
    return t;
  }
  if (isdigit(**src)) {
    t.type = ASM_TOK_NUMBER;
    t.value = (int)strtol(start, (char**)src, 0);
    return t;
  }
  if (**src == ',') {
    t.type = ASM_TOK_COMMA;
    (*src)++;
    return t;
  }
  if (**src == '+') {
    t.type = ASM_TOK_PLUS;
    (*src)++;
    return t;
  }
  if (**src == '-') {
    t.type = ASM_TOK_MINUS;
    (*src)++;
    return t;
  }
  if (**src == '(') {
    t.type = ASM_TOK_LPAREN;
    (*src)++;
    return t;
  }
  if (**src == ')') {
    t.type = ASM_TOK_RPAREN;
    (*src)++;
    return t;
  }
  if (**src == '=') {
    t.type = ASM_TOK_EQUALS;
    (*src)++;
    return t;
  }
  (*src)++;
  return next_asm_token(src);
}

static int get_reg(const char* text) {
  if (!text) return -1;
  if ((text[0] == 'r' || text[0] == 'R') && isdigit(text[1]))
    return atoi(text + 1);
  if (strcmp(text, "Y") == 0) return 28;
  if (strcmp(text, "Z") == 0) return 30;
  if (strcmp(text, "SPL") == 0) return 0x3D;
  if (strcmp(text, "SPH") == 0) return 0x3E;
  uint32_t sym = find_symbol(text);
  if (sym != 0xFFFFFFFF) return (int)sym;
  return -1;
}

static int parse_expr(AsmToken* args, int* idx, int num_args) {
  if (*idx < num_args && args[*idx].type == ASM_TOK_IDENTIFIER) {
    if (strcmp(args[*idx].text, "lo8") == 0 ||
        strcmp(args[*idx].text, "hi8") == 0) {
      bool lo = (args[*idx].text[0] == 'l');
      (*idx)++;  // lo8/hi8
      (*idx)++;  // (
      uint32_t val = 0;
      if (*idx < num_args && args[*idx].type == ASM_TOK_NUMBER) {
        val = args[*idx].value;
      } else if (*idx < num_args && args[*idx].type == ASM_TOK_IDENTIFIER) {
        val = find_symbol(args[*idx].text);
      }
      (*idx)++;  // symbol or number
      (*idx)++;  // )
      if (val == 0xFFFFFFFF) return 0;
      return lo ? (val & 0xFF) : ((val >> 8) & 0xFF);
    } else {
      uint32_t val = find_symbol(args[*idx].text);
      (*idx)++;
      if (val == 0xFFFFFFFF) return 0;
      return (int)val;
    }
  }
  if (*idx < num_args && args[*idx].type == ASM_TOK_NUMBER) {
    return args[(*idx)++].value;
  }
  return 0;
}

void dasm_init(void) {
  dasm_pc = 0;
  while (symbols) {
    Symbol* s = symbols;
    symbols = s->next;
    free(s);
  }
  while (fixups) {
    Fixup* f = fixups;
    fixups = f->next;
    free(f);
  }
}

int dasm_emit(const char* asm_line, uint32_t* out_inst) {
  const char* s = asm_line;
  AsmToken t;
  int emitted_bytes = 0;
  while ((t = next_asm_token(&s)).type != ASM_TOK_EOF) {
    if (t.type == ASM_TOK_NEWLINE) {
      continue;
    }

    if (t.type == ASM_TOK_LABEL) {
      dasm_add_symbol(t.text, dasm_pc);
      continue;
    }

    if (t.type == ASM_TOK_DIRECTIVE) {
      while (t.type != ASM_TOK_NEWLINE && t.type != ASM_TOK_EOF) {
        t = next_asm_token(&s);
      }
      continue;
    }

    if (t.type == ASM_TOK_IDENTIFIER) {
      const char* temp = s;
      while (*temp && isspace(*temp) && *temp != '\n') temp++;
      if (*temp == '=') {
        s = temp + 1;
        AsmToken val_tok = next_asm_token(&s);
        if (val_tok.type == ASM_TOK_NUMBER) {
          dasm_add_symbol(t.text, val_tok.value);
        }
        while (t.type != ASM_TOK_NEWLINE && t.type != ASM_TOK_EOF) {
          t = next_asm_token(&s);
        }
        continue;
      }

      char mnemonic[32];
      strcpy(mnemonic, t.text);
      AsmToken args[10];
      int num_args = 0;
      while ((t = next_asm_token(&s)).type != ASM_TOK_NEWLINE &&
             t.type != ASM_TOK_EOF) {
        if (t.type != ASM_TOK_COMMA) args[num_args++] = t;
      }

      uint16_t op = 0;
      bool encoded = false;
      int arg_idx = 0;

      if (strcmp(mnemonic, "ldi") == 0) {
        int d = get_reg(args[0].text);
        arg_idx = 1;
        int k = 0;
        int fixup_type = 2;  // lo8 default
        if (args[1].type == ASM_TOK_IDENTIFIER) {
          if (strcmp(args[1].text, "lo8") == 0 ||
              strcmp(args[1].text, "hi8") == 0) {
            bool lo = (args[1].text[0] == 'l');
            fixup_type = lo ? 2 : 3;
            // args[2] is '('
            if (args[3].type == ASM_TOK_IDENTIFIER &&
                strcmp(args[3].text, "pm") == 0) {
              fixup_type = lo ? 4 : 5;
              const char* target = args[5].text;
              uint32_t val = find_symbol(target);
              if (val == 0xFFFFFFFF) {
                add_fixup(target, dasm_pc, fixup_type);
              } else {
                int offset = (val - (dasm_pc - (lo ? 4 : 6))) / 2;
                k = lo ? (offset & 0xFF) : ((offset >> 8) & 0xFF);
              }
            } else if (args[3].type == ASM_TOK_IDENTIFIER) {
              uint32_t val = find_symbol(args[3].text);
              if (val == 0xFFFFFFFF) {
                add_fixup(args[3].text, dasm_pc, fixup_type);
              } else {
                k = lo ? (val & 0xFF) : ((val >> 8) & 0xFF);
              }
            } else if (args[3].type == ASM_TOK_NUMBER) {
              k = lo ? (args[3].value & 0xFF) : ((args[3].value >> 8) & 0xFF);
            }
          } else {
            uint32_t val = find_symbol(args[1].text);
            if (val == 0xFFFFFFFF) {
              add_fixup(args[1].text, dasm_pc, 2);
            } else {
              k = val & 0xFF;
            }
          }
        } else {
          k = parse_expr(args, &arg_idx, num_args);
        }

        op = 0xE000 | ((k & 0xF0) << 4) | ((d & 0x0F) << 4) | (k & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "rcall") == 0 ||
                 strcmp(mnemonic, "rjmp") == 0) {
        uint32_t target = find_symbol(args[0].text);
        int k = 0;
        if (target == 0xFFFFFFFF) {
          add_fixup(args[0].text, dasm_pc, 0);
        } else {
          k = (target - (dasm_pc + 2)) / 2;
        }
        op = (strcmp(mnemonic, "rcall") == 0 ? 0xD000 : 0xC000) | (k & 0x0FFF);
        encoded = true;
      } else if (strcmp(mnemonic, "brlo") == 0 ||
                 strcmp(mnemonic, "brne") == 0 ||
                 strcmp(mnemonic, "breq") == 0 ||
                 strcmp(mnemonic, "brlt") == 0) {
        uint32_t target = find_symbol(args[0].text);
        int k = 0;
        if (target == 0xFFFFFFFF) {
          add_fixup(args[0].text, dasm_pc, 1);
        } else {
          k = (target - (dasm_pc + 2)) / 2;
        }
        if (strcmp(mnemonic, "brlo") == 0)
          op = 0xF000 | ((k & 0x7F) << 3);
        else if (strcmp(mnemonic, "brne") == 0)
          op = 0xF401 | ((k & 0x7F) << 3);
        else if (strcmp(mnemonic, "breq") == 0)
          op = 0xF001 | ((k & 0x7F) << 3);
        else if (strcmp(mnemonic, "brlt") == 0)
          op = 0xF004 | ((k & 0x7F) << 3);
        encoded = true;
      } else if (strcmp(mnemonic, "cpi") == 0) {
        int d = get_reg(args[0].text);
        arg_idx = 1;
        int k = parse_expr(args, &arg_idx, num_args);
        op = 0x3000 | ((k & 0xF0) << 4) | ((d & 0x0F) << 4) | (k & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "out") == 0) {
        arg_idx = 0;
        int A = parse_expr(args, &arg_idx, num_args);
        int r = get_reg(args[arg_idx].text);
        op = 0xB800 | ((r & 0x1F) << 4) | ((A & 0x30) << 5) | (A & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "in") == 0) {
        int r = get_reg(args[0].text);
        arg_idx = 1;
        int A = parse_expr(args, &arg_idx, num_args);
        op = 0xB000 | ((r & 0x1F) << 4) | ((A & 0x30) << 5) | (A & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "ret") == 0) {
        op = 0x9508;
        encoded = true;
      } else if (strcmp(mnemonic, "icall") == 0) {
        op = 0x9509;
        encoded = true;
      } else if (strcmp(mnemonic, "ijmp") == 0) {
        op = 0x9409;
        encoded = true;
      } else if (strcmp(mnemonic, "push") == 0) {
        int d = get_reg(args[0].text);
        op = 0x920F | ((d & 0x1F) << 4);
        encoded = true;
      } else if (strcmp(mnemonic, "pop") == 0) {
        int d = get_reg(args[0].text);
        op = 0x900F | ((d & 0x1F) << 4);
        encoded = true;
      } else if (strcmp(mnemonic, "st") == 0) {
        int r = get_reg(args[1].text);
        op = 0x8200 | ((r & 0x1F) << 4);
        encoded = true;
      } else if (strcmp(mnemonic, "std") == 0) {
        int q = 0;
        if (num_args > 2 && args[1].type == ASM_TOK_PLUS) q = args[2].value;
        int r = get_reg(args[num_args - 1].text);
        int yz_bit = (strcmp(args[0].text, "Y") == 0) ? 0x0008 : 0x0000;
        if (q == 0)
          op = (yz_bit ? 0x8208 : 0x8200) | ((r & 0x1F) << 4);
        else
          op = 0x8200 | yz_bit | ((r & 0x1F) << 4) | ((q & 0x20) << 8) |
               ((q & 0x18) << 7) | (q & 0x07);
        encoded = true;
      } else if (strcmp(mnemonic, "ld") == 0) {
        int d = get_reg(args[0].text);
        op = 0x8000 | ((d & 0x1F) << 4);
        encoded = true;
      } else if (strcmp(mnemonic, "ldd") == 0) {
        int d = get_reg(args[0].text);
        int q = 0;
        if (num_args > 3 && args[2].type == ASM_TOK_PLUS) q = args[3].value;
        int yz_bit = (strcmp(args[1].text, "Y") == 0) ? 0x0008 : 0x0000;
        if (q == 0)
          op = (yz_bit ? 0x8008 : 0x8000) | ((d & 0x1F) << 4);
        else
          op = 0x8000 | yz_bit | ((d & 0x1F) << 4) | ((q & 0x20) << 8) |
               ((q & 0x18) << 7) | (q & 0x07);
        encoded = true;
      } else if (strcmp(mnemonic, "adiw") == 0 ||
                 strcmp(mnemonic, "sbiw") == 0) {
        int r = get_reg(args[0].text);
        int k = args[1].value;
        int d = (r - 24) / 2;
        op = (strcmp(mnemonic, "adiw") == 0 ? 0x9600 : 0x9700) |
             ((k & 0x30) << 2) | (d << 4) | (k & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "movw") == 0) {
        int d = get_reg(args[0].text) / 2;
        int r = get_reg(args[1].text) / 2;
        op = 0x0100 | (d << 4) | r;
        encoded = true;
      } else if (strcmp(mnemonic, "cp") == 0) {
        int d = get_reg(args[0].text);
        int r = get_reg(args[1].text);
        op = 0x1400 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "cpc") == 0) {
        int d = get_reg(args[0].text);
        int r = get_reg(args[1].text);
        op = 0x0400 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "clr") == 0) {
        int d = get_reg(args[0].text);
        op = 0x2400 | ((d & 0x10) << 5) | ((d & 0x1F) << 4) | (d & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "tst") == 0) {
        int d = get_reg(args[0].text);
        op = 0x2000 | ((d & 0x10) << 5) | ((d & 0x1F) << 4) | (d & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "lsl") == 0) {
        int d = get_reg(args[0].text);
        op = 0x0C00 | ((d & 0x10) << 5) | ((d & 0x1F) << 4) | (d & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "rol") == 0) {
        int d = get_reg(args[0].text);
        op = 0x1C00 | ((d & 0x10) << 5) | ((d & 0x1F) << 4) | (d & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "dec") == 0) {
        int d = get_reg(args[0].text);
        op = 0x940A | ((d & 0x1F) << 4);
        encoded = true;
      } else if (strcmp(mnemonic, "add") == 0) {
        int d = get_reg(args[0].text);
        int r = get_reg(args[1].text);
        op = 0x0C00 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "or") == 0) {
        int d = get_reg(args[0].text);
        int r = get_reg(args[1].text);
        op = 0x2800 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F);
        encoded = true;
      } else if (strcmp(mnemonic, "adc") == 0) {
        int d = get_reg(args[0].text);
        int r = get_reg(args[1].text);
        op = 0x1C00 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F);
        encoded = true;
      }

      if (encoded) {
        if (out_inst) *out_inst = op;
        dasm_pc += 2;
        emitted_bytes = 2;
      }

      if (encoded) {
        return emitted_bytes;
      }
    }
  }
  return emitted_bytes;
}
