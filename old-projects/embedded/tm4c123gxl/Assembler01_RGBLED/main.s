	.syntax unified
	.cpu cortex-m4
	.fpu fpv4-sp-d16

	.global	main

	.text
	.align	2
	.thumb
	.thumb_func
main:
	bl	init_portf

	/* register r3 contains pointer to portf data register */
	.equ PORTF, 0x400253FC
	ldr 	r3, =PORTF

.loop1:

	/* load state of buttons SW1 and SW2 into r0 */
	ldr	r1, [r3]
	and 	r0, r1, #0x11 /* only bits PF4(SW1),PF0(SW2) are of interest */
   
	/* r1 contains color of LED: 0 ==> LED is off */
   	movs.n	r1, #0

	/* set color of LED (r1) depending on pressed keys */
	cmp	r0, #0x00 /* SW1 and SW2 pressed ? */
	it 	eq
	moveq 	r1, #4    /* set LED blue if both pressed */
	cmp	r0, #0x01 /* only SW1 pressed ? */
	it	eq
	moveq	r1, #2    /* set LED red if SW1 pressed */
	cmp 	r0, #0x10 /* only SW2 pressed ? */
	it	eq
	moveq	r1, #8    /* set LED green if SW2 pressed */

	str	r1, [r3]  /* set GPIO LED output pins */
	b	.loop1

	.thumb_func
init_portf:
	.equ SYSCTL_RCGC2_R, 0x400FE108
	.equ GPIO_PORTF_LOCK_R, 0x40025520
	.equ GPIO_PORTF_CR_R, 0x40025524
	.equ GPIO_PORTF_AMSEL_R, 0x40025528
	.equ GPIO_PORTF_PCTL_R, 0x4002552C
	.equ GPIO_PORTF_DIR_R, 0x40025400
	.equ GPIO_PORTF_AFSEL_R, 0x40025420
	.equ GPIO_PORTF_PUR_R, 0x40025510
	.equ GPIO_PORTF_DEN_R, 0x4002551C

	ldr	r1, =SYSCTL_RCGC2_R
	ldr	r0, [r1]
	orr	r0, r0, 0x20  /* enable port F clock */
	str	r0, [r1]
	ldr	r0, [r1]      /* delay 1 */
	ldr	r1, =GPIO_PORTF_LOCK_R /* delay 2 */
	movw	r0, #0x434b   /* delay 3 */
	movt	r0, #0x4c4f   /* r0 = 0x4c4f434b */
	str	r0, [r1]      /* unlock PortF PF0 */
	movs	r0, #0x1F
	str	r0, [r1, #(GPIO_PORTF_CR_R-GPIO_PORTF_LOCK_R)]    /* allow changes to PF4-0 */
	movs	r0, #0x00
	str	r0, [r1, #(GPIO_PORTF_AMSEL_R-GPIO_PORTF_LOCK_R)] /* disable analog function */
	str	r0, [r1, #(GPIO_PORTF_PCTL_R-GPIO_PORTF_LOCK_R)]  /* clear bit PCTL */
	movs	r0, #0x0e
	ldr	r1, =GPIO_PORTF_DIR_R
	str	r0, [r1]      /* PF4,PF0 input, PF3,PF2,PF1 output */
	movs	r0, #0x00
	str	r0, [r1, #(GPIO_PORTF_AFSEL_R-GPIO_PORTF_DIR_R)] /* no alternate function */
	movs	r0, #0x11
	str	r0, [r1, #(GPIO_PORTF_PUR_R-GPIO_PORTF_DIR_R)] /* enable pullup resistors on PF4,PF0 */
	movs	r0, #0x1F
	str	r0, [r1, #(GPIO_PORTF_DEN_R-GPIO_PORTF_DIR_R)] /* enable digital pins PF4-PF0 */
	bx	lr
