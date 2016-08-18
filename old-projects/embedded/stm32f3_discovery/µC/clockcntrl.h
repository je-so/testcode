/* title: Reset-And-Clock-Control

   Gibt Zugriff auf

      o Anschalten und Abschalten der Hardwareclock für Peripherieeinheiten
      o Auswahl der Quelle der Hardwareclock (intern, extern, Peripherie-Bus, ..)

   Das STM32F3 kennt zwei High-Speed Taktgeber(Clock), einen externen (HSE) und
   einen internen (HSI). Beide werden mit 8MHZ getaktet. Um noch höher zu takten,
   wird ein PLL(Phase-Locked-Loop) benutzt, der diesem Board bei Wahl von HSE als
   Taktquelle maximal 72MHZ erzeugen kann.

   Der erzeugte Takt ist die SYSCLK(System-Clock), von dort wird der Takt über einen
   Prescaler (halbiert, viertelt, usw. den Takt) an den AHB (Advanced High Speed Bus)
   weitergegeben (HCLK). Dessen Takt wird wiederum an die Busse APB1 (Advanced Peripheral Bus)
   und APB2 weitergereicht (PCLK1 und PCLK2), versehen mit entsprechenden Prescalern.

   > FLAGS:  PLLSRC    SW
   >          ---      ---
   >      HSI ─┤\   HSI─┤\
   >           │ ├─ PLL─┤ ├─SYSCLK
   >      HSE ─┤/   HSE─┤/

   > SYSCLK -> [HPRE PRESCALER] -> HCLK-├─ AHB-Bus -> CPU(Cortex-M4)
   >                                    ├─ [PPRE1 PRESCALER] -> PCLK1 -> APB1-Bus + Periphpherie
   >                                    ├─ [PPRE2 PRESCALER] -> PCLK2 -> APB2-Bus + Periphpherie

   Die maximale Frequenz des AHB und des APB2 betragen 72 MHZ.
   Die maximale Frequenz für den APB1 betragen 36 MHZ.

   Flash Latenz:
   Der lesende Zugriff der CPU auf den Programmcode enthaltenden Flashspeicher
   kann bei entsprechend hoher Taktung von HCLK muss mittels ein oder zwei Wartezyklen
   gebremst werden.

   ┌────────────────────┬──────────────┐
   │ 0  < HCLK ≤ 24 MHz │ 0 Wartetakte │
   ├────────────────────┼──────────────┤
   │ 24 < HCLK ≤ 48 MHz │ 1 Wartetakt  │
   ├────────────────────┼──────────────┤
   │ 48 < HCLK ≤ 72 MHz │ 2 Wartetakte │
   └────────────────────┴──────────────┘

   Eingang Externe Clock:
   An den Pins PF0 (OSC_IN), PF1 (OSC_OUT) ist ein externer Quarz anzuschliessen.
   Auf dem Board befindet sich aber keiner. Anstatt dessen liegt
   am Pin PF0 (OSC_IN) der Ausgang MCO (8 MHZ Clock Out) des zusätzlich auf dem Board
   befindlichen µControllers STM32F103C8T6. Somit ist PF0 belegt aber Pin PF1,
   also der Ausgang OSC_OUT, der nur für einen echten Quarzoszillator benötigt wird,
   ist frei verfügbar.

   Precondition:
    - Include "µC/hwmap.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/hwmap.h
    Header file <Hardware-Memory-Map>.

   file: µC/clockcntrl.h
    Header file <Reset-And-Clock-Control>.

   file: TODO: clockcntrl.c
    Implementation file <Reset-And-Clock-Control impl>.
*/
#ifndef STM32F303xC_MC_CLOCKCNTRL_HEADER
#define STM32F303xC_MC_CLOCKCNTRL_HEADER

// == exported Peripherals/HW-Units
#define RCC       ((clockcntrl_t*)HW_REGISTER_BASEADDR_RCC)

// == exported types
struct clockcntrl_t;

typedef enum clock_e {
   clock_INTERNAL,   // HSI
   clock_EXTERNAL,   // HSE
   clock_PLL,        // PLL (with HSE as source)
} clock_e;

// == exported functions

