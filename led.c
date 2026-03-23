// typedef char uint8_t;
// typedef short uint16_t;

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

int main(void) {
  PORTA.DIRSET = 1 << 7;
  PORTA.OUTSET = 1 << 7;
  return 1;
}
