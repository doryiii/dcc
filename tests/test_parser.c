#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "parser.h"

static const char* test_src;
static int test_getchar() {
  return *test_src ? (unsigned char)*test_src++ : EOF;
}

static FILE* out_file;
static void file_emit_cb(const char* line) {
  if (out_file) fprintf(out_file, "%s", line);
}

int test_parser_led_blink() {
  const char* source =
      "struct PORT_struct {\n"
      "  uint8_t DIR;\n"
      "};\n"
      "int main(void) {\n"
      "  PORTC.DIRSET = 1 << 2;\n"
      "  while (1) {\n"
      "    for (uint16_t i=0; i<10000; i++);\n"
      "  }\n"
      "  return 1;\n"
      "}\n";

  test_src = source;
  ASTNode* ast = parse_program(test_getchar);
  if (!ast) {
    fprintf(stderr, "FAIL: parse_program returned NULL\n");
    return 1;
  }

  // Basic structure validation
  if (ast->type != AST_PROGRAM) {
    fprintf(stderr, "FAIL: Expected AST_PROGRAM root\n");
    return 1;
  }

  if (ast->num_children < 2) {
    fprintf(
        stderr,
        "FAIL: Expected at least 2 top-level declarations (struct and main)\n"
    );
    return 1;
  }

  // Validate Struct
  ASTNode* struct_decl = ast->children[0];
  if (struct_decl->type != AST_STRUCT_DECL ||
      strcmp(struct_decl->name, "PORT_struct") != 0) {
    fprintf(stderr, "FAIL: Expected struct PORT_struct declaration\n");
    return 1;
  }

  // Validate main
  ASTNode* main_decl = ast->children[1];
  if (main_decl->type != AST_FUNC_DECL ||
      strcmp(main_decl->name, "main") != 0) {
    fprintf(stderr, "FAIL: Expected main function declaration\n");
    return 1;
  }

  // Added codegen call per Phase 3 requirements
  out_file = NULL;  // Do not emit to stdout during test
  codegen_set_emit_cb(file_emit_cb);
  codegen(ast);

  ast_free_node(ast);
  printf("PASS: test_parser_led_blink\n");
  return 0;
}

int main() {
  int failed = 0;
  failed += test_parser_led_blink();

  if (failed == 0) {
    printf("All tests passed!\n");
    return 0;
  } else {
    printf("%d tests failed.\n", failed);
    return 1;
  }
}