static inline void enable_syscfg_clockcntrl(void);
static inline void disable_syscfg_clockcntrl(void);
static inline void enable_adc12_clockcntrl(void);
static inline void enable_adc34_clockcntrl(void);
static inline void disable_adc12_clockcntrl(void);
static inline void disable_adc34_clockcntrl(void);
static inline int  enable_gpio_clockcntrl(uint8_t port_bits);
static inline int  disable_gpio_clockcntrl(uint8_t port_bits);
static inline void enable_dma_clockcntrl(uint8_t dma_bits);
static inline void disable_dma_clockcntrl(uint8_t dma_bits);
static inline void enable_dac_clockcntrl(void/*only DAC1 supported*/);
static inline void disable_dac_clockcntrl(void/*only DAC1 supported*/);
static inline void enable_uart_clockcntrl(uint8_t uart_bits);
static inline void disable_uart_clockcntrl(uint8_t uart_bits);
static inline void enable_basictimer_clockcntrl(uint8_t timer_bits);
static inline void disable_basictimer_clockcntrl(uint8_t timer_bits);
static inline clock_e getsysclock_clockcntrl(void);
static inline uint32_t getHZ_clockcntrl(void);
static inline void enable_clock_clockcntrl(clock_e clk);
static inline int  disable_clock_clockcntrl(clock_e clk);
static inline void setprescaler_clockcntrl(uint8_t apb1_scale/*0(off),2,4,8,16*/, uint8_t apb2_scale/*0(off),2,4,8,16*/, uint8_t ahb_scale/*2,4,...,512 (no 32)*/);
static inline void setsysclock_clockcntrl(clock_e clk);

// == definitions
typedef volatile struct clockcntrl_t {
   /* Clock control register; Offset: 0x00; Reset value: 0x0000XX83 (X is undefined). */
   uint32_t    cr;
   /* Clock configuration register; Offset: 0x04; Reset value: 0x00000000 */
   uint32_t    cfgr;
} clockcntrl_t;

/* == RCC Reset and Clock Control == */

// == Register Offsets

/* APB1 peripheral reset register; Offset: 0x10; Reset value: 0x00000000 */
#define HW_REGISTER_OFFSET_RCC_APB1RSTR 0x10
/* AHB peripheral clock enable register; Offset: 0x14; */
#define HW_REGISTER_OFFSET_RCC_AHBENR  0x14
/* APB2 peripheral clock enable register; Offset: 0x18; Reset value: 0x00000000 */
#define HW_REGISTER_OFFSET_RCC_APB2ENR 0x18
/* APB1 peripheral clock enable register; Offset: 0x1c; Reset value: 0x00000000 */
#define HW_REGISTER_OFFSET_RCC_APB1ENR 0x1c
/* Clock configuration register 2; Offset: 0x2C; Reset value: 0x00000000 */
#define HW_REGISTER_OFFSET_RCC_CFGR2   0x2c
/* Clock configuration register 3; Offset: 0x30; Reset value: 0x00000000 */
#define HW_REGISTER_OFFSET_RCC_CFGR3   0x30

// == Bit Definitionen

#define HW_REGISTER_BIT_RCC_CR_PLLRDY     (1u << 25)  // PLL clock ready flag (set by hardware (hw)) 1: PLL locked 0: PLL unlocked
#define HW_REGISTER_BIT_RCC_CR_PLLON      (1u << 24)  // PLL enable (set by software (sw)) 1: PLL ON 0: PLL OFF
#define HW_REGISTER_BIT_RCC_CR_CSSON      (1u << 19)  // Clock security system enable (sw) 1: Clock detector ON
#define HW_REGISTER_BIT_RCC_CR_HSEBYP     (1u << 18)  // HSE crystal oscillator bypass (sw) 1: HSE crystal oscillator bypassed with external clock
#define HW_REGISTER_BIT_RCC_CR_HSERDY     (1u << 17)  // HSE clock ready flag (hw) 1: HSE oscillator ready
#define HW_REGISTER_BIT_RCC_CR_HSEON      (1u << 16)  // HSE clock enable (sw) 1: HSE oscillator ON 0: HSE oscillator OFF
#define HW_REGISTER_BIT_RCC_CR_HSITRIM_POS   3u       // [4:0] HSI clock trimming
#define HW_REGISTER_BIT_RCC_CR_HSITRIM_BITS  0x1f     // user-programmable trimming (sw); +/- 40 kHz per increment/decrement;
#define HW_REGISTER_BIT_RCC_CR_HSITRIM_MASK  HW_REGISTER_MASK(RCC, CR, HSITRIM)
#define HW_REGISTER_BIT_RCC_CR_HSIRDY     (1u << 1)   // HSI clock ready flag (hw) 1: HSI oscillator ready
#define HW_REGISTER_BIT_RCC_CR_HSION      (1u << 0)   // HSI clock enable (sw) 1: HSI oscillator ON

