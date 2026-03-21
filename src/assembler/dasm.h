#ifndef DASM_H
#define DASM_H

#include <stdint.h>
#include <stdio.h>

typedef void (*dasm_read_page_fn)(uint32_t page_addr, uint8_t *page_buf);
typedef void (*dasm_write_page_fn)(uint32_t page_addr, uint8_t *page_buf);

void dasm_init(void);
int dasm_emit(const char *asm_line, uint32_t *out_inst);
void dasm_apply_fixups(dasm_read_page_fn read_page,
                       dasm_write_page_fn write_page);

#endif
