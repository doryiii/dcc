#ifndef _USART_H
#define _USART_H

#include <stdint.h>

#define USART_BUFSIZE 16

void usart_init(uint8_t n, uint32_t baud);
void usart_putc(uint8_t n, char c);
void usart_print(uint8_t n, const char *c);
void usart_print_P(uint8_t n, const char *c);
void usart_write_P(uint8_t n, const char *c, uint32_t len);
uint8_t usart_available(uint8_t n);
void usart_tx_flush(uint8_t n);
uint8_t usart_getc(uint8_t n);

#endif // _USART_H
