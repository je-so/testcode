/* title: Extended-Interrupts-Events-Controller

   Gibt Zugriff auf auf Cortex-M4 (STM32F303xC spezifisch) Prozessor Peripherie:

      o Verwaltet externe und interne asynchrone Events/Interrupts und leitet sie
        weiter zu CPU und NVIC sowie Wake-up-Events zum Power-Manager.
      o 28 externe + 8 interne Events werden verwaltet
      o Für externe Quellen kann die steigende oder fallende Flanke (kein Level) als Auslöser
        dienen, bei internen Quellen ist es immer die steigende Flank (rising edge).

   Wait-For-Event / asm("WFE"):
   Mit dem Assembler-Befehl "WFE" wartet der Prozessor auf ein Event.
   Die EXTI-HW-Unit kann so konfiguriert werden, dass ein externes/internes Event als Event-Signal
   an den Prozessor weitergeleitet wird und ihn aus "WFE" (Funktion waitevent_core impl. "WFE").

   Asynchrone Interne Interrupts:
   Einige HW-Units wie (UART, I2C) können Interrupts auch dann generierten, wenn das System
   im Schlafmodus ist. Diese wecken dann das Gesamtsystem wieder auf.


   Tabelle der Events und ihrer internen Verdrahtung:
   ┌─────────┬───────────────────────────────────────────────────────────────┐
   │EXTI Line│ Leitung ist verbunden mit:                                    │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI0   │ GPIO-Pin0 von einem Port A..F konfiguriert in SYSCFG_EXTICR1  │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │EXTI2..14│ GPIO-PinX von einem Port A..F konfiguriert in SYSCFG_EXTICRY  │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI15  │ GPIO-Pin15 von einem Port A..F konfiguriert in SYSCFG_EXTICR4 │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI16  │ Ausgabe von Programmable voltage detector (PVD):überwacht VDD │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI17  │ Alarm von Real-time clock (RTC)                               │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI18  │ USB Device FS wakeup event                                    │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI19  │ Real-time clock (RTC) tamper and Timestamps                   │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI20  │ Real-time clock (RTC) wakeup timer                            │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI21  │ Comparator 1 Ausgabe                                          │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI22  │ Comparator 2 Ausgabe                                          │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI23  │ (intern) I2C1 wakeup    (only enabled in STOP mode)           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI24  │ (intern) I2C2 wakeup    (only enabled in STOP mode)           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI25  │ (intern) USART1 wakeup  (only enabled in STOP mode)           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI26  │ (intern) USART2 wakeup  (only enabled in STOP mode)           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI27  │ (intern) I2C3 wakeup    (only enabled in STOP mode)           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI28  │ (intern) USART3 wakeup  (only enabled in STOP mode)           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI29  │ Comparator 3 output                                           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI30  │ Comparator 4 output                                           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI31  │ Comparator 5 output                                           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI32  │ Comparator 6 output                                           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI33  │ Comparator 7 output                                           │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI34  │ (intern) UART4 wakeup  (only enabled in STOP mode)            │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI35  │ (intern) UART5 wakeup  (only enabled in STOP mode)            │
   └─────────┴───────────────────────────────────────────────────────────────┘

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/exti.h
    Header file <Extended-Interrupts-Events-Controller>.

   file: TODO: µC/exit.c
    Implementation file <Extended-Interrupts-Events-Controller impl>.
*/
#ifndef STM32F303xC_MC_EXTI_HEADER
#define STM32F303xC_MC_EXTI_HEADER

// == exported Peripherals/HW-Units
#define EXTI   ((exti_t*)HW_REGISTER_BASEADDR_EXTI)

// == exported types
struct exti_t;

typedef enum exti_line_e {
   exti_LINE0,  exti_LINE1,  exti_LINE2,  exti_LINE3,  exti_LINE4,  exti_LINE5,  exti_LINE6,  exti_LINE7,  exti_LINE8,
   exti_LINE9,  exti_LINE10, exti_LINE11, exti_LINE12, exti_LINE13, exti_LINE14, exti_LINE15, exti_LINE16, exti_LINE17,
   exti_LINE18, exti_LINE19, exti_LINE20, exti_LINE21, exti_LINE22, exti_LINE23, exti_LINE24, exti_LINE25, exti_LINE26,
   exti_LINE27, exti_LINE28, exti_LINE29, exti_LINE30, exti_LINE31, exti_LINE32, exti_LINE33, exti_LINE34, exti_LINE35,
} exti_line_e;

