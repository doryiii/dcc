#ifndef _GPIO_H
#define _GPIO_H

#define SET_AS_OUTPUT(PORT, PIN) PORT.DIRSET = 1 << PIN
#define SET_AS_INPUT(PORT, PIN) PORT.DIRCLR = 1 << PIN

#define WRITE_HIGH(PORT, PIN) PORT.OUTSET = 1 << PIN
#define WRITE_LOW(PORT, PIN) PORT.OUTCLR = 1 << PIN
#define TOGGLE(PORT, PIN) PORT.OUTTGL = 1 << PIN

#define WRITE(PORT, VALUE, MASK) PORT.OUT = (PORT.OUT & ~MASK) | (VALUE & MASK)

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif

#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#endif // _GPIO_H
