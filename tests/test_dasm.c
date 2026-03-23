#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dasm.h"

static uint8_t* host_buffer = NULL;
static void host_read_page(uint32_t page_addr, uint8_t* page_buf) {
  memcpy(page_buf, host_buffer + page_addr, 512);
}
static void host_write_page(uint32_t page_addr, uint8_t* page_buf) {
  memcpy(host_buffer + page_addr, page_buf, 512);
}

void assemble_string(const char* source, uint8_t* buffer, uint32_t* len_out) {
  dasm_init();
  char* str = strdup(source);
  char* line = strtok(str, "\n");
  uint32_t pc = 0;
  while (line) {
    uint32_t inst = 0;
    int len = dasm_emit(line, &inst);
    if (len > 0) {
      for (int i = 0; i < len; i++) {
        buffer[pc + i] = (inst >> (i * 8)) & 0xFF;
      }
      pc += len;
    }
    line = strtok(NULL, "\n");
  }
  free(str);
  host_buffer = buffer;
  dasm_apply_fixups(host_read_page, host_write_page);
  *len_out = pc;
}

int test_dasm_basic() {
  const char* source = "ldi r24, 0x42\nret\n";
  uint8_t buffer[1024] = {0};
  uint32_t len;
  assemble_string(source, buffer, &len);

  uint16_t* code = (uint16_t*)buffer;
  if (code[0] != 0xE482) {
    fprintf(
        stderr,
        "FAIL: test_dasm_basic: ldi r24, 0x42 expected 0xE482, got 0x%04X\n",
        code[0]
    );
    return 1;
  }
  if (code[1] != 0x9508) {
    fprintf(
        stderr, "FAIL: test_dasm_basic: ret expected 0x9508, got 0x%04X\n",
        code[1]
    );
    return 1;
  }

  printf("PASS: test_dasm_basic\n");
  return 0;
}

int test_dasm_labels() {
  const char* source =
      "start:\n"
      "  ldi r24, 1\n"
      "  rjmp start\n";

  uint8_t buffer[1024] = {0};
  uint32_t len;
  assemble_string(source, buffer, &len);

  uint16_t* code = (uint16_t*)buffer;
  if (code[0] != 0xE081) {
    fprintf(
        stderr,
        "FAIL: test_dasm_labels: ldi r24, 1 expected 0xE081, got 0x%04X\n",
        code[0]
    );
    return 1;
  }
  if (code[1] != 0xCFFE) {
    fprintf(
        stderr,
        "FAIL: test_dasm_labels: rjmp start expected 0xCFFE, got 0x%04X\n",
        code[1]
    );
    return 1;
  }

  printf("PASS: test_dasm_labels\n");
  return 0;
}

int test_dasm_or() {
  const char* source = "or r24, r25\n";
  uint8_t buffer[1024] = {0};
  uint32_t len;
  assemble_string(source, buffer, &len);

  uint16_t* code = (uint16_t*)buffer;
  // 0x2800 | ((25 & 0x10) << 5) | ((24 & 0x1F) << 4) | (25 & 0x0F)
  // 0x2800 | 0x0200 | 0x0180 | 0x0009 = 0x2B89
  if (code[0] != 0x2B89) {
    fprintf(
        stderr, "FAIL: test_dasm_or: or r24, r25 expected 0x2B89, got 0x%04X\n",
        code[0]
    );
    return 1;
  }

  printf("PASS: test_dasm_or\n");
  return 0;
}

int main() {
  int failed = 0;
  failed += test_dasm_basic();
  failed += test_dasm_labels();
  failed += test_dasm_or();
  return failed > 0 ? 1 : 0;
}