// == exported functions
static inline void enable_interrupt_exti(exti_line_e linenr);
static inline void disable_interrupt_exti(exti_line_e linenr);
static inline void enable_event_exti(exti_line_e linenr);
static inline void disable_event_exti(exti_line_e linenr);
static inline void enable_interrupts_exti(uint32_t bits/*bits of lines 0..31*/);
static inline void disable_interrupts_exti(uint32_t bits/*bits of lines 0..31*/);
static inline void enable_events_exti(uint32_t bits/*bits of lines 0..31*/);
static inline void disable_events_exti(uint32_t bits/*bits of lines 0..31*/);
static inline void clear_interrupts_exti(uint32_t bits/*bits of lines 0..31*/);
static inline void generate_interrupts_exti(uint32_t bits/*bits of lines 0..31*/);
static inline void clear_interrupt_exti(exti_line_e linenr);
static inline void generate_interrupt_exti(exti_line_e linenr);
static inline void set_edge_exti(exti_line_e linenr, bool isRising, bool isFalling);

// == definitions
typedef volatile struct exti_t {
   /* Interrupt mask register (EXTI_IMR1); Address offset: 0x00; Reset value: 0x1F800000
    * The reset value for the internal lines (23, 24, 25, 26, 27 and 28) is set to ‘1’ in order to
    * enable the interrupt by default.
    * Bits 31:0 MRx: Interrupt Mask on external/internal line x, x = 0..31
    * 0: Interrupt request from Line x is masked
    * 1: Interrupt request from Line x is not masked */
   uint32_t    imr1;
   /* Event mask register (EXTI_EMR1); Address offset: 0x04; Reset value: 0x00000000
    * Bits 31:0 MRx: Event Mask on external/internal line x
    * 0: Event request from Line x is masked
    * 1: Event request from Line x is not masked */
   uint32_t    emr1;
   /* Rising trigger selection register (EXTI_RTSR1); Address offset: 0x08; Reset value: 0x00000000
    * Bits 31:29 TRx: Rising trigger event configuration bit of line x
    * Bits 28:23 Reserved, must be kept at reset value (internal lines).
    * Bits 22:0 TRx: Rising trigger event configuration bit of line x
    * 0: Rising trigger disabled (for Event and Interrupt) for input line x
    * 1: Rising trigger enabled (for Event and Interrupt) for input line x.
    * The external wakeup lines are edge-triggered. No glitches must be generated on these
    * lines. If a rising edge on an external interrupt line occurs during a write operation in the
    * EXTI_RTSR register, the pending bit is not set. */
   uint32_t    rtsr1;
   /* Falling trigger selection register (EXTI_FTSR1); Address offset: 0x0C; Reset value: 0x00000000
    * Bits 31:29 TRx: Falling trigger event configuration bit of line x
    * Bits 28:23 Reserved, must be kept at reset value (internal lines).
    * Bits 22:0 TRx: Falling trigger event configuration bit of line x
    * 0: Falling trigger disabled (for Event and Interrupt) for input line x
    * 1: Falling trigger enabled (for Event and Interrupt) for input line x.
    * The external wakeup lines are edge-triggered. No glitches must be generated on these
    * lines. If a falling edge on an external interrupt line occurs during a write operation to the
    * EXTI_FTSR register, the pending bit is not set. */
   uint32_t    ftsr1;
   /* Software interrupt event register (EXTI_SWIER1); Address offset: 0x10; Reset value: 0x00000000
    * Bits 31: 29 SWIERx: Software interrupt on line x
    * Bits 28:23 Reserved, must be kept at reset value (internal lines).
    * Bits 22:0 SWIERx: Software interrupt on line x
    * If the interrupt is enabled on this line in the EXTI_IMR, writing a '1' to this bit when
    * it is at '0' sets the corresponding pending bit in EXTI_PR resulting in an interrupt
    * request generation.
    * This bit is cleared by clearing the corresponding bit of EXTI_PR (by writing a ‘1’ to the bit). */
   uint32_t    swier1;
   /* Pending register (EXTI_PR1); Address offset: 0x14; Reset value: undefined
    * Bits 31:29 PRx: Pending bit on line x (x = 31 to 29)
    * Bits 28:23 Reserved, must be kept at reset value (internal lines).
    * Bits 22:0 PRx: Pending bit on line x
    * 0: No trigger request occurred
    * 1: Selected trigger request occurred
    * This bit is set when the selected edge event arrives on the external interrupt line.
    * TODO: check that only set if value of imr1 == 1
    * This bit is cleared by writing a ‘1’ to the bit. */
   uint32_t    pr1;
   uint32_t    _reserved1;
   uint32_t    _reserved2;
   /* Interrupt mask register (EXTI_IMR2); Address offset: 0x20; Reset value: 0xFFFFFFFC
    * The reset value for the internal lines (EXTI Lines 34 and 35) and the reserved lines is set to ‘1’.
    * Bits 3:0 MRx: Interrupt mask on external/internal line x+32
    * 0: Interrupt request from Line x+32 is masked
    * 1: Interrupt request from Line x+32 is not masked */
   uint32_t    imr2;
   /* Event mask register (EXTI_EMR2); Address offset: 0x24; Reset value: 0x00000000
    * Bits 3:0 MRx: Event mask on external/internal line x+32
    * 0: Event request from Line x+32 is masked
    * 1: Event request from Line x+32 is not masked */
   uint32_t    emr2;
   /* Rising trigger selection register (EXTI_RTSR2); Address offset: 0x28; Reset value: 0x00000000
    * Bits 1:0 TRx: Rising trigger event configuration bit of line x+32
    * 0: Rising trigger disabled (for Event and Interrupt) for input line x+32
    * 1: Rising trigger enabled (for Event and Interrupt) for input line x+32.
    * The external wakeup lines are edge-triggered. No glitches must be generated on these
    * lines. If a rising edge on an external interrupt line occurs during a write operation to the
    * EXTI_RTSR register, the pending bit is not set. */
   uint32_t    rtsr2;
   /* Falling trigger selection register (EXTI_FTSR2); Address offset: 0x2C; Reset value: 0x00000000
    * Bits 1:0 TRx: Falling trigger event configuration bit of line x+32
    * 0: Falling trigger disabled (for Event and Interrupt) for input line x+32
    * 1: Falling trigger enabled (for Event and Interrupt) for input line x+32.
    * The external wakeup lines are edge-triggered. No glitches must be generated on these
    * lines. If a falling edge on an external interrupt line occurs during a write operation to the
    * EXTI_FTSR register, the pending bit is not set. */
   uint32_t    ftsr2;
   /* Software interrupt event register (EXTI_SWIER2); Address offset: 0x30; Reset value: 0x00000000
    * Bits 1:0 SWIERx: Software interrupt on line x+32
    * If the interrupt is enabled on this line in the EXTI_IMR, writing a '1' to this bit when
    * it is at '0' sets the corresponding pending bit in EXTI_PR resulting in an interrupt
    * request generation.
    * This bit is cleared by clearing the corresponding bit of EXTI_PR (by writing a ‘1’ to the bit). */
   uint32_t    swier2;
   /* Pending register (EXTI_PR2); Address offset: 0x34; Reset value: undefined
    * Bits 1:0 PRx: Pending bit on line x+32
    * 0: No trigger request occurred
    * 1: Selected trigger request occurred
    * This bit is set when the selected edge event arrives on the external interrupt line.
    * This bit is cleared by writing a ‘1’ into the bit. */
   uint32_t    pr2;
} exti_t;