#define HW_REGISTER_BIT_RCC_CFGR_MCOF     (1u << 28)  // Microcontroller Clock Output Flag (hw) 1: switch to the new MCO source is effective 0: MCO field is written with new value without beeing effective
#define HW_REGISTER_BIT_RCC_CFGR_MCO_POS  24u         // [2:0] Microcontroller clock output (sw)
#define HW_REGISTER_BIT_RCC_CFGR_MCO_BITS 0x7         // 000: MCO output disable, 010: LSI, 100: System clock (SYSCLK),
#define HW_REGISTER_BIT_RCC_CFGR_MCO_MASK HW_REGISTER_MASK(RCC, CFGR, MCO) // 101: HSI, 110: HSE, 111: PLL clock
#define HW_REGISTER_BIT_RCC_CFGR_I2SSRC   (1u << 23)  // I2S external clock source selection (sw) 0: I2S2 and I2S3 clocked by system clock, 1: I2S2 and I2S3 clocked by the external clock
#define HW_REGISTER_BIT_RCC_CFGR_USBPRE   (1u << 22)  // USB prescaler to generate 48 MHz USB clock (sw) 0: PLL clock is divided by 1.5, 1: PLL clock is not divided
#define HW_REGISTER_BIT_RCC_CFGR_PLLMUL_POS  18u      // [3:0] PLL multiplication factor (sw) 0000: PLL input clock x 2,
#define HW_REGISTER_BIT_RCC_CFGR_PLLMUL_BITS 0xf      // 0001: PLL input clock x 3 ... 1101: PLL input clock x 15, 1110: PLL input clock x 16
#define HW_REGISTER_BIT_RCC_CFGR_PLLMUL_MASK HW_REGISTER_MASK(RCC, CFGR, PLLMUL) // PLL output frequency must not exceed 72 MHz !!
#define HW_REGISTER_BIT_RCC_CFGR_PLLSRC   (1u << 16)  // PLL entry clock source (sw) 0: HSI/2 selected as PLL input clock, 1: HSE/PREDIV selected as PLL input clock
#define HW_REGISTER_BIT_RCC_CFGR_PPRE2_POS   11u      // [2:0] APB high-speed prescaler (APB2) (sw) 0xx: HCLK not divided,
#define HW_REGISTER_BIT_RCC_CFGR_PPRE2_BITS  0x7      // 100: HCLK divided by 2, 101: HCLK divided by 4, 110: HCLK divided by 8, 111: HCLK divided by 16
#define HW_REGISTER_BIT_RCC_CFGR_PPRE2_MASK  HW_REGISTER_MASK(RCC, CFGR, PPRE2)
#define HW_REGISTER_BIT_RCC_CFGR_PPRE1_POS   8u       // [2:0] APB Low-speed prescaler (APB1) (sw) 0xx: HCLK not divided,
#define HW_REGISTER_BIT_RCC_CFGR_PPRE1_BITS  0x7      // 100: HCLK divided by 2, 101: HCLK divided by 4, 110: HCLK divided by 8, 111: HCLK divided by 16
#define HW_REGISTER_BIT_RCC_CFGR_PPRE1_MASK  HW_REGISTER_MASK(RCC, CFGR, PPRE1)
#define HW_REGISTER_BIT_RCC_CFGR_HPRE_POS    4u       // [3:0] HCLK prescaler (AHB) (sw) 0xxx: SYSCLK not divided
#define HW_REGISTER_BIT_RCC_CFGR_HPRE_BITS   0xf      // 1000: SYSCLK divided by 2, 1001: SYSCLK divided by 4, ... (32 is missing), 1111: SYSCLK divided by 512
#define HW_REGISTER_BIT_RCC_CFGR_HPRE_MASK   HW_REGISTER_MASK(RCC, CFGR, HPRE) // The prefetch buffer (ICODE) must be kept on when using a prescaler different from 1
#define HW_REGISTER_BIT_RCC_CFGR_SWS_POS     2u       // [1:0] System clock switch status (hw) 00: HSI oscillator used as system clock,
#define HW_REGISTER_BIT_RCC_CFGR_SWS_BITS    0x3      // 01: HSE oscillator used as system clock, 10: PLL used as system clock
#define HW_REGISTER_BIT_RCC_CFGR_SWS_MASK    HW_REGISTER_MASK(RCC, CFGR, SWS)
#define HW_REGISTER_BIT_RCC_CFGR_SW_POS      0u       // [1:0] System clock switch (sw) 00: HSI selected as system clock,
#define HW_REGISTER_BIT_RCC_CFGR_SW_BITS     0x3      // 01: HSE selected as system clock, 10: PLL selected as system clock
#define HW_REGISTER_BIT_RCC_CFGR_SW_MASK     HW_REGISTER_MASK(RCC, CFGR, SW)  // Cleared by hardware to force HSI selection when leaving Stop and Standby mode
                                                                              // or in case of failure of the HSE if CSS is on
