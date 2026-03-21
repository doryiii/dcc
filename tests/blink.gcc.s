	.file	"blink.gcc.c"
__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__RAMPZ__ = 0x3b
__CCP__ = 0x34
__tmp_reg__ = 0
__zero_reg__ = 1
	.text
.global	main
	.type	main, @function
main:
	push r28
	push r29
	rcall .
	in r28,__SP_L__
	in r29,__SP_H__
/* prologue: function */
/* frame size = 2 */
/* stack size = 4 */
.L__stack_usage = 4
	ldi r24,lo8(64)
	ldi r25,lo8(4)
	ldi r18,lo8(4)
	movw r30,r24
	std Z+1,r18
.L4:
	std Y+1,__zero_reg__
	std Y+2,__zero_reg__
	rjmp .L2
.L3:
	ldd r24,Y+1
	ldd r25,Y+2
	adiw r24,1
	std Y+1,r24
	std Y+2,r25
.L2:
	ldd r24,Y+1
	ldd r25,Y+2
	cpi r24,16
	ldi r31,39
	cpc r25,r31
	brlo .L3
	ldi r24,lo8(64)
	ldi r25,lo8(4)
	ldi r18,lo8(4)
	movw r30,r24
	std Z+7,r18
	rjmp .L4
	.size	main, .-main
	.ident	"GCC: (GNU) 15.1.0"
