//typedef char uint8_t;
//typedef short uint16_t;

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
#define PORTC (*(struct PORT_struct*)0x0440)
#define PORTA (*(struct PORT_struct*)0x0400)

int blinkLED(struct PORT_struct* port, uint8_t pin) {
  (*port).OUTTGL = 1 << pin;
}

int main(void) {
  uint8_t count = 0;
  uint8_t current = 0;
  PORTC.DIRSET = 1 << 2;
  PORTA.DIRSET = 1 << 7;
  while (1) {
    for (uint16_t i = 0; i < 30000; i++);
    if (count > 9) {
      current = !current;
      count = 0;
    }
    count = count + 1;
    if (current == 0) {
      blinkLED(&PORTC, 2);
    } else {
      blinkLED(&PORTA, 7);
    }
  }
  return 1;
}