/* RCC_AHBENR_ADC34EN: ADC3 and ADC4 enable. Set and reset by software. 0: disabled; 1: ADC3 and ADC4 clock enabled */
#define HW_REGISTER_BIT_RCC_AHBENR_ADC34EN (1u << 29)
/* RCC_AHBENR_ADC12EN: ADC1 and ADC2 enable. Set and reset by software. 0: disabled; 1: ADC1 and ADC2 clock enabled */
#define HW_REGISTER_BIT_RCC_AHBENR_ADC12EN (1u << 28)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPAEN (1u << 17)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPBEN (1u << 18)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPCEN (1u << 19)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPDEN (1u << 20)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPEEN (1u << 21)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPFEN (1u << 22)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPGEN (1u << 23)
#define HW_REGISTER_BIT_RCC_AHBENR_IOPHEN (1u << 16)
#define HW_REGISTER_BIT_RCC_AHBENR_DMA2EN (1u << 1)
#define HW_REGISTER_BIT_RCC_AHBENR_DMA1EN (1u << 0)

/* 0: SYSCFG clock disabled, 1: SYSCFG clock enabled */
#define HW_REGISTER_BIT_RCC_APB2ENR_SYSCFGEN (1u << 0)

// APB1 peripheral clock enable register
/* 0: DAC1 clock disabled, 1: DAC1 clock enabled */
#define HW_REGISTER_BIT_RCC_APB1ENR_DAC1EN   (1u << 29)
/* 0: UART4 clock disabled, 1: UART4 clock enabled */
#define HW_REGISTER_BIT_RCC_APB1ENR_UART4EN  (1u << 19)
/* 0: UART5 clock disabled, 1: UART5 clock enabled */
#define HW_REGISTER_BIT_RCC_APB1ENR_UART5EN  (1u << 20)
/* TIM7 timer clock enable. (sw) 0: TIM7 clock disabled, 1: TIM7 clock enabled */
#define HW_REGISTER_BIT_RCC_APB1ENR_TIM7EN   (1u << 5)
/* TIM6 timer clock enable. (sw) 0: TIM6 clock disabled, 1: TIM6 clock enabled */
#define HW_REGISTER_BIT_RCC_APB1ENR_TIM6EN   (1u << 4)

/* [3:0]: PREDIV division factor
 * 0000: HSE input to PLL not divided, 0001: HSE input to PLL divided by 2
 * 0010: HSE input to PLL divided by 3, ..., 1111: HSE input to PLL divided by 16 */
