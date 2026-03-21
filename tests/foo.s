; Setup Stack Pointer
    ldi r28, 0xFF
    ldi r29, 0x7F
    out 0x3D, r28 ; SPL
    out 0x3E, r29 ; SPH

.global foo
foo:
    push r28
    push r29
    in r28, 0x3D ; SPL
    in r29, 0x3E ; SPH
    sbiw r28, 2
    out 0x3D, r28
    out 0x3E, r29
    ldi r24, 0x05
    ldi r25, 0x00
    push r24
    push r25
    movw r30, r28
    adiw r30, 1
    pop r25
    pop r24
    st Z, r24
    std Z+1, r25
L_end_foo:
    adiw r28, 2
    out 0x3D, r28
    out 0x3E, r29
    pop r29
    pop r28
    ret

