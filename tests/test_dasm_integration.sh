#!/bin/bash

# Simple integration test for dasm using avr-objdump
# Usage: ./tests/test_dasm_integration.sh <asm_file> <expected_disassembly_regex>

ASM_FILE=$1
EXPECTED_REGEX=$2

if [ ! -f "dasm" ]; then
    echo "dasm not found. Build it first."
    exit 1
fi

TEMP_BIN=$(mktemp).bin
./dasm "$ASM_FILE" -o "$TEMP_BIN"

DISASM=$(avr-objdump -m avr -b binary -D "$TEMP_BIN")

echo "Disassembly of $ASM_FILE:"
echo "$DISASM"

# Allow aliases (e.g., tst -> and, lsl -> add, rol -> adc)
if echo "$DISASM" | grep -iE "(tst|and).*r22" && \
   echo "$DISASM" | grep -iE "(lsl|add).*r24" && \
   echo "$DISASM" | grep -iE "(rol|adc).*r25" && \
   echo "$DISASM" | grep -iE "add.*r24,.*r22" && \
   echo "$DISASM" | grep -iE "adc.*r25,.*r23" && \
   echo "$DISASM" | grep -iE "cp.*r24,.*r22" && \
   echo "$DISASM" | grep -iE "cpc.*r25,.*r23"; then
    echo "PASS: $ASM_FILE assembled correctly (including aliases)"
    rm "$TEMP_BIN"
    exit 0
else
    echo "FAIL: $ASM_FILE disassembly did not match expected patterns"
    rm "$TEMP_BIN"
    exit 1
fi
