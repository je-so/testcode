	.syntax	unified
	.cpu	cortex-m4
	.fpu	fpv4-sp-d16

	.global	init_uart
	.global	write_uart
	.global	readline_uart

	.text
	.align	2
	.thumb
	.thumb_func
	.type	init_pll, %function
init_pll:
	.equ	SYSCTL_RCC2_R, 0x400FE070
	.equ	SYSCTL_RCC2_USERCC2, 0x80000000 /* use RCC2 */
	.equ	SYSCTL_RCC2_BYPASS2, 0x00000800 
	.equ	SYSCTL_RCC2_OSCSRC2_M, 0x00000070 /* source 2 mask */
	.equ	SYSCTL_RCC2_PWRDN2, 0x00002000 /* power down */
	.equ	SYSCTL_RCC2_DIV400, 0x40000000
	.equ	SYSCTL_RCC_R, 0x400FE060
	.equ	SYSCTL_RCC_XTAL_M, 0x000007C0 /* freq mask */
	.equ	SYSCTL_RCC_XTAL_16MHZ, 0x00000540
	.equ	SYSCTL_RIS_R, 0x400FE050
	.equ	SYSCTL_RIS_PLLLRIS, 0x00000040 /* lock interrupt status */
	ldr	r3, =SYSCTL_RCC2_R
	ldr	r0, [r3]
	orrs	r0, #SYSCTL_RCC2_USERCC2
	orrs	r0, #SYSCTL_RCC2_BYPASS2
	str	r0, [r3]
	ldr	r2, =SYSCTL_RCC_R
	ldr	r0, [r2]
	bics	r0, #SYSCTL_RCC_XTAL_M     /* r0 &= ~SYSCTL_RCC_XTAL_M */
	orrs	r0, #SYSCTL_RCC_XTAL_16MHZ /* r0 |= SYSCTL_RCC_XTAL_16MHZ */
	str	r0, [r2]
	ldr	r0, [r3]
	bics	r0, #SYSCTL_RCC2_OSCSRC2_M /* r0 &= ~SYSCTL_RCC2_OSCSRC2_M */
	bics	r0, #SYSCTL_RCC2_PWRDN2 /* r0 &= ~SYSCTL_RCC2_PWRDN2 */
	str	r0, [r3] /* SYSCTL_RCC2_R = r0 */
	/* 80 MHz clock */
	ldr	r0, [r3]
	orrs	r0, SYSCTL_RCC2_DIV400 /* SYSCTL_RCC2_R |= SYSCTL_RCC2_DIV400 */
	bics	r0, #0x1FC00000 /* mask freq */
	orrs	r0, #0x01000000 
	str	r0, [r3]
	/* wait for pll */
	ldr	r2, =SYSCTL_RIS_R
.wait1:
	ldr	r0, [r2]
	ands	r0, #SYSCTL_RIS_PLLLRIS
	beq	.wait1
	/* switch on */
	ldr	r0, [r3]
	bics	r0, #SYSCTL_RCC2_BYPASS2
	str	r0, [r3] /* SYSCTL_RCC2_R &= ~SYSCTL_RCC2_BYPASS2 */
	bx	lr
	.size	init_pll, .-init_pll

	.type	init_uart, %function
