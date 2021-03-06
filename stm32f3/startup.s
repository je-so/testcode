/* ARM Assembler (Thumb-2) implementation of startup code for CortexM4 based boards.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: startup.h
    Header file <Startup-Code-CortexM4>.

   file: startup.c
    Implementation file <Startup-Code-CortexM4 impl>.

   file: startup.s
    Implementation alternative in Assembler instead of startup.c.
*/
	.syntax unified
	.cpu cortex-m4
	.fpu fpv4-sp-d16

	.global	getmainpsp_startup
	.global	g_stack_msp
	.global	g_NVIC_vectortable

	.text
	.align	2
	.thumb

	.thumb_func
reset_interrupt:
	bl	   init_datasegment_startup
.ifdef KONFIG_USE_PSP
   bl    getmainpsp_startup
   msr   psp, r0           /* psp = start_get_psp() */
   mrs   r0, control       /* r0 = control */
   orrs  r0, #(1<<1)       /* set bit 1 in r0 */
   msr   control, r0       /* switch cpu using psp as stack pointer */
.endif
	bl	   main
1:	b	   1b                /* endless loop in case main returns */

	.thumb_func
default_interrupt:
1:	b	   1b                /* endless loop to preserve state */

	.thumb_func
init_datasegment_startup:
	movw	r1, #:lower16:_romdata
	movt  r1, #:upper16:_romdata
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
	bcs	.clear_end /* r2 >= r3 ==> done */
.clear_bss:
	str	r0, [r2], #4
	cmp	r2, r3
	bcc	.clear_bss /* r2 < r3 ==> clear next */

.clear_end:
	bx	lr

.if (KONFIG_STACKSIZE&0x03) != 0
.error "KONFIG_STACKSIZE not aligned"
.endif
	.section	.sram_address_start,"aw",%nobits
g_stack_msp:
	.space	KONFIG_STACKSIZE

	.section	.rom_address_0x0,"aw",%progbits
g_NVIC_vectortable:
	.word	g_stack_msp + KONFIG_STACKSIZE   // msp stack pointer loaded at after reset
	.word	reset_interrupt                  // reset interrupted executed in thread mode after reset
	.word	nmi_interrupt
	.word	fault_interrupt
	.word	mpufault_interrupt
	.word	busfault_interrupt
	.word	usagefault_interrupt
	.space	16
	.word	svcall_interrupt
	.word	debugmonitor_interrupt
	.space	4
	.word	pendsv_interrupt
	.word	systick_interrupt
	.word	WWDG_IRQHandler  /* 16 */
	.word	pvd_interrupt
	.word	TAMP_STAMP_IRQHandler
	.word	RTC_WKUP_IRQHandler
	.word	FLASH_IRQHandler /* 20 */
	.word	RCC_IRQHandler
	.word	gpiopin0_interrupt
	.word	EXTI1_IRQHandler
	.word	gpiopin2_tsc_interrupt
	.word	EXTI3_IRQHandler /*25*/
	.word	EXTI4_IRQHandler
	.word	dma1_channel1_interrupt
	.word	dma1_channel2_interrupt
	.word	dma1_channel3_interrupt
	.word	dma1_channel4_interrupt /*30*/
	.word	dma1_channel5_interrupt
	.word	dma1_channel6_interrupt
	.word	dma1_channel7_interrupt
	.word	ADC1_2_IRQHandler
	.word	USB_HP_CAN_TX_IRQHandler /*35*/
	.word	USB_LP_CAN_RX0_IRQHandler
	.word	CAN_RX1_IRQHandler
	.word	CAN_SCE_IRQHandler
	.word	EXTI9_5_IRQHandler
	.word	TIM1_BRK_TIM15_IRQHandler /*40*/
	.word	TIM1_UP_TIM16_IRQHandler
	.word	TIM1_TRG_COM_TIM17_IRQHandler
	.word	TIM1_CC_IRQHandler
	.word	TIM2_IRQHandler
	.word	TIM3_IRQHandler /*45*/
	.word	TIM4_IRQHandler
	.word	I2C1_EV_IRQHandler
	.word	I2C1_ER_IRQHandler
	.word	I2C2_EV_IRQHandler
	.word	I2C2_ER_IRQHandler /*50 ---*/
	.word	SPI1_IRQHandler
	.word	SPI2_IRQHandler
	.word	USART1_IRQHandler
	.word	USART2_IRQHandler
	.word	USART3_IRQHandler /*55*/
	.word	EXTI15_10_IRQHandler
	.word	RTC_Alarm_IRQHandler
	.word	USBWakeUp_IRQHandler
	.word	TIM8_BRK_IRQHandler
	.word	TIM8_UP_IRQHandler /*60 ----*/
	.word	TIM8_TRG_COM_IRQHandler
	.word	TIM8_CC_IRQHandler
	.word	ADC3_IRQHandler
	.space	12 /*64..66*/
	.word	SPI3_IRQHandler
	.word	UART4_IRQHandler
	.word	UART5_IRQHandler
	.word	timer6_dac_interrupt /*70*/
	.word	timer7_interrupt
	.word	dma2_channel1_interrupt
	.word	dma2_channel2_interrupt
	.word	dma2_channel3_interrupt
	.word	dma2_channel4_interrupt /*75-----*/
	.word	dma2_channel5_interrupt
	.word	ADC4_IRQHandler
	.space	8
	.word	COMP1_2_3_IRQHandler /*80*/
	.word	COMP4_5_6_IRQHandler
	.word	COMP7_IRQHandler
	.space	28 /*83..89*/
	.word	USB_HP_IRQHandler /*90*/
	.word	USB_LP_IRQHandler
	.word	USBWakeUp_RMP_IRQHandler
	.space	16 /*93..96*/
	.word	FPU_IRQHandler /*97*/


