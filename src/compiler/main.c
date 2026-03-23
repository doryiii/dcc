#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "parser.h"
#include "preprocessor.h"

static FILE* in_file;
static int file_getchar() { return fgetc(in_file); }

static FILE* out_file;
static void file_emit_cb(const char* line) {
  if (out_file) fprintf(out_file, "%s", line);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <input.c> [-o output.s]\n", argv[0]);
    return 1;
  }

  const char* input_file = argv[1];
  in_file = fopen(input_file, "r");
  if (!in_file) {
    perror("fopen");
    return 1;
  }

  preprocessor_init(file_getchar);
  parser_init(preprocessor_getchar);

  out_file = stdout;
  if (argc >= 4 && strcmp(argv[2], "-o") == 0) {
    out_file = fopen(argv[3], "w");
    if (!out_file) {
      perror("fopen output");
      return 1;
    }
  }

  codegen_set_emit_cb(file_emit_cb);
  codegen_prologue();

  ASTNode* decl;
  while ((decl = parse_top_level_declaration()) != NULL) {
    codegen_top_level(decl);
    ast_free_node(decl);
  }

  fclose(in_file);
  if (out_file != stdout) fclose(out_file);
  ast_free_string_pool();

  return 0;
}
