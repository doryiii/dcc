#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dasm.h"

static void write_ihex_line(
    FILE* out, uint8_t length, uint16_t address, uint8_t type, uint8_t* data
) {
  if (!out) return;
  uint8_t checksum = length + (address >> 8) + (address & 0xFF) + type;
  fprintf(out, ":%02X%04X%02X", length, address, type);
  for (int i = 0; i < length; i++) {
    fprintf(out, "%02X", data[i]);
    checksum += data[i];
  }
  fprintf(out, "%02X\n", (uint8_t)(-(int)checksum));
}

static uint8_t* host_buffer = NULL;

static void host_read_page(uint32_t page_addr, uint8_t* page_buf) {
  memcpy(page_buf, host_buffer + page_addr, 512);
}

static void host_write_page(uint32_t page_addr, uint8_t* page_buf) {
  memcpy(host_buffer + page_addr, page_buf, 512);
}

int main(int argc, char** argv) {
  const char* input_file = NULL;
  FILE* in = stdin;
  FILE* bin_out = NULL;
  FILE* hex_out = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      bin_out = fopen(argv[++i], "wb");
    } else if ((strcmp(argv[i], "-hex") == 0 || strcmp(argv[i], "-h") == 0) &&
               i + 1 < argc) {
      hex_out = fopen(argv[++i], "w");
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
      fprintf(
          stderr, "Usage: %s [input.s] [-o output.bin] [-hex|-h output.hex]\n",
          argv[0]
      );
      return 1;
    } else if (input_file == NULL) {
      input_file = argv[i];
      in = fopen(input_file, "r");
      if (!in) {
        perror("fopen");
        return 1;
      }
    } else {
      fprintf(stderr, "Unexpected extra argument: %s\n", argv[i]);
      fprintf(
          stderr, "Usage: %s [input.s] [-o output.bin] [-hex|-h output.hex]\n",
          argv[0]
      );
      return 1;
    }
  }

  uint8_t* output_buffer = malloc(16384);
  memset(output_buffer, 0, 16384);
  host_buffer = output_buffer;
  uint32_t pc = 0;

  dasm_init();

  char line[256];
  while (fgets(line, sizeof(line), in)) {
    uint32_t inst = 0;
    int len = dasm_emit(line, &inst);
    if (len > 0) {
      for (int i = 0; i < len; i++) {
        output_buffer[pc + i] = (inst >> (i * 8)) & 0xFF;
      }
      pc += len;
    }
  }
  if (in != stdin) fclose(in);

  dasm_apply_fixups(host_read_page, host_write_page);

  if (bin_out) {
    fwrite(output_buffer, 1, pc, bin_out);
    fclose(bin_out);
  }
  if (hex_out) {
    for (uint32_t i = 0; i < pc; i += 16) {
      uint8_t len = (pc - i > 16) ? 16 : (pc - i);
      write_ihex_line(hex_out, len, i, 0, &output_buffer[i]);
    }
    write_ihex_line(hex_out, 0, 0, 1, NULL);
    fclose(hex_out);
  }
  free(output_buffer);

  return 0;
}