#define HW_REGISTER_BIT_RCC_CFGR2_PREDIV_POS    0u
#define HW_REGISTER_BIT_RCC_CFGR2_PREDIV_BITS   0xf
#define HW_REGISTER_BIT_RCC_CFGR2_PREDIV_MASK   HW_REGISTER_MASK(RCC, CFGR2, PREDIV)
/* UART5SW[1:0]: UART5 clock source selection
 * This bit is set and cleared by software to select the UART4 clock source.
 * 00: PCLK selected as UART5 clock source (default)
 * 01: System clock (SYSCLK) selected as UART5 clock
 * 10: LSE clock selected as UART5 clock
 * 11: HSI clock selected as UART5 clock */
#define HW_REGISTER_BIT_RCC_CFGR3_UART5SW (3u << 20)
/* UART4SW[1:0]: UART4 clock source selection
 * This bit is set and cleared by software to select the UART4 clock source.
 * 00: PCLK selected as UART4 clock source (default)
 * 01: System clock (SYSCLK) selected as UART4 clock
 * 10: LSE clock selected as UART4 clock
 * 11: HSI clock selected as UART4 clock */
#define HW_REGISTER_BIT_RCC_CFGR3_UART4SW (3u << 20)


/* == Flash Register == */

// TODO: move flash register into own module

// == Register Offsets

/* Flash access control register; Offset: 0x00; Reset value: 0x00000030 */
#define HW_REGISTER_OFFSET_FLASH_ACR   0x00

// == Bit Definitionen

#define HW_REGISTER_BIT_FLASH_ACR_LATENCY_POS   0u
#define HW_REGISTER_BIT_FLASH_ACR_LATENCY_BITS  0x7
#define HW_REGISTER_BIT_FLASH_ACR_LATENCY_MASK  HW_REGISTER_MASK(FLASH, ACR, LATENCY)


// section: inline implementation

static inline void enable_syscfg_clockcntrl(void)
{
   HW_REGISTER(RCC, APB2ENR) |= HW_REGISTER_BIT_RCC_APB2ENR_SYSCFGEN;
}

static inline void disable_syscfg_clockcntrl(void)
{
   HW_REGISTER(RCC, APB2ENR) &= ~ HW_REGISTER_BIT_RCC_APB2ENR_SYSCFGEN;
}

static inline void enable_adc12_clockcntrl(void)
{
   HW_REGISTER(RCC, AHBENR) |= HW_REGISTER_BIT_RCC_AHBENR_ADC12EN;
}

static inline void enable_adc34_clockcntrl(void)
{
   HW_REGISTER(RCC, AHBENR) |= HW_REGISTER_BIT_RCC_AHBENR_ADC34EN;
}

static inline void disable_adc12_clockcntrl(void)
{
   HW_REGISTER(RCC, AHBENR) &= ~HW_REGISTER_BIT_RCC_AHBENR_ADC12EN;
}

static inline void disable_adc34_clockcntrl(void)
{
   HW_REGISTER(RCC, AHBENR) &= ~HW_REGISTER_BIT_RCC_AHBENR_ADC34EN;
}

static inline int enable_gpio_clockcntrl(uint8_t port_bits)
{
   uint32_t bits;

   if (port_bits >= (1u << 8)) return EINVAL;
   static_assert( HW_REGISTER_BIT_RCC_AHBENR_IOPAEN == (1u << 17)
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPBEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPAEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPCEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPBEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPDEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPCEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPEEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPDEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPFEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPEEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPGEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPFEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPHEN <  HW_REGISTER_BIT_RCC_AHBENR_IOPAEN);
   if (port_bits & (1u << 7)) {
      bits = HW_REGISTER_BIT_RCC_AHBENR_IOPHEN + ((port_bits & ~(1u << 7)) << 17);
   } else {
      bits = port_bits << 17;
   }

   HW_REGISTER(RCC, AHBENR) |= bits;
   return 0;
}

static inline int disable_gpio_clockcntrl(uint8_t port_bits)
{
   uint32_t bits;

   if (port_bits >= (1u << 8)) return EINVAL;
   static_assert( HW_REGISTER_BIT_RCC_AHBENR_IOPAEN == (1u << 17)
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPBEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPAEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPCEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPBEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPDEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPCEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPEEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPDEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPFEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPEEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPGEN == 2 * HW_REGISTER_BIT_RCC_AHBENR_IOPFEN
                  && HW_REGISTER_BIT_RCC_AHBENR_IOPHEN <  HW_REGISTER_BIT_RCC_AHBENR_IOPAEN);
   if (port_bits & (1u << 7)) {
      bits = HW_REGISTER_BIT_RCC_AHBENR_IOPHEN + ((port_bits & ~(1u << 7)) << 17);
   } else {
      bits = port_bits << 17;
   }

   HW_REGISTER(RCC, AHBENR) &= ~bits;
   return 0;
}