// section: inline implementation

static inline void assert_offset_exti(void)
{
   static_assert(offsetof(exti_t, imr1) == 0);
   static_assert(offsetof(exti_t, emr1) == 4);
   static_assert(offsetof(exti_t, rtsr1) == 8);
   static_assert(offsetof(exti_t, ftsr1) == 12);
   static_assert(offsetof(exti_t, swier1) == 16);
   static_assert(offsetof(exti_t, pr1) == 20);
   static_assert(offsetof(exti_t, imr2) == 0+32);
   static_assert(offsetof(exti_t, emr2) == 4+32);
   static_assert(offsetof(exti_t, rtsr2) == 8+32);
   static_assert(offsetof(exti_t, ftsr2) == 12+32);
   static_assert(offsetof(exti_t, swier2) == 16+32);
   static_assert(offsetof(exti_t, pr2) == 20+32);
}

// only external line number supported, internal line numbers are ignored
#define VALIDATE_EXTLINENR_EXTI(linenr) \
         if (23 <= linenr && (linenr <= 28 || 34 <= linenr)) return
#define REG_EXTI(reg, linenr) \
         ((volatile uint32_t*) ((uintptr_t)&EXTI->reg + (linenr & 0x20)))
#define DISABLE_BIT_EXTI(reg, linenr) \
         const uint32_t bit = 1u << (linenr&0x1f); \
         *REG_EXTI(reg, linenr) &= ~ bit
