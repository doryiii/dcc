#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>

#include "clkctrl.h"
#include "codegen.h"
#include "dasm.h"
#include "gpio.h"
#include "parser.h"
#include "preprocessor.h"
#include "usart.h"

#define BOOT_SIZE (0xC0 * 512UL)
#define APP_START BOOT_SIZE

FUSES = {
    .OSCCFG = CLKSEL_OSCHF_gc,
    .SYSCFG0 = CRCSRC_NOCRC_gc | RSTPINCFG_GPIO_gc,
    .SYSCFG1 = SUT_64MS_gc,
    .CODESIZE = 0x00,
    .BOOTSIZE = 0xC0
};

static inline void pgm_word_write(uint32_t address, uint16_t data) {
  __asm__ __volatile__(
      "movw r30, %A0\n\t"
      "out %2, %C0\n\t"
      "movw r0, %A1\n\t"
      "spm\n\t"
      "clr r1\n\t"
      :
      : "r"(address), "r"(data), "I"(_SFR_IO_ADDR(RAMPZ))
      : "r0", "r30", "r31"
  );
}

static inline void pgm_jmp_far(uint32_t word_address) {
  __asm__ __volatile__(
      "movw r30, %A0\n\t"
      "out %1, %C0\n\t"
      "ijmp\n\t"
      :
      : "r"(word_address), "I"(_SFR_IO_ADDR(RAMPZ))
      : "r30", "r31"
  );
}

static void wait_nvm() {
  while (NVMCTRL.STATUS & (NVMCTRL_EEBUSY_bm | NVMCTRL_FBUSY_bm));
}

static void erase_page(uint32_t address) {
  wait_nvm();
  ccp_write_spm((void*)&NVMCTRL.CTRLA, NVMCTRL_CMD_NONE_gc);
  ccp_write_spm((void*)&NVMCTRL.CTRLA, NVMCTRL_CMD_FLPER_gc);
  pgm_word_write(address, 0xFFFF);
  wait_nvm();
  ccp_write_spm((void*)&NVMCTRL.CTRLA, NVMCTRL_CMD_NONE_gc);
}

static void write_page(uint32_t address, uint8_t* buf) {
  erase_page(address);
  ccp_write_spm((void*)&NVMCTRL.CTRLA, NVMCTRL_CMD_FLWR_gc);
  for (uint16_t i = 0; i < PROGMEM_PAGE_SIZE; i += 2) {
    uint16_t data = buf[i] | (buf[i + 1] << 8);
    wait_nvm();
    pgm_word_write(address + i, data);
  }
  wait_nvm();
  ccp_write_spm((void*)&NVMCTRL.CTRLA, NVMCTRL_CMD_NONE_gc);
}

static void read_page(uint32_t address, uint8_t* buf) {
  for (uint16_t i = 0; i < PROGMEM_PAGE_SIZE; i++) {
    buf[i] = pgm_read_byte_far(address + i);
  }
}

static bool eof_reached = false;

int avr_getchar(void) {
  if (eof_reached) return EOF;
  while (!usart_available(2));
  uint8_t c = usart_getc(2);
  if (c == '\r' || c == '\n') {
    usart_putc(2, '\r');
    usart_putc(2, '\n');
    return '\n';
  } else if (c == '\0' || c == 0x04) {
    eof_reached = true;
    return EOF;
  } else {
    usart_putc(2, c);
  }
  return c;
}

extern void *__brkval;

static uint16_t get_free_ram(void) {
  uint16_t free_memory;
  if ((uint16_t)__brkval == 0) {
    // if malloc hasn't been called, heap starts after bss
    extern int __bss_end;
    free_memory = ((uint16_t)&free_memory) - ((uint16_t)&__bss_end);
  } else {
    // heap has been used, calculate diff between stack pointer and top of heap
    free_memory = ((uint16_t)&free_memory) - ((uint16_t)__brkval);
  }
  return free_memory;
}

static uint32_t avr_pc = APP_START;
static uint8_t current_page_buf[PROGMEM_PAGE_SIZE];

static void avr_emit_cb(const char* line) {
#ifdef DEBUG_PRINT_ASM
  printf_P(PSTR("%s\r"), line);
#endif
  uint32_t inst;
  int len = dasm_emit(line, &inst);
  if (len > 0) {
    for (int i = 0; i < len; i++) {
      uint32_t offset = (avr_pc - APP_START) % PROGMEM_PAGE_SIZE;
      current_page_buf[offset] = (inst >> (i * 8)) & 0xFF;
      avr_pc++;
      if (offset == PROGMEM_PAGE_SIZE - 1) {
        write_page(avr_pc - PROGMEM_PAGE_SIZE, current_page_buf);
        memset(current_page_buf, 0xFF, PROGMEM_PAGE_SIZE);
      }
    }
  }
}

void dasm_read_page_cb(uint32_t page_addr, uint8_t* page_buf) {
  read_page(APP_START + page_addr, page_buf);
}

void dasm_write_page_cb(uint32_t page_addr, uint8_t* page_buf) {
  write_page(APP_START + page_addr, page_buf);
}

int main(void) {
  // Move interrupts to boot section
  ccp_write_io((void*)&CPUINT.CTRLA, CPUINT_IVSEL_bm);

  set_clock();
  usart_console_init(2, 9600);

  // Global interrupts enable
  sei();

  _delay_ms(1000);
  printf_P(PSTR("Enter C source ending with EOT or NULL.\r\n"));
  memset(current_page_buf, 0xFF, PROGMEM_PAGE_SIZE);
  avr_pc = APP_START;

  printf_P(PSTR("Free RAM before parsing: %u bytes\r\n"), get_free_ram());
  dasm_init();
  preprocessor_init(avr_getchar);
  ASTNode* ast = parse_program(preprocessor_getchar);
  printf_P(PSTR("Free RAM after parsing: %u bytes\r\n"), get_free_ram());

  printf_P(PSTR("Parsing done. Generating code...\r\n"));
  codegen_set_emit_cb(avr_emit_cb);
  codegen(ast);

  // Flush any remaining partial page
  uint32_t offset = (avr_pc - APP_START) % PROGMEM_PAGE_SIZE;
  if (offset > 0) {
    write_page(avr_pc - offset, current_page_buf);
  }
  printf_P(PSTR("Free RAM after codegen: %u bytes\r\n"), get_free_ram());

  printf_P(PSTR("Code generation done. Applying fixups...\r\n"));
  dasm_apply_fixups(dasm_read_page_cb, dasm_write_page_cb);
  printf_P(PSTR("Free RAM after fixup: %u bytes\r\n"), get_free_ram());

  ast_free_node(ast);

  printf_P(PSTR("Done! Program size: %u\r\n"), (uint16_t)(avr_pc - APP_START));
  printf_P(PSTR("Jumping to app...\r\n"));
  usart_tx_flush(2);
  _delay_ms(1000);

  cli();  // Disable interrupts before jumping
  // Lock boot section (may not be needed)
  NVMCTRL.CTRLB = NVMCTRL_BOOTRP_bm;

  pgm_jmp_far(APP_START / 2);

  while (1);
}
