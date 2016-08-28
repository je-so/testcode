/* ARM Assembler (Thumb-2) implementation of startup code for TM4C123GXL board.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/
	.syntax unified
	.cpu cortex-m4
	.fpu fpv4-sp-d16

	.global	g_main_stack
	.global	g_NVIC_vectortable

	.text
	.align	2
	.thumb

	.thumb_func
reset_interrupt:
	bl	startup_init_datasegment
	bl	main
	b	.Wait /* in case main returns */

	.thumb_func
default_interrupt:
.Wait:
	b	.Wait

	.thumb_func
startup_init_datasegment:
	movw	r1, #:lower16:_romdata
	movt	r1, #:upper16:_romdata
	movw	r2, #:lower16:_data
	movt	r2, #:upper16:_data
	movw	r3, #:lower16:_edata
	movt	r3, #:upper16:_edata
	cmp	r2, r3
	bcs	.next_bss /* r2 >= r3 ==> done */
.copy_data:
	ldr	r0, [r1], #4
	str	r0, [r2], #4
	cmp	r2, r3
	bcc	.copy_data /* r2 < r3 ==> copy next */

.next_bss:
	movw	r2, #:lower16:_bss
	movt	r2, #:upper16:_bss
	movw	r3, #:lower16:_ebss
	movt	r3, #:upper16:_ebss
	movs	r0, #0
	cmp	r2, r3
	bcs	.copy_end /* r2 >= r3 ==> done */
.clear_bss:
	str	r0, [r2], #4
	cmp	r2, r3
	bcc	.clear_bss /* r2 < r3 ==> clear next */

.copy_end:
	bx	lr

	.section	.sram_address_start,"aw",%nobits
g_main_stack:
	.space	512

	.section	.rom_address_0x0,"aw",%progbits
