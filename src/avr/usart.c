#include "usart.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/atomic.h>

#include "gpio.h"

typedef struct {
  uint8_t* rx_buf;
  uint8_t rx_bufindex;
  uint8_t rx_buflen;
  uint8_t* tx_buf;
  uint8_t tx_bufindex;
  uint8_t tx_buflen;
} usart_state_t;

static volatile usart_state_t* usart_states[3] = {NULL, NULL, NULL};

volatile USART_t* dev[3] = {
    (USART_t*)0x0800,
    (USART_t*)0x0820,
    (USART_t*)0x0840,
};

static uint8_t usart_console;

static int usart_putc_f(char c, FILE* stream __attribute__((unused))) {
  if (c == '\n') {
    usart_putc(usart_console, '\r');
  }
  usart_putc(usart_console, c);
  return 0;
}

static FILE usart_stdout =
    FDEV_SETUP_STREAM(usart_putc_f, NULL, _FDEV_SETUP_WRITE);

void usart_console_init(uint8_t n, uint32_t baud) {
  usart_console = n;
  usart_init(n, baud);
  stdout = &usart_stdout;
}

void usart_init(uint8_t n, uint32_t baud) {
  if (n < 3) {
    if (!usart_states[n]) {
      usart_states[n] = (usart_state_t*)calloc(1, sizeof(usart_state_t));
      usart_states[n]->rx_buf = (uint8_t*)malloc(USART_BUFSIZE);
      usart_states[n]->tx_buf = (uint8_t*)malloc(USART_BUFSIZE);
    }
  }
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    switch (n) {
      case 0:
        SET_AS_OUTPUT(PORTA, 0);
        SET_AS_INPUT(PORTA, 1);
        break;
      case 1:
        SET_AS_OUTPUT(PORTC, 0);
        SET_AS_INPUT(PORTC, 1);
        break;
      case 2:
        SET_AS_OUTPUT(PORTF, 0);
        SET_AS_INPUT(PORTF, 1);
        break;
      default:
        return;
    }
    dev[n]->BAUD = (64 / 16) * (F_CPU / baud);
    dev[n]->CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_PMODE_DISABLED_gc |
                    USART_SBMODE_1BIT_gc | USART_CHSIZE_8BIT_gc;
    dev[n]->CTRLB = USART_RXEN_bm | USART_TXEN_bm | USART_RXMODE_NORMAL_gc;
    dev[n]->CTRLA = USART_RXCIE_bm;  // interrupts
  }
}

void usart_putc(uint8_t n, char c) {
  if (n >= 3 || !usart_states[n]) return;
  while (usart_states[n]->tx_buflen >= USART_BUFSIZE);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    uint8_t i = (usart_states[n]->tx_bufindex + usart_states[n]->tx_buflen++) %
                USART_BUFSIZE;
    usart_states[n]->tx_buf[i] = c;
    dev[n]->STATUS |= USART_TXCIF_bm;
    dev[n]->CTRLA |= USART_DREIE_bm;
  }
}

uint8_t usart_available(uint8_t n) {
  return (n < 3 && usart_states[n]) ? usart_states[n]->rx_buflen : 0;
}

void usart_tx_flush(uint8_t n) {
  if (n >= 3 || !usart_states[n]) return;
  while (usart_states[n]->tx_buflen);
  while (dev[n]->CTRLA & USART_DREIE_bm);
  while (!(dev[n]->STATUS & USART_TXCIF_bm));
}

uint8_t usart_getc(uint8_t n) {
  if (n >= 3 || !usart_states[n] || usart_states[n]->rx_buflen == 0) return 0;
  uint8_t c;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    c = usart_states[n]->rx_buf[usart_states[n]->rx_bufindex++];
    usart_states[n]->rx_bufindex %= USART_BUFSIZE;
    usart_states[n]->rx_buflen--;
  }
  return c;
}

void usart_print(uint8_t n, const char* c) {
  while (*c) usart_putc(n, *(c++));
}

void usart_print_P(uint8_t n, const char* c) {
  for (uint8_t x; (x = pgm_read_byte(c++));) {
    usart_putc(n, x);
  }
}

void usart_write_P(uint8_t n, const char* c, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    uint8_t x = pgm_read_byte(c++);
    usart_putc(n, x);
  }
}

// Write 1 byte from buffer if buffer not empty
static inline void write_ready_isr(uint8_t n) {
  if (!usart_states[n]) return;
  if (usart_states[n]->tx_buflen > 0) {
    dev[n]->TXDATAL = usart_states[n]->tx_buf[usart_states[n]->tx_bufindex++];
    usart_states[n]->tx_bufindex %= USART_BUFSIZE;
    usart_states[n]->tx_buflen--;
    if (usart_states[n]->tx_buflen == 0) {
      dev[n]->CTRLA &= ~USART_DREIE_bm;
    }
  }
}
ISR(USART0_DRE_vect, ISR_BLOCK) { write_ready_isr(0); }
ISR(USART1_DRE_vect, ISR_BLOCK) { write_ready_isr(1); }
ISR(USART2_DRE_vect, ISR_BLOCK) { write_ready_isr(2); }

// Read 1 byte to buffer whenever a byte arrives. If buffer full, drop byte.
static inline void read_avail_isr(uint8_t n) {
  uint8_t c = dev[n]->RXDATAL;
  if (!usart_states[n]) return;
  if (usart_states[n]->rx_buflen < USART_BUFSIZE) {
    uint8_t i = (usart_states[n]->rx_bufindex + usart_states[n]->rx_buflen++) %
                USART_BUFSIZE;
    usart_states[n]->rx_buf[i] = c;
  }
}
ISR(USART0_RXC_vect, ISR_BLOCK) { read_avail_isr(0); }
ISR(USART1_RXC_vect, ISR_BLOCK) { read_avail_isr(1); }
ISR(USART2_RXC_vect, ISR_BLOCK) { read_avail_isr(2); }
