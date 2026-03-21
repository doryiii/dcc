; Setup Stack Pointer
    ldi r28, 0xFF
    ldi r29, 0x7F
    out 0x3D, r28 ; SPL
    out 0x3E, r29 ; SPH

.global main
main:
    push r28
    push r29
    in r28, 0x3D ; SPL
    in r29, 0x3E ; SPH
    sbiw r28, 2
    out 0x3D, r28
    out 0x3E, r29
    ldi r24, 0x02
    ldi r25, 0x00
    push r24
    push r25
    ldi r24, 0x01
    ldi r25, 0x00
    pop r23
    pop r22
    tst r22
    breq L1
L0:
    lsl r24
    rol r25
    dec r22
    brne L0
L1:
    push r24
    push r25
    ldi r24, 0x40
    ldi r25, 0x04
    movw r30, r24
    adiw r30, 1
    pop r25
    pop r24
    st Z, r24
L2:
    ldi r24, 0x01
    ldi r25, 0x00
    tst r24
    breq L3
    ldi r24, 0x00
    ldi r25, 0x00
    push r24
    push r25
    movw r30, r28
    pop r25
    pop r24
    st Z, r24
    std Z+1, r25
L4:
    ldi r24, 0x10
    ldi r25, 0x27
    push r24
    push r25
    ldd r24, Y+0
    ldd r25, Y+1
    pop r23
    pop r22
    cp r24, r22
    cpc r25, r23
    brlt L6
    ldi r24, 0
    rjmp L7
L6:
    ldi r24, 1
L7:
    clr r25
    tst r24
    breq L5
    movw r30, r28
    ld r24, Z
    ldd r25, Z+1
    adiw r24, 1
    st Z, r24
    std Z+1, r25
    rjmp L4
L5:
    ldi r24, 0x02
    ldi r25, 0x00
    push r24
    push r25
    ldi r24, 0x01
    ldi r25, 0x00
    pop r23
    pop r22
    tst r22
    breq L9
L8:
    lsl r24
    rol r25
    dec r22
    brne L8
L9:
    push r24
    push r25
    ldi r24, 0x40
    ldi r25, 0x04
    movw r30, r24
    adiw r30, 7
    pop r25
    pop r24
    st Z, r24
    rjmp L2
L3:
    ldi r24, 0x01
    ldi r25, 0x00
    rjmp L_end_main
L_end_main:
    adiw r28, 2
    out 0x3D, r28
    out 0x3E, r29
    pop r29
    pop r28
    ret

