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
   an den Prozessor weitergeleitet wird und ihn aus "WFE" erweckt (Funktion waitevent_core impl. "WFE").

   Asynchrone Interne Interrupts:
   Einige HW-Units wie (UART, I2C) können Interrupts auch dann generierten, wenn das System
   im Schlafmodus ist. Diese wecken dann das Gesamtsystem wieder auf.


   Tabelle der Events und ihrer internen Verdrahtung:
   ┌─────────┬───────────────────────────────────────────────────────────────┐
   │EXTI Line│ Leitung ist verbunden mit:                                    │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI0   │ GPIO-Pin0 zu einem in SYSCFG_EXTICR1 konfigurierten Port A-F  │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │EXTI2..14│ GPIO-PinY zu einem in SYSCFG_EXTICR2 konfigurierten Port A-F  │
   ├─────────┼───────────────────────────────────────────────────────────────┤
   │ EXTI15  │ GPIO-Pin15 zu einem in SYSCFG_EXTICR4 konfigurierten Port A-F │
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

// == Exported Peripherals/HW-Units
#define EXTI   ((exti_t*)HW_BASEADDR_EXTI)

// == Exported Types
struct exti_t;

typedef enum exti_line_e {
   exti_LINE0,  exti_LINE1,  exti_LINE2,  exti_LINE3,  exti_LINE4,  exti_LINE5,  exti_LINE6,  exti_LINE7,  exti_LINE8,
   exti_LINE9,  exti_LINE10, exti_LINE11, exti_LINE12, exti_LINE13, exti_LINE14, exti_LINE15, exti_LINE16, exti_LINE17,
   exti_LINE18, exti_LINE19, exti_LINE20, exti_LINE21, exti_LINE22, exti_LINE23, exti_LINE24, exti_LINE25, exti_LINE26,
   exti_LINE27, exti_LINE28, exti_LINE29, exti_LINE30, exti_LINE31, exti_LINE32, exti_LINE33, exti_LINE34, exti_LINE35,
} exti_line_e;

// == Exported Functions
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
static inline void setedge_exti(exti_line_e linenr, bool isRising, bool isFalling);

// == Definitions

typedef volatile struct exti_t {
   uint32_t       imr1;   /* Interrupt Mask Register, rw, Offset: 0x00, Reset: 0x1F800000
                           * Enables interrupts generated from external/internal line 0..31.
                           * The reset value for the internal lines 23..28 is set to ‘1’ in order to enable the interrupt by default.
                           * Bit[x] INTENA 1: Interrupt request from Line x is enabled. 0: Interrupt request is masked.
                           */
   uint32_t       emr1;   /* Event Mask Register, rw, Offset: 0x04, Reset: 0x00000000
                           * Enables events (SEV/WFE) generated from external/internal line 0..31.
                           * Bit[x] EVTENA 1: Event request from Line x is enabled. 0: Event request is masked.
                           */
   uint32_t       rtsr1;  /* Rising Trigger Selection Register, rw, Offset: 0x08, Reset: 0x00000000
                           * Enables rising-edge detection on external line 0..22,28..31. Bits 28:23 reserved (internal lines).
                           * The external wakeup lines are edge-triggered. No glitches must be generated on these lines.
                           * If a rising edge on an external line occurs during a write operation to the RTSR1/2 register,
                           * the pending bit is not set.
                           * Bit[x] RISENA 1: Rising trigger enabled for Line x. 0: Rising trigger disabled.
                           */
   uint32_t       ftsr1;  /* Falling Trigger Selection Register, rw, Offset: 0x0C, Reset: 0x00000000
                           * Enables falling-edge detection on external line 0..22,28..31. Bits 28:23 reserved (internal lines).
                           * If a falling edge on an external line occurs during a write operation to the FTSR1/2 register,
                           * the pending bit is not set.
                           * Bit[x] FALENA 1: Falling trigger enabled for Line x. 0: Falling trigger disabled.
                           */
   uint32_t       swier1; /* Software Interrupt Event Register, rw, Offset: 0x10, Reset: 0x00000000
                           * Allows software to generates interrupt/event on external line 0..22,28..31. Bits 28:23 reserved (internal lines).
                           * Bit[x] SWIEGEN 0->1: Generates interrupt/Eventfor Line x if interrupt/event is enabled (bit must be 0 before changing to 1, clear it if it is 1). 0: Nothing generated.
                           */
   uint32_t       pr1;    /* Pending Register, rw, Offset: 0x14, Reset: Undefined
                           * Reads or clears pending state of interrupt on external line 0..22,28..31. Bits 28:23 reserved (internal lines).
                           * Bit[x] PEND reads 1: Selected trigger request occurred for Line x. 0: No trigger occurred. writes 1: Bit is cleared. 0: No effect.
                           */
   uint32_t       _res0[2];
   uint32_t       imr2;   /* Interrupt Mask Register, rw, Offset: 0x20, Reset: 0xFFFFFFFC
                           * Enables interrupts generated from external/internal line 32..35.
                           * The reset value for the internal lines 34..35 and reserved bits is set to ‘1’.
                           * Bit[x] INTENA 1: Interrupt request from Line x+32 is enabled. 0: Interrupt request is masked.
                           */
   uint32_t       emr2;   /* Event Mask Register, rw, Offset: 0x24, Reset: 0x00000000
                           * Enables events (see also SEV/WFE) generated from external/internal line 32..35.
                           * Bit[x] EVTENA 1: Event request from Line x+32 is enabled. 0: Event request is masked.
                           */
   uint32_t       rtsr2;  /* Rising Trigger Selection Register, rw, Offset: 0x28, Reset: 0x00000000
                           * Enables rising-edge detection on external line 32..33. Bits 31:2 reserved (internal lines).
                           * The external wakeup lines are edge-triggered. No glitches must be generated on these lines.
                           * If a rising edge on an external line occurs during a write operation to the RTSR1/2 register,
                           * the pending bit is not set.
                           * Bit[x] RISENA 1: Rising trigger enabled for Line x+32. 0: Rising trigger disabled.
                           */
   uint32_t       ftsr2;  /* Falling Trigger Selection Register, rw, Offset: 0x2C, Reset: 0x00000000
                           * Enables falling-edge detection on external line 32..33. Bits 31:2 reserved (internal lines).
                           * If a falling edge on an external line occurs during a write operation to the FTSR1/2 register,
                           * the pending bit is not set.
                           * Bit[x] FALENA 1: Falling trigger enabled for Line x+32. 0: Falling trigger disabled.
                           */
   uint32_t       swier2; /* Software Interrupt Event Register, rw, Offset: 0x30, Reset: 0x00000000
                           * Allows software to generates interrupt/event on external line 32..33. Bits 31:2 reserved (internal lines).
                           * Bit[x] SWIEGEN 1: Generates interrupt/Eventfor Line x+32 if interrupt/event is enabled. 0: Nothing generated.
                           */
   uint32_t       pr2;    /* Pending Register, rw, Offset: 0x34, Reset: Undefined
                           * Reads or clears pending state of interrupt on external line 32..33. Bits 31:2 reserved (internal lines).
                           * Bit[x] PEND reads 1: Selected trigger request occurred for Line x+32. 0: No trigger occurred. writes 1: Bit is cleared. 0: No effect.
                           */
} exti_t;


