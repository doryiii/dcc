CC = gcc
CFLAGS = -Wall -Wextra -Isrc/compiler -Isrc/assembler
COMPILER_SRC = src/compiler/lexer.c src/compiler/ast.c src/compiler/parser.c src/compiler/codegen.c src/compiler/preprocessor.c
ASSEMBLER_SRC = src/assembler/dasm.c
AVR_SRC = src/avr/main.c src/avr/clkctrl.c src/avr/usart.c 
AVR_CFLAGS = -mmcu=avr128db28 -DAVR_TARGET -DF_CPU=24000000U

all: dcc dasm avr test_lexer test_parser test_codegen test_preprocessor test_dasm

dcc: src/compiler/main.c $(COMPILER_SRC)
	$(CC) $(CFLAGS) $^ -o $@

dasm: src/assembler/main.c $(ASSEMBLER_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test_lexer: tests/test_lexer.c src/compiler/lexer.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_parser: tests/test_parser.c $(COMPILER_SRC)
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_codegen: tests/test_codegen.c $(COMPILER_SRC)
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_preprocessor: tests/test_preprocessor.c src/compiler/preprocessor.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_dasm: tests/test_dasm.c $(ASSEMBLER_SRC) dasm
	$(CC) $(CFLAGS) tests/test_dasm.c $(ASSEMBLER_SRC) -o $@
	./$@
	./tests/test_dasm_compare.sh tests/blink.s
	./tests/test_dasm_compare.sh tests/blink.gcc.s

avr: dcc.hex
load: dcc.hex
	avrdude -p avr128db28 -c snap_updi -P usb -B 5 -Uflash:w:$^

dcc.hex: dcc.elf
	avr-objcopy -j .text -j .data -j .rodata -j .bss -O ihex $^ $@
	avr-size $^

dcc.elf: $(AVR_SRC) $(COMPILER_SRC) $(ASSEMBLER_SRC)
	avr-gcc $(AVR_CFLAGS) -Os $(CFLAGS) -Isrc/avr $^ -o $@

clean:
	rm -f test_lexer test_parser test_codegen test_preprocessor test_dasm dcc dasm dcc.elf dcc.hex tests/blink.bin tests/blink.hex
	find src/ -iname "*.o" -delete
