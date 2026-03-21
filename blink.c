struct PORT_struct {
  uint8_t DIR;
  uint8_t DIRSET;
  uint8_t DIRCLR;
  uint8_t DIRTGL;
  uint8_t OUT;
  uint8_t OUTSET;
  uint8_t OUTCLR;
  uint8_t OUTTGL;
  uint8_t IN;
  uint8_t reserved_2[23];
};
#define PORTC (*(struct PORT_struct *)0x0440)
#define PORTA (*(struct PORT_struct *)0x0400)

int main(void) {
  uint8_t count = 0;
  uint8_t current = 0;
  PORTC.DIRSET = 1<<2;
  PORTA.DIRSET = 1<<7;
  while (1) {
    for (uint16_t i = 0; i < 20000; i++);
    if (count > 9) {
      if (current == 0) {
        current = 1;
      } else {
        current = 0;
      }
      count = 0;
    }
    count = count + 1;
    if (current == 0) {
      PORTC.OUTTGL = 1<<2;
    } else {
      PORTA.OUTTGL = 1<<7;
    }
  }
  return 1;
}