/*****************************************
 * Make all interrupt handler weak
 * so other modules could overwrite the default implementation.
 * Every not implemented interrupt handler is mapped to default_interrupt
 * which is a simple endless loop.
 *****************************************/

	.weak	default_interrupt
	.weak	reset_interrupt
	.weak	nmi_interrupt
	.thumb_set nmi_interrupt,default_interrupt
	.weak	fault_interrupt
	.thumb_set fault_interrupt,default_interrupt
	.weak	mpufault_interrupt
	.thumb_set mpufault_interrupt,default_interrupt
	.weak	busfault_interrupt
	.thumb_set busfault_interrupt,default_interrupt
	.weak	usagefault_interrupt
	.thumb_set usagefault_interrupt,default_interrupt
	.weak	svcall_interrupt
	.thumb_set svcall_interrupt,default_interrupt
	.weak	debugmonitor_interrupt
	.thumb_set debugmonitor_interrupt,default_interrupt
	.weak	pendsv_interrupt
	.thumb_set pendsv_interrupt,default_interrupt
	.weak	systick_interrupt
	.thumb_set systick_interrupt,default_interrupt
	.weak	WWDG_IRQHandler
	.thumb_set WWDG_IRQHandler,default_interrupt
	.weak	pvd_interrupt
	.thumb_set pvd_interrupt,default_interrupt
	.weak	TAMP_STAMP_IRQHandler
	.thumb_set TAMP_STAMP_IRQHandler,default_interrupt
	.weak	RTC_WKUP_IRQHandler
	.thumb_set RTC_WKUP_IRQHandler,default_interrupt
	.weak	FLASH_IRQHandler
	.thumb_set FLASH_IRQHandler,default_interrupt
	.weak	RCC_IRQHandler
	.thumb_set RCC_IRQHandler,default_interrupt
	.weak	gpiopin0_interrupt
	.thumb_set gpiopin0_interrupt,default_interrupt
	.weak	EXTI1_IRQHandler
	.thumb_set EXTI1_IRQHandler,default_interrupt
	.weak	gpiopin2_tsc_interrupt
	.thumb_set gpiopin2_tsc_interrupt,default_interrupt
	.weak	EXTI3_IRQHandler
	.thumb_set EXTI3_IRQHandler,default_interrupt
	.weak	EXTI4_IRQHandler
	.thumb_set EXTI4_IRQHandler,default_interrupt
	.weak	dma1_channel1_interrupt
	.thumb_set dma1_channel1_interrupt,default_interrupt
	.weak	dma1_channel2_interrupt
	.thumb_set dma1_channel2_interrupt,default_interrupt
	.weak	dma1_channel3_interrupt
	.thumb_set dma1_channel3_interrupt,default_interrupt
	.weak	dma1_channel4_interrupt
	.thumb_set dma1_channel4_interrupt,default_interrupt
	.weak	dma1_channel5_interrupt
	.thumb_set dma1_channel5_interrupt,default_interrupt
	.weak	dma1_channel6_interrupt
	.thumb_set dma1_channel6_interrupt,default_interrupt
	.weak	dma1_channel7_interrupt
	.thumb_set dma1_channel7_interrupt,default_interrupt
	.weak	ADC1_2_IRQHandler
	.thumb_set ADC1_2_IRQHandler,default_interrupt
	.weak	USB_HP_CAN_TX_IRQHandler
	.thumb_set USB_HP_CAN_TX_IRQHandler,default_interrupt
	.weak	USB_LP_CAN_RX0_IRQHandler
	.thumb_set USB_LP_CAN_RX0_IRQHandler,default_interrupt
	.weak	CAN_RX1_IRQHandler
	.thumb_set CAN_RX1_IRQHandler,default_interrupt
	.weak	CAN_SCE_IRQHandler
	.thumb_set CAN_SCE_IRQHandler,default_interrupt
	.weak	EXTI9_5_IRQHandler
	.thumb_set EXTI9_5_IRQHandler,default_interrupt
	.weak	TIM1_BRK_TIM15_IRQHandler
	.thumb_set TIM1_BRK_TIM15_IRQHandler,default_interrupt
	.weak	TIM1_UP_TIM16_IRQHandler
	.thumb_set TIM1_UP_TIM16_IRQHandler,default_interrupt
	.weak	TIM1_TRG_COM_TIM17_IRQHandler
	.thumb_set TIM1_TRG_COM_TIM17_IRQHandler,default_interrupt
	.weak	TIM1_CC_IRQHandler
	.thumb_set TIM1_CC_IRQHandler,default_interrupt
	.weak	TIM2_IRQHandler
	.thumb_set TIM2_IRQHandler,default_interrupt
	.weak	TIM3_IRQHandler
	.thumb_set TIM3_IRQHandler,default_interrupt
	.weak	TIM4_IRQHandler
	.thumb_set TIM4_IRQHandler,default_interrupt
	.weak	I2C1_EV_IRQHandler
	.thumb_set I2C1_EV_IRQHandler,default_interrupt
	.weak	I2C1_ER_IRQHandler
	.thumb_set I2C1_ER_IRQHandler,default_interrupt
	.weak	I2C2_EV_IRQHandler
	.thumb_set I2C2_EV_IRQHandler,default_interrupt
	.weak	I2C2_ER_IRQHandler
	.thumb_set I2C2_ER_IRQHandler,default_interrupt
	.weak	SPI1_IRQHandler
	.thumb_set SPI1_IRQHandler,default_interrupt
	.weak	SPI2_IRQHandler
	.thumb_set SPI2_IRQHandler,default_interrupt
	.weak	USART1_IRQHandler
	.thumb_set USART1_IRQHandler,default_interrupt
	.weak	USART2_IRQHandler
	.thumb_set USART2_IRQHandler,default_interrupt
	.weak	USART3_IRQHandler
	.thumb_set USART3_IRQHandler,default_interrupt
	.weak	EXTI15_10_IRQHandler
	.thumb_set EXTI15_10_IRQHandler,default_interrupt
	.weak	RTC_Alarm_IRQHandler
	.thumb_set RTC_Alarm_IRQHandler,default_interrupt
	.weak	USBWakeUp_IRQHandler
	.thumb_set USBWakeUp_IRQHandler,default_interrupt
	.weak	TIM8_BRK_IRQHandler
	.thumb_set TIM8_BRK_IRQHandler,default_interrupt
	.weak	TIM8_UP_IRQHandler
	.thumb_set TIM8_UP_IRQHandler,default_interrupt
	.weak	TIM8_TRG_COM_IRQHandler
	.thumb_set TIM8_TRG_COM_IRQHandler,default_interrupt
	.weak	TIM8_CC_IRQHandler
	.thumb_set TIM8_CC_IRQHandler,default_interrupt
	.weak	ADC3_IRQHandler
	.thumb_set ADC3_IRQHandler,default_interrupt
	.weak	SPI3_IRQHandler
	.thumb_set SPI3_IRQHandler,default_interrupt
	.weak	UART4_IRQHandler
	.thumb_set UART4_IRQHandler,default_interrupt
	.weak	UART5_IRQHandler
	.thumb_set UART5_IRQHandler,default_interrupt
	.weak	timer6_dac_interrupt
	.thumb_set timer6_dac_interrupt,default_interrupt
	.weak	timer7_interrupt
	.thumb_set timer7_interrupt,default_interrupt
	.weak	dma2_channel1_interrupt
	.thumb_set dma2_channel1_interrupt,default_interrupt
	.weak	dma2_channel2_interrupt
	.thumb_set dma2_channel2_interrupt,default_interrupt
	.weak	dma2_channel3_interrupt
	.thumb_set dma2_channel3_interrupt,default_interrupt
	.weak	dma2_channel4_interrupt
	.thumb_set dma2_channel4_interrupt,default_interrupt
	.weak	dma2_channel5_interrupt
	.thumb_set dma2_channel5_interrupt,default_interrupt
	.weak	ADC4_IRQHandler
	.thumb_set ADC4_IRQHandler,default_interrupt
	.weak	COMP1_2_3_IRQHandler
	.thumb_set COMP1_2_3_IRQHandler,default_interrupt
	.weak	COMP4_5_6_IRQHandler
	.thumb_set COMP4_5_6_IRQHandler,default_interrupt
	.weak	COMP7_IRQHandler
	.thumb_set COMP7_IRQHandler,default_interrupt
	.weak	USB_HP_IRQHandler
	.thumb_set USB_HP_IRQHandler,default_interrupt
	.weak	USB_LP_IRQHandler
	.thumb_set USB_LP_IRQHandler,default_interrupt
	.weak	USBWakeUp_RMP_IRQHandler
	.thumb_set USBWakeUp_RMP_IRQHandler,default_interrupt
	.weak	FPU_IRQHandler
	.thumb_set FPU_IRQHandler,default_interrupt