g_NVIC_vectortable:
	.word	g_main_stack+512
	.word	reset_interrupt
	.word	NMI_Handler
	.word	HardFault_Handler
	.word	MPUFault_Handler
	.word	BusFault_Handler
	.word	UsageFault_Handler
	.space	16
	.word	SVCall_Handler
	.word	DebugMonitor_Handler
	.space	4
	.word	PendSV_Handler
	.word	systick_interrupt
	.word	GPIOPortA_Handler
	.word	GPIOPortB_Handler
	.word	GPIOPortC_Handler
	.word	GPIOPortD_Handler
	.word	GPIOPortE_Handler
	.word	UART0_Handler
	.word	UART1_Handler
	.word	SSI0_Handler
	.word	I2C0_Handler
	.word	PWMFault_Handler
	.word	PWMGenerator0_Handler
	.word	PWMGenerator1_Handler
	.word	PWMGenerator2_Handler
	.word	QuadratureEncoder0_Handler
	.word	ADCSequence0_Handler
	.word	ADCSequence1_Handler
	.word	ADCSequence2_Handler
	.word	ADCSequence3_Handler
	.word	WatchdogTimer_Handler
	.word	Timer0SubtimerA_Handler
	.word	Timer0SubtimerB_Handler
	.word	Timer1SubtimerA_Handler
	.word	Timer1SubtimerB_Handler
	.word	Timer2SubtimerA_Handler
	.word	Timer2SubtimerB_Handler
	.word	AnalogComparator0_Handler
	.word	AnalogComparator1_Handler
	.word	AnalogComparator2_Handler
	.word	SystemControl_Handler
	.word	FlashControl_Handler
	.word	GPIOPortF_Handler
	.word	GPIOPortG_Handler
	.word	GPIOPortH_Handler
	.word	UART2_Handler
	.word	SSI1_Handler
	.word	Timer3SubtimerA_Handler
	.word	Timer3SubtimerB_Handler
	.word	I2C1_Handler
	.word	QuadratureEncoder1_Handler
	.word	CAN0_Handler
	.word	CAN1_Handler
	.space	8
	.word	Hibernate_Handler
	.word	USB0_Handler
	.word	PWMGenerator3_Handler
	.word	MicroDMASoftwareTransfer_Handler
	.word	MicroDMAError_Handler
	.word	ADC1Sequence0_Handler
	.word	ADC1Sequence1_Handler
	.word	ADC1Sequence2_Handler
	.word	ADC1Sequence3_Handler
	.space	8
	.word	GPIOPortJ_Handler
	.word	GPIOPortK_Handler
	.word	GPIOPortL_Handler
	.word	SSI2_Handler
	.word	SSI3_Handler
	.word	UART3_Handler
	.word	UART4_Handler
	.word	UART5_Handler
	.word	UART6_Handler
	.word	UART7_Handler
	.space	16
	.word	I2C2_Handler
	.word	I2C3_Handler
	.word	Timer4SubtimerA_Handler
	.word	Timer4SubtimerB_Handler
	.space	80
	.word	Timer5SubtimerA_Handler
	.word	Timer5SubtimerB_Handler
	.word	WideTimer0SubtimerA_Handler
	.word	WideTimer0SubtimerB_Handler
	.word	WideTimer1SubtimerA_Handler
	.word	WideTimer1SubtimerB_Handler
	.word	WideTimer2SubtimerA_Handler
	.word	WideTimer2SubtimerB_Handler
	.word	WideTimer3SubtimerA_Handler
	.word	WideTimer3SubtimerB_Handler
	.word	WideTimer4SubtimerA_Handler
	.word	WideTimer4SubtimerB_Handler
	.word	WideTimer5SubtimerA_Handler
	.word	WideTimer5SubtimerB_Handler
	.word	FPU_Handler
	.space	8
	.word	I2C4_Handler
	.word	I2C5_Handler
	.word	GPIOPortM_Handler
	.word	GPIOPortN_Handler
	.word	QuadratureEncoder2_Handler
	.space	8
	.word	GPIOPortP_Handler
	.word	GPIOPortP1_Handler
	.word	GPIOPortP2_Handler
	.word	GPIOPortP3_Handler
	.word	GPIOPortP4_Handler
	.word	GPIOPortP5_Handler
	.word	GPIOPortP6_Handler
	.word	GPIOPortP7_Handler
	.word	GPIOPortQ_Handler
	.word	GPIOPortQ1_Handler
	.word	GPIOPortQ2_Handler
	.word	GPIOPortQ3_Handler
	.word	GPIOPortQ4_Handler
	.word	GPIOPortQ5_Handler
	.word	GPIOPortQ6_Handler
	.word	GPIOPortQ7_Handler
	.word	GPIOPortR_Handler
	.word	GPIOPortS_Handler
	.word	PWM1Generator0_Handler
	.word	PWM1Generator1_Handler
	.word	PWM1Generator2_Handler
	.word	PWM1Generator3_Handler
	.word	PWM1Fault_Handler

	.weak	default_interrupt
	.weak	reset_interrupt
	.weak	NMI_Handler
	.thumb_set NMI_Handler,default_interrupt
	.weak	HardFault_Handler
	.thumb_set HardFault_Handler,default_interrupt
	.weak	MPUFault_Handler
	.thumb_set MPUFault_Handler,default_interrupt
	.weak	BusFault_Handler
	.thumb_set BusFault_Handler,default_interrupt
	.weak	UsageFault_Handler
	.thumb_set UsageFault_Handler,default_interrupt
	.weak	SVCall_Handler
	.thumb_set SVCall_Handler,default_interrupt
	.weak	DebugMonitor_Handler
	.thumb_set DebugMonitor_Handler,default_interrupt
	.weak	PendSV_Handler
	.thumb_set PendSV_Handler,default_interrupt
	.weak	systick_interrupt
	.thumb_set systick_interrupt,default_interrupt
	.weak	GPIOPortA_Handler
	.thumb_set GPIOPortA_Handler,default_interrupt
	.weak	GPIOPortB_Handler
	.thumb_set GPIOPortB_Handler,default_interrupt
	.weak	GPIOPortC_Handler
	.thumb_set GPIOPortC_Handler,default_interrupt
	.weak	GPIOPortD_Handler
	.thumb_set GPIOPortD_Handler,default_interrupt
	.weak	GPIOPortE_Handler
	.thumb_set GPIOPortE_Handler,default_interrupt
	.weak	UART0_Handler
	.thumb_set UART0_Handler,default_interrupt
	.weak	UART1_Handler
	.thumb_set UART1_Handler,default_interrupt
	.weak	SSI0_Handler
	.thumb_set SSI0_Handler,default_interrupt
	.weak	I2C0_Handler
	.thumb_set I2C0_Handler,default_interrupt
	.weak	PWMFault_Handler
	.thumb_set PWMFault_Handler,default_interrupt
	.weak	PWMGenerator0_Handler
	.thumb_set PWMGenerator0_Handler,default_interrupt
	.weak	PWMGenerator1_Handler
	.thumb_set PWMGenerator1_Handler,default_interrupt
	.weak	PWMGenerator2_Handler
	.thumb_set PWMGenerator2_Handler,default_interrupt
	.weak	QuadratureEncoder0_Handler
	.thumb_set QuadratureEncoder0_Handler,default_interrupt
	.weak	ADCSequence0_Handler
	.thumb_set ADCSequence0_Handler,default_interrupt
	.weak	ADCSequence1_Handler
	.thumb_set ADCSequence1_Handler,default_interrupt
	.weak	ADCSequence2_Handler
	.thumb_set ADCSequence2_Handler,default_interrupt
	.weak	ADCSequence3_Handler
	.thumb_set ADCSequence3_Handler,default_interrupt
	.weak	WatchdogTimer_Handler
	.thumb_set WatchdogTimer_Handler,default_interrupt
	.weak	Timer0SubtimerA_Handler
	.thumb_set Timer0SubtimerA_Handler,default_interrupt
	.weak	Timer0SubtimerB_Handler
	.thumb_set Timer0SubtimerB_Handler,default_interrupt
	.weak	Timer1SubtimerA_Handler
	.thumb_set Timer1SubtimerA_Handler,default_interrupt
	.weak	Timer1SubtimerB_Handler
	.thumb_set Timer1SubtimerB_Handler,default_interrupt
	.weak	Timer2SubtimerA_Handler
	.thumb_set Timer2SubtimerA_Handler,default_interrupt
	.weak	Timer2SubtimerB_Handler
	.thumb_set Timer2SubtimerB_Handler,default_interrupt
	.weak	AnalogComparator0_Handler
	.thumb_set AnalogComparator0_Handler,default_interrupt
	.weak	AnalogComparator1_Handler
	.thumb_set AnalogComparator1_Handler,default_interrupt
	.weak	AnalogComparator2_Handler
	.thumb_set AnalogComparator2_Handler,default_interrupt
	.weak	SystemControl_Handler
	.thumb_set SystemControl_Handler,default_interrupt
	.weak	FlashControl_Handler
	.thumb_set FlashControl_Handler,default_interrupt
	.weak	GPIOPortF_Handler
	.thumb_set GPIOPortF_Handler,default_interrupt
	.weak	GPIOPortG_Handler
	.thumb_set GPIOPortG_Handler,default_interrupt
	.weak	GPIOPortH_Handler
	.thumb_set GPIOPortH_Handler,default_interrupt
	.weak	UART2_Handler
	.thumb_set UART2_Handler,default_interrupt
	.weak	SSI1_Handler
	.thumb_set SSI1_Handler,default_interrupt
	.weak	Timer3SubtimerA_Handler
	.thumb_set Timer3SubtimerA_Handler,default_interrupt
	.weak	Timer3SubtimerB_Handler
	.thumb_set Timer3SubtimerB_Handler,default_interrupt
	.weak	I2C1_Handler
	.thumb_set I2C1_Handler,default_interrupt
	.weak	QuadratureEncoder1_Handler
	.thumb_set QuadratureEncoder1_Handler,default_interrupt
	.weak	CAN0_Handler
	.thumb_set CAN0_Handler,default_interrupt
	.weak	CAN1_Handler
	.thumb_set CAN1_Handler,default_interrupt
	.weak	Hibernate_Handler
	.thumb_set Hibernate_Handler,default_interrupt
	.weak	USB0_Handler
	.thumb_set USB0_Handler,default_interrupt
	.weak	PWMGenerator3_Handler
	.thumb_set PWMGenerator3_Handler,default_interrupt
	.weak	MicroDMASoftwareTransfer_Handler
	.thumb_set MicroDMASoftwareTransfer_Handler,default_interrupt
	.weak	MicroDMAError_Handler
	.thumb_set MicroDMAError_Handler,default_interrupt
	.weak	ADC1Sequence0_Handler
	.thumb_set ADC1Sequence0_Handler,default_interrupt
	.weak	ADC1Sequence1_Handler
	.thumb_set ADC1Sequence1_Handler,default_interrupt
	.weak	ADC1Sequence2_Handler
	.thumb_set ADC1Sequence2_Handler,default_interrupt
	.weak	ADC1Sequence3_Handler
	.thumb_set ADC1Sequence3_Handler,default_interrupt
	.weak	GPIOPortJ_Handler
	.thumb_set GPIOPortJ_Handler,default_interrupt
	.weak	GPIOPortK_Handler
	.thumb_set GPIOPortK_Handler,default_interrupt
	.weak	GPIOPortL_Handler
	.thumb_set GPIOPortL_Handler,default_interrupt
	.weak	SSI2_Handler
	.thumb_set SSI2_Handler,default_interrupt
	.weak	SSI3_Handler
	.thumb_set SSI3_Handler,default_interrupt
	.weak	UART3_Handler
	.thumb_set UART3_Handler,default_interrupt
	.weak	UART4_Handler
	.thumb_set UART4_Handler,default_interrupt
	.weak	UART5_Handler
	.thumb_set UART5_Handler,default_interrupt
	.weak	UART6_Handler
	.thumb_set UART6_Handler,default_interrupt
	.weak	UART7_Handler
	.thumb_set UART7_Handler,default_interrupt
	.weak	I2C2_Handler
	.thumb_set I2C2_Handler,default_interrupt
	.weak	I2C3_Handler
	.thumb_set I2C3_Handler,default_interrupt
	.weak	Timer4SubtimerA_Handler
	.thumb_set Timer4SubtimerA_Handler,default_interrupt
	.weak	Timer4SubtimerB_Handler
	.thumb_set Timer4SubtimerB_Handler,default_interrupt
	.weak	Timer5SubtimerA_Handler
	.thumb_set Timer5SubtimerA_Handler,default_interrupt
	.weak	Timer5SubtimerB_Handler
	.thumb_set Timer5SubtimerB_Handler,default_interrupt
	.weak	WideTimer0SubtimerA_Handler
	.thumb_set WideTimer0SubtimerA_Handler,default_interrupt
	.weak	WideTimer0SubtimerB_Handler
	.thumb_set WideTimer0SubtimerB_Handler,default_interrupt
	.weak	WideTimer1SubtimerA_Handler
	.thumb_set WideTimer1SubtimerA_Handler,default_interrupt
	.weak	WideTimer1SubtimerB_Handler
	.thumb_set WideTimer1SubtimerB_Handler,default_interrupt
	.weak	WideTimer2SubtimerA_Handler
	.thumb_set WideTimer2SubtimerA_Handler,default_interrupt
	.weak	WideTimer2SubtimerB_Handler
	.thumb_set WideTimer2SubtimerB_Handler,default_interrupt
	.weak	WideTimer3SubtimerA_Handler
	.thumb_set WideTimer3SubtimerA_Handler,default_interrupt
	.weak	WideTimer3SubtimerB_Handler
	.thumb_set WideTimer3SubtimerB_Handler,default_interrupt
	.weak	WideTimer4SubtimerA_Handler
	.thumb_set WideTimer4SubtimerA_Handler,default_interrupt
	.weak	WideTimer4SubtimerB_Handler
	.thumb_set WideTimer4SubtimerB_Handler,default_interrupt
	.weak	WideTimer5SubtimerA_Handler
	.thumb_set WideTimer5SubtimerA_Handler,default_interrupt
	.weak	WideTimer5SubtimerB_Handler
	.thumb_set WideTimer5SubtimerB_Handler,default_interrupt
	.weak	FPU_Handler
	.thumb_set FPU_Handler,default_interrupt
	.weak	I2C4_Handler
	.thumb_set I2C4_Handler,default_interrupt
	.weak	I2C5_Handler
	.thumb_set I2C5_Handler,default_interrupt
	.weak	GPIOPortM_Handler
	.thumb_set GPIOPortM_Handler,default_interrupt
	.weak	GPIOPortN_Handler
	.thumb_set GPIOPortN_Handler,default_interrupt
	.weak	QuadratureEncoder2_Handler
	.thumb_set QuadratureEncoder2_Handler,default_interrupt
	.weak	GPIOPortP_Handler
	.thumb_set GPIOPortP_Handler,default_interrupt
	.weak	GPIOPortP1_Handler
	.thumb_set GPIOPortP1_Handler,default_interrupt
	.weak	GPIOPortP2_Handler
	.thumb_set GPIOPortP2_Handler,default_interrupt
	.weak	GPIOPortP3_Handler
	.thumb_set GPIOPortP3_Handler,default_interrupt
	.weak	GPIOPortP4_Handler
	.thumb_set GPIOPortP4_Handler,default_interrupt
	.weak	GPIOPortP5_Handler
	.thumb_set GPIOPortP5_Handler,default_interrupt
	.weak	GPIOPortP6_Handler
	.thumb_set GPIOPortP6_Handler,default_interrupt
	.weak	GPIOPortP7_Handler
	.thumb_set GPIOPortP7_Handler,default_interrupt
	.weak	GPIOPortQ_Handler
	.thumb_set GPIOPortQ_Handler,default_interrupt
	.weak	GPIOPortQ1_Handler
	.thumb_set GPIOPortQ1_Handler,default_interrupt
	.weak	GPIOPortQ2_Handler
	.thumb_set GPIOPortQ2_Handler,default_interrupt
	.weak	GPIOPortQ3_Handler
	.thumb_set GPIOPortQ3_Handler,default_interrupt
	.weak	GPIOPortQ4_Handler
	.thumb_set GPIOPortQ4_Handler,default_interrupt
	.weak	GPIOPortQ5_Handler
	.thumb_set GPIOPortQ5_Handler,default_interrupt
	.weak	GPIOPortQ6_Handler
	.thumb_set GPIOPortQ6_Handler,default_interrupt
	.weak	GPIOPortQ7_Handler
	.thumb_set GPIOPortQ7_Handler,default_interrupt
	.weak	GPIOPortR_Handler
	.thumb_set GPIOPortR_Handler,default_interrupt
	.weak	GPIOPortS_Handler
	.thumb_set GPIOPortS_Handler,default_interrupt
	.weak	PWM1Generator0_Handler
	.thumb_set PWM1Generator0_Handler,default_interrupt
	.weak	PWM1Generator1_Handler
	.thumb_set PWM1Generator1_Handler,default_interrupt
	.weak	PWM1Generator2_Handler
	.thumb_set PWM1Generator2_Handler,default_interrupt
	.weak	PWM1Generator3_Handler
	.thumb_set PWM1Generator3_Handler,default_interrupt
	.weak	PWM1Fault_Handler
	.thumb_set PWM1Fault_Handler,default_interrupt