static inline void enable_dma_clockcntrl(uint8_t dma_bits)
{
   HW_REGISTER(RCC, AHBENR) |= dma_bits;
}

static inline void disable_dma_clockcntrl(uint8_t dma_bits)
{
   HW_REGISTER(RCC, AHBENR) &= ~dma_bits;
}

static inline void enable_dac_clockcntrl(void/*nur DAC1 unterstützt*/)
{
   HW_REGISTER(RCC, APB1ENR) |= HW_REGISTER_BIT_RCC_APB1ENR_DAC1EN;
}

static inline void disable_dac_clockcntrl(void/*nur DAC1 unterstützt*/)
{
   HW_REGISTER(RCC, APB1ENR) &= ~HW_REGISTER_BIT_RCC_APB1ENR_DAC1EN;
}

static inline void enable_uart_clockcntrl(uint8_t uart_bits)
{
   static_assert(HW_REGISTER_BIT_RCC_APB1ENR_UART4EN == (1u << 19));
   // UART4_BIT == (1u << 3) ==> shift of (19-3)
   uint32_t bits = (uint32_t) uart_bits << (19-3);
   HW_REGISTER(RCC, APB1ENR) |= bits;
   // TODO: implement clock source selection
   bits = (uart_bits & (1u << 4) ? HW_REGISTER_BIT_RCC_CFGR3_UART5SW : 0)
        | (uart_bits & (1u << 3) ? HW_REGISTER_BIT_RCC_CFGR3_UART4SW : 0);
   // select HSI clock working at HW_KONFIG_CLOCK_INTERNAL_HZ
   HW_REGISTER(RCC, CFGR3) |= bits;
}

static inline void disable_uart_clockcntrl(uint8_t uart_bits)
{
   static_assert(HW_REGISTER_BIT_RCC_APB1ENR_UART4EN == (1u << 19));
   // UART4_BIT == (1u << 3) ==> shift of (19-3)
   uint32_t bits = (uint32_t) uart_bits << (19-3);
   HW_REGISTER(RCC, APB1ENR) &= ~bits;
}

static inline void enable_basictimer_clockcntrl(uint8_t timer_bits)
{
   HW_REGISTER(RCC, APB1ENR) |= timer_bits;
}

static inline void disable_basictimer_clockcntrl(uint8_t timer_bits)
{
   HW_REGISTER(RCC, APB1ENR) &= ~timer_bits;
}


static inline clock_e getsysclock_clockcntrl(void)
{
   return (RCC->cfgr & HW_REGISTER_BIT_RCC_CFGR_SWS_MASK) >> HW_REGISTER_BIT_RCC_CFGR_SWS_POS;
}

static inline uint32_t getHZ_clockcntrl(void)
{
   uint32_t hz = HW_KONFIG_CLOCK_INTERNAL_HZ;
   switch (getsysclock_clockcntrl()) {
   case clock_INTERNAL:
      hz = HW_KONFIG_CLOCK_INTERNAL_HZ;
      break;
   case clock_EXTERNAL:
      hz = HW_KONFIG_CLOCK_EXTERNAL_HZ;
      break;
   case clock_PLL:
      if ((RCC->cfgr & HW_REGISTER_BIT_RCC_CFGR_PLLSRC) != 0) {
         // 1: HSE/PREDIV selected as PLL input clock
         hz = HW_KONFIG_CLOCK_EXTERNAL_HZ;
         // HSE input to PLL not divided
         uint32_t prediv = (HW_SREGISTER(RCC, CFGR2) & HW_REGISTER_BIT_RCC_CFGR2_PREDIV_MASK) >> HW_REGISTER_BIT_RCC_CFGR2_PREDIV_POS;
         hz /= (prediv + 1);
      } else {
         // 0: HSI/2 selected as PLL input clock
         hz = HW_KONFIG_CLOCK_INTERNAL_HZ / 2;
      }
      uint32_t pllmul = 2 + ((RCC->cfgr & HW_REGISTER_BIT_RCC_CFGR_PLLMUL_MASK) >> HW_REGISTER_BIT_RCC_CFGR_PLLMUL_POS);
      hz *= (pllmul <= 16 ? pllmul : 16);
      break;
   }
   return hz;
}

