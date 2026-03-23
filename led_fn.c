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

#define PORTA (*(struct PORT_struct*)0x0400)

int turn_on(struct PORT_struct* port, uint8_t pin) {
  (*port).DIRSET = 1 << pin;
  (*port).OUTSET = 1 << pin;
  return 1;
}

int main(void) {
  turn_on(&PORTA, 7);
  return 0;
}