#define ENABLE_BIT_EXTI(reg, linenr) \
         const uint32_t bit = 1u << (linenr&0x1f); \
         *REG_EXTI(reg, linenr) |= bit

static inline void enable_interrupt_exti(exti_line_e linenr)
{
   ENABLE_BIT_EXTI(imr1, linenr);
}

static inline void disable_interrupt_exti(exti_line_e linenr)
{
   DISABLE_BIT_EXTI(imr1, linenr);
}

static inline void enable_event_exti(uint8_t linenr)
{
   ENABLE_BIT_EXTI(emr1, linenr);
}

static inline void disable_event_exti(uint8_t linenr)
{
   DISABLE_BIT_EXTI(emr1, linenr);
}

static inline void enable_interrupts_exti(uint32_t bits/*bits of lines 0..31*/)
{
   EXTI->imr1 |= bits;
}

static inline void disable_interrupts_exti(uint32_t bits/*bits of lines 0..31*/)
{
   EXTI->imr1 &= ~bits;
}

static inline void enable_events_exti(uint32_t bits/*bits of lines 0..31*/)
{
   EXTI->emr1 |= bits;
}

static inline void disable_events_exti(uint32_t bits/*bits of lines 0..31*/)
{
   EXTI->emr1 &= ~bits;
}

static inline void clear_interrupts_exti(uint32_t bits/*bits of lines 0..31*/)
{
   bits &= ~(0x3f << 23);  // mask internal lines
   EXTI->pr1 |= bits;
}

static inline void generate_interrupts_exti(uint32_t bits/*bits of lines 0..31*/)
{
   bits &= ~(0x3f << 23);  // mask internal lines
   EXTI->swier1 |= bits;
}

static inline void clear_interrupt_exti(exti_line_e linenr)
{
   VALIDATE_EXTLINENR_EXTI(linenr);
   ENABLE_BIT_EXTI(pr1, linenr);
}

static inline void generate_interrupt_exti(exti_line_e linenr)
{
   VALIDATE_EXTLINENR_EXTI(linenr);
   ENABLE_BIT_EXTI(swier1, linenr);
}

static inline void set_edge_exti(exti_line_e linenr, bool isRising, bool isFalling)
{
   VALIDATE_EXTLINENR_EXTI(linenr);
   const uint32_t bit = 1u << (linenr&0x1f);
   {
      volatile uint32_t *RTSR = REG_EXTI(rtsr1, linenr);
      *RTSR = (*RTSR & ~bit) | (bit & (0-(uint32_t)isRising));
   }
   {
      volatile uint32_t *FTSR = REG_EXTI(ftsr1, linenr);
      *FTSR = (*FTSR & ~bit) | (bit & (0-(uint32_t)isFalling));
   }
}

#undef VALIDATE_EXTLINENR_EXTI
#undef REG_EXTI
#undef DISABLE_BIT_EXTI
#undef ENABLE_BIT_EXTI

#endif