static inline void enable_clock_clockcntrl(clock_e clk)
{
   uint32_t cr = RCC->cr;
   switch (clk) {
   case clock_INTERNAL:
      if ((cr & HW_REGISTER_BIT_RCC_CR_HSIRDY) == 0) {
         RCC->cr = cr | (HW_REGISTER_BIT_RCC_CR_HSION);
         while ((RCC->cr & HW_REGISTER_BIT_RCC_CR_HSIRDY) == 0) ;
      }
      break;
   case clock_EXTERNAL:
      if ((cr & HW_REGISTER_BIT_RCC_CR_HSERDY) == 0) {
         #if (HW_KONFIG_CLOCK_EXTERNAL_ISCRYSTAL != 0)
         cr &= ~HW_REGISTER_BIT_RCC_CR_HSEBYP;
         #else
         cr |= HW_REGISTER_BIT_RCC_CR_HSEBYP;
         #endif
         RCC->cr = cr | (HW_REGISTER_BIT_RCC_CR_HSEON|HW_REGISTER_BIT_RCC_CR_CSSON);
         while ((RCC->cr & HW_REGISTER_BIT_RCC_CR_HSERDY) == 0) ;
      }
      break;
   case clock_PLL:
      if ((cr & HW_REGISTER_BIT_RCC_CR_PLLRDY) == 0) {
         RCC->cr = cr | (HW_REGISTER_BIT_RCC_CR_PLLON);
         while ((RCC->cr & HW_REGISTER_BIT_RCC_CR_PLLRDY) == 0) ;
      }
      break;
   }
}

static inline int disable_clock_clockcntrl(clock_e clk)
{
   clock_e sysclock = getsysclock_clockcntrl();
   if (sysclock == clk) {
      return EBUSY;
   }

   uint32_t cr = RCC->cr;
   switch (clk) {
   case clock_INTERNAL:
      if (sysclock == clock_PLL && (RCC->cfgr & HW_REGISTER_BIT_RCC_CFGR_PLLSRC) == 0) {
         return EBUSY;
      }
      if ((cr & HW_REGISTER_BIT_RCC_CR_HSIRDY) != 0) {
         RCC->cr = cr & ~(HW_REGISTER_BIT_RCC_CR_HSION);
         while ((RCC->cr & HW_REGISTER_BIT_RCC_CR_HSIRDY) != 0) ;
      }
      break;
   case clock_EXTERNAL:
      if (sysclock == clock_PLL && (RCC->cfgr & HW_REGISTER_BIT_RCC_CFGR_PLLSRC) != 0) {
         return EBUSY;
      }
      if ((cr & HW_REGISTER_BIT_RCC_CR_HSERDY) != 0) {
         RCC->cr = cr & ~(HW_REGISTER_BIT_RCC_CR_HSEBYP|HW_REGISTER_BIT_RCC_CR_HSEON|HW_REGISTER_BIT_RCC_CR_CSSON);
         while ((RCC->cr & HW_REGISTER_BIT_RCC_CR_HSERDY) != 0) ;
      }
      break;
   case clock_PLL:
      if ((cr & HW_REGISTER_BIT_RCC_CR_PLLRDY) != 0) {
         RCC->cr = cr & ~(HW_REGISTER_BIT_RCC_CR_PLLON);
         while ((RCC->cr & HW_REGISTER_BIT_RCC_CR_PLLRDY) != 0) ;
      }
      break;
   default:
      return ENOSYS;
   }

   return 0;
}

