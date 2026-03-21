#include "clkctrl.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <util/atomic.h>

#if F_CPU == 1000000U
#define FRQSEL CLKCTRL_FRQSEL_1M_gc;
#elif F_CPU == 2000000U
#define FRQSEL CLKCTRL_FRQSEL_2M_gc;
#elif F_CPU == 3000000U
#define FRQSEL CLKCTRL_FRQSEL_3M_gc;
#elif F_CPU == 4000000U
#define FRQSEL CLKCTRL_FRQSEL_4M_gc;
#elif F_CPU == 8000000U
#define FRQSEL CLKCTRL_FRQSEL_8M_gc;
#elif F_CPU == 12000000U
#define FRQSEL CLKCTRL_FRQSEL_12M_gc;
#elif F_CPU == 16000000U
#define FRQSEL CLKCTRL_FRQSEL_16M_gc;
#elif F_CPU == 20000000U
#define FRQSEL CLKCTRL_FRQSEL_20M_gc;
#elif F_CPU == 24000000U
#define FRQSEL CLKCTRL_FRQSEL_24M_gc;
#else
#error invalid F_CPU
#endif

inline void set_clock() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    CPU_CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKCTRLA = CLKCTRL_CLKSEL_OSCHF_gc;
    CPU_CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKCTRLB = 0;
    CPU_CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKCTRLC = 0;
    CPU_CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKINTCTRL = 0;

    CPU_CCP = CCP_IOREG_gc;
    CLKCTRL.OSCHFCTRLA = FRQSEL;
    while (!(CLKCTRL.MCLKSTATUS & CLKCTRL_OSCHFS_bm));
  }
}

void (*rtc_pit_isr)(void) = NULL;

void start_rtc_pit_isr(uint8_t period_setting, void (*isr_func)(void)) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    while (RTC.STATUS | RTC.PITSTATUS);
    RTC.CLKSEL = RTC_CLKSEL_OSC32K_gc;
    RTC.PITINTCTRL = RTC_PI_bm;
    // 32768 cycles = 1s
    RTC.PITCTRLA = period_setting | RTC_PITEN_bm;
    rtc_pit_isr = isr_func;
  }
}

ISR(RTC_PIT_vect) {
  if (rtc_pit_isr) (*rtc_pit_isr)();
  RTC.PITINTFLAGS = RTC_PI_bm;
}
