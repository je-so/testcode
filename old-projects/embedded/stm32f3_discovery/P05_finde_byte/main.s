	.syntax	unified
	.cpu	cortex-m4
	.fpu	fpv4-sp-d16

	.global	main

	.text
	.align	2
	.thumb
	.thumb_func
main:
	bl	init_timer
	bl	start_timer
	ldr	r0, =data
	ldr	r1, =enddata
	bl	find_zero_4
	push	{ r0 }
	bl	read_timer
	movs	r1, #0x01000000
	subs	r0, r1, r0
	pop	{ r1 }
	ldr	r2, =findpos1
	subs	r4, r2, r1 /* r4 == 0 ==> result OK */
	ldr	r2, =data
	subs	r2, r1, r2 /* r2 == number of searched bytes */
	udiv	r3, r0, r2 /* r3 == cycles per byte */
.end:
	b	.end	

	.align	4
data:
	.ascii "asfohsa dfsoasfd aoq409 jadsf p ok"
findpos1:
	.ascii "\0"
	.ascii "asfohsa dfsoasfd aoq409 jadsf p ok"
enddata:

	.thumb_func
	.align	2
find_zero_4: /* assumes (r1 - r0) % 4 == 0 */
	cmp	r0, r1
	bcs	.not_found
/*
   r2  = *(uint32_t*)r0;
   r0 += 4;
   r3  = (~ r2) & (r2 - 0x01010101) & 0x80808080

   ! r3 != 0 ==> 0 byte in r2 gefunden !
*/
	ldr	r2, [r0], #4
	mvns	r3, r2
	subs	r2, #0x01010101
	ands	r3, r2
	ands	r3, #0x80808080
	beq	find_zero_4
	subs	r0, #4
	tst	r3, #0x80
	bne	.found
	adds	r0, #1
	tst	r3, #0x8000
	bne	.found
	adds	r0, #1
	tst	r3, #0x800000
	bne	.found
	adds	r0, #1
.found:
	bx	lr
.not_found:
	mov	r0, #0
	bx	lr

	.thumb_func
init_timer:
	.equ	HW_REGISTER_BASEADDR_SCS, 0xE000E000
	.equ	SYSTICK_CTRL, 0x10
	.equ	SYSTICK_LOAD, 0x14
	.equ	SYSTICK_VAL, 0x18
	ldr	r0, =HW_REGISTER_BASEADDR_SCS
	ldr	r1, [r0, #SYSTICK_CTRL]
	bics	r1, #7
	orrs	r1, #4
	str	r1, [r0, #SYSTICK_CTRL]
	movs	r1, #0x00ffffff
	str	r1, [r0, #SYSTICK_LOAD]
	bx	lr

	.thumb_func
start_timer:
	ldr	r0, =HW_REGISTER_BASEADDR_SCS
	ldr	r1, [r0, #SYSTICK_CTRL]
	bics	r1, #1
	str	r1, [r0, #SYSTICK_CTRL] /* disable timer */
	mov	r2, #0
	str	r2, [r0, #SYSTICK_VAL] /* clear counter */
	orrs	r1, #1
	str	r1, [r0, #SYSTICK_CTRL] /* enable timer */
	bx	lr

       .thumb_func
read_timer:
        ldr     r0, =HW_REGISTER_BASEADDR_SCS
        ldr     r0, [r0, #SYSTICK_VAL]
	bx	lr