static inline void setprescaler_clockcntrl(uint8_t apb1_scale/*0(off),2,4,8,16*/, uint8_t apb2_scale/*0(off),2,4,8,16*/, uint8_t ahb_scale/*2,4,...,512 (no 32)*/)
{
   uint32_t cfgr, scale, bits;
   // all prescalers off
   cfgr = RCC->cfgr & ~(HW_REGISTER_BIT_RCC_CFGR_PPRE1_MASK|HW_REGISTER_BIT_RCC_CFGR_PPRE2_MASK|HW_REGISTER_BIT_RCC_CFGR_HPRE_MASK);
   if ((scale = apb1_scale>>1) > 0) {
      static_assert(HW_REGISTER_BIT_RCC_CFGR_PPRE1_BITS == 7);
      for (bits = 4; (scale >>= 1); ) {
         ++ bits;
      }
      cfgr |= (bits <= 7 ? bits : 7) << HW_REGISTER_BIT_RCC_CFGR_PPRE1_POS;
   }
   if ((scale = apb2_scale>>1) > 0) {
      static_assert(HW_REGISTER_BIT_RCC_CFGR_PPRE2_BITS == 7);
      for (bits = 4; (scale >>= 1); ) {
         ++ bits;
      }
      cfgr |= (bits <= 7 ? bits : 7) << HW_REGISTER_BIT_RCC_CFGR_PPRE2_POS;
   }
   if ((scale = ahb_scale>>1) > 0) {
      static_assert(HW_REGISTER_BIT_RCC_CFGR_HPRE_BITS == 15);
      if (scale >= 16) scale >>= 1; // skip 32, 32 is mapped to 16
      for (bits = 8; (scale >>= 1); ) {
         ++ bits;
      }
      cfgr |= (bits <= 15 ? bits : 15) << HW_REGISTER_BIT_RCC_CFGR_HPRE_POS;
   }
   RCC->cfgr = cfgr;
}

static inline void setsysclock_clockcntrl(clock_e clk)
{
   if (getsysclock_clockcntrl() != clk) {
      if (clk == clock_PLL) {
         enable_clock_clockcntrl(clock_EXTERNAL);
         disable_clock_clockcntrl(clock_PLL);   // disable to allow change to PLL config
         HW_SREGISTER(RCC, CFGR2) &= ~HW_REGISTER_BIT_RCC_CFGR2_PREDIV_MASK; // HSE input to PLL not divided
         const uint32_t MAX_HZ = 72000000;
         static_assert((MAX_HZ / HW_KONFIG_CLOCK_EXTERNAL_HZ) >= 2);
         static_assert((MAX_HZ / HW_KONFIG_CLOCK_EXTERNAL_HZ) <= 16);
         RCC->cfgr = (RCC->cfgr & ~HW_REGISTER_BIT_RCC_CFGR_PLLMUL_MASK)
                     | HW_REGISTER_BIT_RCC_CFGR_PLLSRC   // 1: HSE/PREDIV selected as PLL input clock
                     | ((MAX_HZ/HW_KONFIG_CLOCK_EXTERNAL_HZ - 2) << HW_REGISTER_BIT_RCC_CFGR_PLLMUL_POS);
         // PCLK1 36MHZ, PCLK2 72MHZ, HCLK 72MHZ
         setprescaler_clockcntrl(2,0,0);
         // set 2 wait cycles for FLASH Memory
         HW_REGISTER(FLASH, ACR) = (HW_REGISTER(FLASH, ACR) & ~ HW_REGISTER_BIT_FLASH_ACR_LATENCY_MASK)
                                   | (2 << HW_REGISTER_BIT_FLASH_ACR_LATENCY_POS);
      }
      // switch clock
      enable_clock_clockcntrl(clk);
      RCC->cfgr = (RCC->cfgr & ~HW_REGISTER_BIT_RCC_CFGR_SW_MASK) | (clk << HW_REGISTER_BIT_RCC_CFGR_SW_POS);
      // wait until SYSCLK switched to new source
      while (getsysclock_clockcntrl() != clk) ;
      if (clk != clock_PLL) {
         // PCLK1 8MHZ, PCLK2 8MHZ, HCLK 8MHZ
         setprescaler_clockcntrl(0,0,0);
         // set 0 wait cycles for FLASH Memory
         HW_REGISTER(FLASH, ACR) = (HW_REGISTER(FLASH, ACR) & ~ HW_REGISTER_BIT_FLASH_ACR_LATENCY_MASK);
      }
   }
}

#endif
