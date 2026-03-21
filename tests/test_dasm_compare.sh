#!/bin/bash
# Compares dasm output against avr-gcc byte-for-byte
ASM_FILE=$1

if [ ! -f "dasm" ]; then
    echo "dasm not found. Build it first."
    exit 1
fi

TEMP_BIN_DASM=$(mktemp).bin
TEMP_ELF_GCC=$(mktemp).elf
TEMP_BIN_GCC=$(mktemp).bin

# Hide ./dasm output to make test logs cleaner
./dasm "$ASM_FILE" -o "$TEMP_BIN_DASM" > /dev/null
if [ $? -ne 0 ]; then
    echo "FAIL: dasm failed on $ASM_FILE"
    rm -f "$TEMP_BIN_DASM" "$TEMP_ELF_GCC" "$TEMP_BIN_GCC"
    exit 1
fi

avr-gcc -mmcu=avr128db28 -nostartfiles -nodefaultlibs -Wl,--no-relax -o "$TEMP_ELF_GCC" "$ASM_FILE" > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: avr-gcc failed on $ASM_FILE"
    rm -f "$TEMP_BIN_DASM" "$TEMP_ELF_GCC" "$TEMP_BIN_GCC"
    exit 1
fi

avr-objcopy -j .text -O binary "$TEMP_ELF_GCC" "$TEMP_BIN_GCC"

cmp -s "$TEMP_BIN_DASM" "$TEMP_BIN_GCC"
if [ $? -eq 0 ]; then
    echo "PASS: $ASM_FILE perfectly matches avr-gcc output"
    rm -f "$TEMP_BIN_DASM" "$TEMP_ELF_GCC" "$TEMP_BIN_GCC"
    exit 0
else
    echo "FAIL: $ASM_FILE differs from avr-gcc output"
    rm -f "$TEMP_BIN_DASM" "$TEMP_ELF_GCC" "$TEMP_BIN_GCC"
    exit 1
fi
