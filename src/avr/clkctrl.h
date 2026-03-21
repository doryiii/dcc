#ifndef _CLKCTRL_H
#define _CLKCTRL_H

#include <stdint.h>

void set_clock();
void start_rtc_pit_isr(uint8_t period_setting, void (*isr_func)(void));

#endif // _CLKCTRL_H
