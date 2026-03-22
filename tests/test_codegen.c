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

int test_codegen_main_return() {
  const char* source = "int main(void) { return 42; }";
  test_src = source;
  ASTNode* ast = parse_program(test_getchar);

  char buffer[4096];
  out_file = fmemopen(buffer, sizeof(buffer), "w");
  codegen_set_emit_cb(file_emit_cb);
  codegen(ast);
  fclose(out_file);
  out_file = NULL;

  // Check for r24 ldi with 0x2A (42)
  if (strstr(buffer, "ldi r24, 0x2A") == NULL) {
    fprintf(
        stderr,
        "FAIL: test_codegen_main_return: expected 'ldi r24, 0x2A' in output\n"
    );
    return 1;
  }

  ast_free_node(ast);
  printf("PASS: test_codegen_main_return\n");
  return 0;
}

int test_codegen_struct_access() {
  const char* source =
      "struct S { uint8_t a; uint8_t b; };\n"
      "int main(void) {\n"
      "  (*(struct S) 0x100).b = 5;\n"
      "}";
  test_src = source;
  ASTNode* ast = parse_program(test_getchar);

  char buffer[4096];
  out_file = fmemopen(buffer, sizeof(buffer), "w");
  codegen_set_emit_cb(file_emit_cb);
  codegen(ast);
  fclose(out_file);
  out_file = NULL;

  // Check for adiw r30, 1 (offset of b) and st Z, r24
  if (strstr(buffer, "adiw r30, 1") == NULL) {
    fprintf(
        stderr,
        "FAIL: test_codegen_struct_access: expected offset adiw r30, 1\n"
    );
    return 1;
  }

  ast_free_node(ast);
  printf("PASS: test_codegen_struct_access\n");
  return 0;
}

int test_codegen_if_and_comparisons() {
  const char* source =
      "int main(void) {\n"
      "  uint8_t a = 1;\n"
      "  if (a == 1) { a = 2; }\n"
      "  if (a > 1) { a = 3; }\n"
      "  if (a >= 2) { a = 4; }\n"
      "  if (a < 5) { a = 5; }\n"
      "  if (a <= 5) { a = 6; }\n"
      "  if (a != 0) { a = 7; }\n"
      "}";
  test_src = source;
  ASTNode* ast = parse_program(test_getchar);

  char buffer[8192];
  out_file = fmemopen(buffer, sizeof(buffer), "w");
  codegen_set_emit_cb(file_emit_cb);
  codegen(ast);
  fclose(out_file);
  out_file = NULL;

  if (strstr(buffer, "breq L") == NULL) {
    fprintf(stderr, "FAIL: test_codegen_if_and_comparisons: expected breq\n");
    return 1;
  }
  if (strstr(buffer, "brlt L") == NULL) {
    fprintf(stderr, "FAIL: test_codegen_if_and_comparisons: expected brlt\n");
    return 1;
  }
  if (strstr(buffer, "brge L") == NULL) {
    fprintf(stderr, "FAIL: test_codegen_if_and_comparisons: expected brge\n");
    return 1;
  }
  if (strstr(buffer, "brne L") == NULL) {
    fprintf(stderr, "FAIL: test_codegen_if_and_comparisons: expected brne\n");
    return 1;
  }

  ast_free_node(ast);
  printf("PASS: test_codegen_if_and_comparisons\n");
  return 0;
}

int test_codegen_not() {
  const char* source =
      "int main(void) {\n"
      "  uint8_t a = 1;\n"
      "  a = !a;\n"
      "}";
  test_src = source;
  ASTNode* ast = parse_program(test_getchar);

  char buffer[4096];
  out_file = fmemopen(buffer, sizeof(buffer), "w");
  codegen_set_emit_cb(file_emit_cb);
  codegen(ast);
  fclose(out_file);
  out_file = NULL;

  if (strstr(buffer, "or r24, r25") == NULL) {
    fprintf(stderr, "FAIL: test_codegen_not: expected 'or r24, r25' in output\n");
    return 1;
  }

  ast_free_node(ast);
  printf("PASS: test_codegen_not\n");
  return 0;
}

int main() {
  int failed = 0;
  failed += test_codegen_main_return();
  failed += test_codegen_struct_access();
  failed += test_codegen_if_and_comparisons();
  failed += test_codegen_not();
  return failed > 0 ? 1 : 0;
}