// == Register Descriptions

enum exti_register_e {
   /* IMR1: Interrupt Mask Register */
   HW_DEF_OFF(EXTI, IMR1,  0x00),
   /* EMR1: Event Mask Register */
   HW_DEF_OFF(EXTI, EMR1,  0x04),
   /* RTSR1: Rising Trigger Selection Register */
   HW_DEF_OFF(EXTI, RTSR1, 0x08),
   /* FTSR1: Falling Trigger Selection Register */
   HW_DEF_OFF(EXTI, FTSR1, 0x0C),
   /* SWIER1: Software Interrupt Event Register */
   HW_DEF_OFF(EXTI, SWIER1, 0x10),
   /* PR1: Pending Register */
   HW_DEF_OFF(EXTI, PR1,   0x14),
   /* IMR2: Interrupt Mask Register */
   HW_DEF_OFF(EXTI, IMR2,  0x20),
   /* EMR2: Event Mask Register */
   HW_DEF_OFF(EXTI, EMR2,  0x24),
   /* RTSR2: Rising Trigger Selection Register */
   HW_DEF_OFF(EXTI, RTSR2, 0x28),
   /* FTSR2: Falling Trigger Selection Register */
   HW_DEF_OFF(EXTI, FTSR2, 0x2C),
   /* SWIER2: Software Interrupt Event Register */
   HW_DEF_OFF(EXTI, SWIER2, 0x30),
   /* PR2: Pending Register */
   HW_DEF_OFF(EXTI, PR2,   0x34),
};



// section: inline implementation

static inline void assert_offset_exti(void)
{
   static_assert(offsetof(exti_t, imr1)   == HW_OFF(EXTI, IMR1));
   static_assert(offsetof(exti_t, emr1)   == HW_OFF(EXTI, EMR1));
   static_assert(offsetof(exti_t, rtsr1)  == HW_OFF(EXTI, RTSR1));
   static_assert(offsetof(exti_t, ftsr1)  == HW_OFF(EXTI, FTSR1));
   static_assert(offsetof(exti_t, swier1) == HW_OFF(EXTI, SWIER1));
   static_assert(offsetof(exti_t, pr1)    == HW_OFF(EXTI, PR1));
   static_assert(offsetof(exti_t, imr2)   == HW_OFF(EXTI, IMR2));
   static_assert(offsetof(exti_t, emr2)   == HW_OFF(EXTI, EMR2));
   static_assert(offsetof(exti_t, rtsr2)  == HW_OFF(EXTI, RTSR2));
   static_assert(offsetof(exti_t, ftsr2)  == HW_OFF(EXTI, FTSR2));
   static_assert(offsetof(exti_t, swier2) == HW_OFF(EXTI, SWIER2));
   static_assert(offsetof(exti_t, pr2)    == HW_OFF(EXTI, PR2));
}

// define helper macros for function implementations

#define VALIDATE_EXTLINENR_EXTI(linenr) \
         if (23 <= linenr && (linenr <= 28 || 34 <= linenr)) return        // check linenr is external line number, internal line numbers are ignored
#define REG_EXTI(reg, linenr) \
         ((volatile uint32_t*) ((uintptr_t)&EXTI->reg + (linenr & 0x20)))  // access EXTI register which holds the corresponding linenr bit
#define DISABLE_BIT_EXTI(reg, linenr) \
         const uint32_t bit = 1u << (linenr&0x1f); \
         *REG_EXTI(reg, linenr) &= ~ bit                                   // clear bit in register holding linenr
#define ENABLE_BIT_EXTI(reg, linenr) \
         const uint32_t bit = 1u << (linenr&0x1f); \
         *REG_EXTI(reg, linenr) |= bit                                     // set bit in register holding linenr

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

static inline void setedge_exti(exti_line_e linenr, bool isRising, bool isFalling)
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

// undefine helper macros
#undef VALIDATE_EXTLINENR_EXTI
#undef REG_EXTI
#undef DISABLE_BIT_EXTI
#undef ENABLE_BIT_EXTI

#endif
