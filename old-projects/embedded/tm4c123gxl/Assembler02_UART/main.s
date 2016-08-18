	.syntax	unified
	.cpu	cortex-m4
	.fpu	fpv4-sp-d16

	.global	main

	.text
	.align	2
	.thumb

	.thumb_func
	.type	calc_area, %function
calc_area: /* uint32_t (r0: uint32_t length, r1: uint32_t width) */
	mul	r0, r0, r1  /* area = length * width */
	bx	lr
	.size	calc_area, .-calc_area

	.thumb_func
	.type	print_number, %function
print_number: /* void (r0: uint32_t len, r1: uint8_t buffer[len], r2: uint32_t nr) */
	push	{r4-r7}
	cmp	r0, #0
	beq	.return1
	movs	r5, #10 
	movs	r3, #1     /* base = 1 */
	movw	r4, #39321
	movt	r4, #6553  /* r4 = 0xffffffff/10 */
.mul_base:
	mul	r6, r3, r5 /* r6 = base * 10 */
	cmp	r2, r6     
	bcc	.L1        /* if nr < base * 10 goto .L1 */
	movs	r3, r6     /* base *= 10 */
	cmp	r4, r3   
	bcs	.mul_base  /* if base <= 0xffffffff/10 goto .mul_base */

.L1:
	movs	r4, #0 /* i = 0 */
.fill_buffer:
	udiv	r6, r2, r3 /* r6 = nr / base */
	mul	r7, r6, r3 /* r7 = (nr/base)*base */
	subs	r2, r2, r7 /* nr = nr % base */
	adds	r6, r6, #'0'
	strb	r6, [r1, r4] /* buffer[i] = '0' + (nr/base) */
	udiv	r3, r3, r5   /* base /= 10 */
	adds	r4, #1 /* i++ */
	cmp	r4, r0
	beq	.end_of_buffer /* if i == len goto .end_of_buffer */
	cmp	r3, #0
	bne	.fill_buffer
	strb	r3, [r1, r4] /* buffer[i] = 0 */
	b	.return1
.end_of_buffer:
	subs	r4, #1
	movs	r3, #0
	strb	r3, [r1, r4] /* buffer[i-1] = 0 */
.return1:
	pop	{r4-r7}
	bx	lr
	.size	print_number, .-print_number

	.thumb_func
	.type	scan_number, %function
scan_number: /* bool scan_number(r0: const uint8_t buffer[], *out* r1: uint32_t *nr) */
	push	{r4}
	movs	r4, #10
	movs	r2, #0 /* val = 0 */
.next_digit:
	ldrb	r3, [r0], #1 /* r3 = *buffer++ */
	cmp	r3, #0
	beq	.return2
	subs	r3, #'0'     /* r3 -= '0' */
	bcc	.return2_err /* r3 < 0 ==> ERROR */
	cmp	r3, #9
	bhi	.return2_err /* r3 > 9 ==> ERROR */
	mul	r2, r2, r4 /* val *= 10 */
	adds	r2, r3 /* val += r3 */
	b	.next_digit

.return2:
	mov	r0, #1 /* true == OK */
	str	r2, [r1] /* *nr = val */
	pop	{r4}
	bx	lr
.return2_err:
	mov	r0, #0 /* false == ERROR */
	pop	{r4}
	bx	lr
	.size	scan_number, .-scan_number

	.section	.rodata
	.align	2
.CSTARTMSG:
	.ascii	"\n== Rectangle area calculator =="
.CNEWLINE:
	.ascii	"\n\0"
.CENTERLEN:
	.ascii	"\nEnter length (0..20): \0"
.CENTERWIDTH:
	.ascii	"\nEnter width (0..20): \0"
.CRESULT:
	.ascii	"\nlength*width = \0"

	.text
	.align	2
	.thumb
	.thumb_func
	.type	main, %function
main:
	sub	sp, sp, #24
	add	r7, sp, #0
	bl	init_uart
.loop1:
	movw	r0, #:lower16:.CSTARTMSG
	movt	r0, #:upper16:.CSTARTMSG
	bl	write_uart
.enterlen:
	movw	r0, #:lower16:.CENTERLEN
	movt	r0, #:upper16:.CENTERLEN
	bl	write_uart
	movs	r0, #10 /* r0 = sizeof(buffer) */
	mov	r1, r7  /* r1 = &buffer */
	bl	readline_uart /* (sizeof(buffer), buffer) */
	mov	r0, r7
	add	r1, r7, #16
	bl	scan_number /* (&buffer, &len) */
	cmp	r0, #1
	bne	.enterlen
	ldr	r3, [r7, #16] /* r3 = len */
	cmp	r3, #20 /* len > 20 */
	bhi	.enterlen /* ==> repeat */
.enterwidth:
	movw	r0, #:lower16:.CENTERWIDTH
	movt	r0, #:upper16:.CENTERWIDTH
	bl	write_uart
	movs	r0, #10 /* r0 = sizeof(buffer) */
	mov	r1, r7  /* r1 = &buffer */
	bl	readline_uart
	mov	r0, r7
	add	r1, r7, #12
	bl	scan_number /* (&buffer, &width) */
	cmp	r0, #1
	bne	.enterwidth /* scan_number() != OK => repeat */
	ldr	r3, [r7, #12] /* r3 = width */
	cmp	r3, #20 /* r3 > 20 */
	bhi	.enterwidth /* ==> repeat */
	ldr	r0, [r7, #16]
	ldr	r1, [r7, #12]
	bl	calc_area /* calc_area(len, width) */
	str	r0, [r7, #20] /* area = calc_area(len, width) */
	movw	r0, #:lower16:.CRESULT
	movt	r0, #:upper16:.CRESULT
	bl	write_uart
	movs	r0, #10
	mov	r1, r7
	ldr	r2, [r7, #20]
	bl	print_number /* print_number(sizeof(buffer), &buffer, area) */
	mov	r0, r7
	bl	write_uart /* write_uart(&buffer) */
	movw	r0, #:lower16:.CNEWLINE
	movt	r0, #:upper16:.CNEWLINE
	bl	write_uart /* write_uart(&CNEWLINE) */
	b	.loop1
	.size	main, .-main
