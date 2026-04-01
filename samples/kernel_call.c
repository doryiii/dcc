// kernel function looks like this
// __attribute__((noinline)) void blink_porta_7(void) {
//   PORTA.OUTTGL = 1 << 7;
// }

int main(void) {
  PORTA.DIRSET = 1 << 7;
  while (1) {
    for (uint16_t i = 0; i < 30000; i++);
    blink_porta_7();
  }
  return 1;
}
