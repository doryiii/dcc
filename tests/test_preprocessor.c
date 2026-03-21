#include "preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_preprocessor_basic() {
  const char *source = "#define LED_PIN 2\nint x = LED_PIN;";
  const char *expected = "int x = 2;";

  char *result = preprocess(source);
  // Use strstr or manual trim comparison because our preprocessor might leave
  // extra newlines
  if (strstr(result, expected) == NULL) {
    fprintf(stderr,
            "FAIL: test_preprocessor_basic: expected '%s' to be in '%s'\n",
            expected, result);
    free(result);
    return 1;
  }
  free(result);
  printf("PASS: test_preprocessor_basic\n");
  return 0;
}

int test_preprocessor_arguments() {
  const char *source =
      "#define SET_BIT(reg, bit) reg |= (1 << bit)\nSET_BIT(PORTC, 2);";
  const char *expected = "PORTC |= (1 << 2);";

  char *result = preprocess(source);
  if (strstr(result, expected) == NULL) {
    fprintf(stderr,
            "FAIL: test_preprocessor_arguments: expected '%s' to be in '%s'\n",
            expected, result);
    free(result);
    return 1;
  }
  free(result);
  printf("PASS: test_preprocessor_arguments\n");
  return 0;
}

int main() {
  int failed = 0;
  failed += test_preprocessor_basic();
  failed += test_preprocessor_arguments();
  return failed > 0 ? 1 : 0;
}