init_uart:
	.equ	SYSCTL_RCGC1_R, 0x400FE104
	.equ	SYSCTL_RCGC2_R, 0x400FE108
	.equ	SYSCTL_RCGC1_UART0, 0x00000001
	.equ	SYSCTL_RCGC2_GPIOA, 0x00000001
	.equ	UART0_CTL_R, 0x4000C030
	.equ	UART_CTL_UARTEN, 0x00000001
	.equ	UART0_IBRD_R, 0x4000C024
	.equ	UART0_FBRD_R, 0x4000C028
	.equ	UART0_LCRH_R, 0x4000C02C
	.equ	UART_LCRH_WLEN_8, 0x00000060
	.equ	UART_LCRH_FEN, 0x00000010
	.equ	GPIO_PORTA_AFSEL_R, 0x40004420
	.equ	GPIO_PORTA_DEN_R, 0x4000451C
	.equ	GPIO_PORTA_AMSEL_R, 0x40004528
	.equ	GPIO_PORTA_PCTL_R, 0x4000452C
	push	{lr}
	bl	init_pll
	ldr	r3, =SYSCTL_RCGC1_R
	ldr	r0, [r3]
	orrs	r0, SYSCTL_RCGC1_UART0 /* UART0 active */
	str	r0, [r3] /* SYSCTL_RCGC1_R |= SYSCTL_RCGC1_UART0 */
	ldr	r3, =SYSCTL_RCGC2_R
	ldr	r0, [r3]
	orrs	r0, SYSCTL_RCGC2_GPIOA /* port A active */
	str	r0, [r3] /* SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOA */
	ldr	r3, =UART0_IBRD_R
	ldr	r0, [r3, #(UART0_CTL_R-UART0_IBRD_R)]
	bics	r0, #UART_CTL_UARTEN /* disable uart */
	str	r0, [r3, #(UART0_CTL_R-UART0_IBRD_R)]
	movs	r0, #43  /* 80,000,000 / (16 * 115200) == 43.402778 */
	str	r0, [r3]
	movs	r0, #26  /* == 0.402778*64 */
	str	r0, [r3, #(UART0_FBRD_R-UART0_IBRD_R)] /* 115200 baud */
	movs	r0, #(UART_LCRH_WLEN_8+UART_LCRH_FEN)  /* 8N1 + FIFOs */
	str	r0, [r3, #(UART0_LCRH_R-UART0_IBRD_R)]
	ldr	r0, [r3, #(UART0_CTL_R-UART0_IBRD_R)]
	orrs	r0, #UART_CTL_UARTEN /* enable uart */
	str	r0, [r3, #(UART0_CTL_R-UART0_IBRD_R)]
	ldr	r3, =GPIO_PORTA_AFSEL_R
	ldr	r0, [r3]
	orrs	r0, #3 /* alt func on port A 1..0 */
	str	r0, [r3]
	ldr	r0, [r3, #(GPIO_PORTA_DEN_R-GPIO_PORTA_AFSEL_R)]
	orrs	r0, #3 /* enable digital I/O on port A 1..0 */
	str	r0, [r3, #(GPIO_PORTA_DEN_R-GPIO_PORTA_AFSEL_R)]
	ldr	r0, [r3, #(GPIO_PORTA_PCTL_R-GPIO_PORTA_AFSEL_R)]
	bics	r0, #0xFF
	orrs	r0, #0x11 /* port A 1..0 config as uart */
	str	r0, [r3, #(GPIO_PORTA_PCTL_R-GPIO_PORTA_AFSEL_R)]
	ldr	r0, [r3, #(GPIO_PORTA_AMSEL_R-GPIO_PORTA_AFSEL_R)]
	bics	r0, #3 /* disable analog on port A 1..0 */
	str	r0, [r3, #(GPIO_PORTA_AMSEL_R-GPIO_PORTA_AFSEL_R)]
	pop	{pc}
	.size	init_uart, .-init_uart

	.type	write_uart, %function
write_uart: /* void (r0: const uint8_t buffer[]) */
	.equ	UART0_DR_R, 0x4000C000
	.equ	UART0_FR_R, 0x4000C018
	.equ	UART_FR_TXFF, 0x00000020 // send FIFO full
	ldr	r3, =UART0_DR_R
	b	.get_next_byte
.send_next: /* while ((UART0_FR_R & UART_FR_TXFF)) ; */
	ldr	r2, [r3, #(UART0_FR_R-UART0_DR_R)] /* r2 = UART0_FR_R */
	ands	r2, #UART_FR_TXFF
	bne	.send_next
	str	r1, [r3] /* UART0_DR_R = buffer[-1] */
.get_next_byte:
	ldrb	r1, [r0], #1 /* r1 = *buffer++ */
	cmp	r1, #0
	bne	.send_next
	bx	lr
	.size	write_uart, .-write_uart

	.type	readline_uart, %function
readline_uart: /* void (r0: uint32_t len, *out* r1: uint8_t buffer[len]) */
	.equ	UART_FR_RXFE, 0x00000010 /* receive FIFO empty */
	cmp	r0, #0
	beq	.return3 
	ldr	r3, =UART0_DR_R
.recv_next: /* while ((UART0_FR_R & UART_FR_RXFE)) ; */
	ldr	r2, [r3, #(UART0_FR_R-UART0_DR_R)] /* r2 = UART0_FR_R */
	ands	r2, #UART_FR_RXFE
	bne	.recv_next
	ldr	r2, [r3]
	strb	r2, [r1], #1 /* buffer[0] = (uint8_t) UART0_DR_R; ++buffer */
	/* echo read character */
	push	{r2}
.send_wait2: /* while ((UART0_FR_R & UART_FR_TXFF)) ; */
	ldr	r2, [r3, #(UART0_FR_R-UART0_DR_R)] /* r2 = UART0_FR_R */
	ands	r2, #UART_FR_TXFF
	bne	.send_wait2
	pop	{r2} 
	str	r2, [r3] /* UART0_DR_R = buffer[-1] */
	subs	r0, #1
	beq	.recv_end
	subs	r2, #'\n'
	bne	.recv_next
.recv_end:
	ands	r2, r0
	strb	r2, [r1, #-1] /* buffer[-1] = 0 */
.return3:
	bx	lr
	.size	readline_uart, .-readline_uart
