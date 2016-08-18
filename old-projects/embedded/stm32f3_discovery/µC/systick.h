/* title: CortexM4-Timer

   Gibt Zugriff auf

      o SysTick - 24-Bit clear-on-write, decrementing, wrap-on-zero Timer

   Der Systicktimer ist ein 24-Bit Timer, der mit demselben Takt läuft
   wie der Prozessor (systick_clock_CORE) oder mit einem herstellerspezifischen
   Takt (systick_clock_OTHER (Achtel des CPU-Taktes für STM32F303xC)).

   Der Systick-Timer besitzt ein 24-Bit abwärtszählendes Register.
   Wird mit dem nächsten Bustakt der erreichte Wert 1 auf 0 dekrementiert,
   wird ein internes Flag ("expire") gesetzt und zusätzlich ein Interrupt ausgelöst,
   falls diese eingeschaltet ist.

   Der muss das "Expired"-Flag nicht löschen, denn der Interrupt wird unabhängig
   davon nur mit dem Zählen von 1 auf 0 ausgelöst. Ein ausgeführter Interrupt
   setzt dieses Flag auch nicht automatisch zurück. Nur Das Lesen des "Expired"-Flags
   mit isexpired_systick setzt dieses automatisch auf 0 zurück.

   Der Prototyp der Interruptfunktion ist »void systick_interrupt(void);«.

   Hat der Zählerstand den Wert 0 erreicht, wird dieser mit dem nächsten Bustakt
   wieder auf seinen ursprünglichen Wert zurückgesetzt, der mit dem Parameter
   nrticks_per_period konfiguriert wurde. Intern wird nrticks_per_period um eins
   verringert, da der Timer immer von 0 aus beginnt. D.h. bei einem Wert
   von nrticks_per_period == 3 wird intern der "Reload"-Wert auf 2 gesetzt
   und es ergibt sich folgender Ablauf des Timerzählers:

   > start_systick(): <???->0> "Start setzt Zähler auf 0"
   > 1-Clock: <0->2> "Setze Reload-Value"
   > 1-Clock: <2->1> "Dekrement"
   > 1-Clock" <1->0> "Dekrement" + "Setze Expired-Flag" + "Löse Interrupt aus falls eingeschaltet"

   D.h das "Expired"-Flag wird nach genau 3 Taken gesetzt mit einem "Reload"-Value von 2.
   Die minimale unterstützte Timerperiode ist 2 (0->1, 1->0).

   Precondition:
    - Include "µC/core.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/core.h
    Common header file <CortexM4-Core-Peripherals>.

   file: µC/systick.h
    Header file <CortexM4-Timer>.

   file: TODO: systick.c
    Implementation file <CortexM4-Timer impl>.
*/
#ifndef CORTEXM4_MC_SYSTICK_HEADER
#define CORTEXM4_MC_SYSTICK_HEADER

// == exported Peripherals/HW-Units

// == exported types

typedef enum systick_clock_e {
   systick_clock_OTHER = 0, /* Use vendor specific reference clock (AHB/8 for STM32F303xC) */
   systick_clock_CORE       /* use core clock the AHB Bus and the processor runs with */
} systick_clock_e;

typedef enum systickcfg_e {
   // 1. -- select one of --
   systickcfg_CORECLKDIV8 = 0,      // (default) HCLK divided by 8
   systickcfg_CORECLK     = 1 << 2, // HCLK (CPU clock)
   // -- flags could be added to others. disabled is default */
   systickcfg_INTERRUPT   = 1 << 1, // enable systick interrupt ("void systick_interrupt(void)")
   systickcfg_ENABLE      = 1 << 0, // starts systick timer
} systickcfg_e;

// == exported functions

static inline int config_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/, systickcfg_e config);
static inline int setperiod_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/);
static inline uint32_t period_systick(void);
static inline uint32_t value_systick(void);
static inline unsigned isexpired_systick(void);
static inline void enable_interrupt_systick(void);
static inline void disable_interrupt_systick(void);
static inline void stop_systick(void);
static inline void start_systick(void);
static inline void continue_systick(void);

// == definitions

// === Register der core hwunit SysTick

/* Registeroffset (CTRL) Control and Status Register von core hwunit SysTick */
#define HW_REGISTER_OFFSET_SYSTICK_CTRL    0x000
/* Registeroffset (LOAD) Reload Value Register (24 Bit) von core hwunit SysTick */
#define HW_REGISTER_OFFSET_SYSTICK_LOAD    0x004
/* Registeroffset (VAL) Current Value Register (24 Bit) von core hwunit SysTick */
#define HW_REGISTER_OFFSET_SYSTICK_VAL     0x008

// === Bits der SysTick Register

#define HW_REGISTER_BIT_SYSTICK_CTRL_COUNTFLAG  (1u << 16)  // Returns 1 if timer counted to 0 since last time this was read.
#define HW_REGISTER_BIT_SYSTICK_CTRL_CLKSOURCE  (1u << 2)   // Clock source selection: 0: AHB/8 ; 1: Processor clock (AHB)
#define HW_REGISTER_BIT_SYSTICK_CTRL_CLKSOURCE_POS 2u
#define HW_REGISTER_BIT_SYSTICK_CTRL_TICKINT    (1u << 1)   // 1: enable SysTick exception request
#define HW_REGISTER_BIT_SYSTICK_CTRL_ENABLE     (1u << 0)   // 1: Counter enabled

#define HW_REGISTER_BIT_SYSTICK_LOAD_RELOAD_POS    0u
#define HW_REGISTER_BIT_SYSTICK_LOAD_RELOAD_BITS   0x00ffffff
#define HW_REGISTER_BIT_SYSTICK_LOAD_RELOAD_MASK   HW_REGISTER_MASK(SYSTICK, LOAD, RELOAD)

// section: inline implementation

static inline void assert_config_systick(void)
{
   static_assert(systickcfg_CORECLKDIV8 == 0);
   static_assert(systickcfg_CORECLK     == HW_REGISTER_BIT_SYSTICK_CTRL_CLKSOURCE);
   static_assert(systickcfg_INTERRUPT   == HW_REGISTER_BIT_SYSTICK_CTRL_TICKINT);
   static_assert(systickcfg_ENABLE      == HW_REGISTER_BIT_SYSTICK_CTRL_ENABLE);
}

static inline int config_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/, systickcfg_e config)
{
   uint32_t ticks = nrticks_per_period-1;
   if (ticks == 0 || (ticks & 0xff000000) != 0) {
      return EINVAL;
   }
   HW_REGISTER(SYSTICK, CTRL) = 0;  // disable + default config values
   HW_REGISTER(SYSTICK, LOAD) = ticks;
   HW_REGISTER(SYSTICK, VAL)  = 0; // reset counter ==> ensure full period before expiration
   HW_REGISTER(SYSTICK, CTRL) = (uint32_t) (config);
   return 0;
}

/* function: setperiod_systick
 * Setzt die neue Periode des Timers.
 * Die Neue wird erst ab dem nächsten Interrupt, bzw. wenn isexpired_systick true liefert, aktiv.
 * Solange bleibt die noch die Alte aktiv. */
static inline int setperiod_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/)
{
   uint32_t ticks = nrticks_per_period-1;
   if (ticks == 0 || (ticks & ~HW_REGISTER_BIT_SYSTICK_LOAD_RELOAD_BITS) != 0) return EINVAL;
   HW_REGISTER(SYSTICK, LOAD) = ticks; // HW_REGISTER_BIT_SYSTICK_LOAD_RELOAD_POS == 0
   return 0;
}

static inline uint32_t period_systick(void)
{
   return HW_REGISTER(SYSTICK, LOAD) + 1;
}

/* Liest den aktuellen Zählerstand aus. Läuft von period_systick()-1 bis 0. */
static inline uint32_t value_systick(void)
{
   return HW_REGISTER(SYSTICK, VAL);
}

/* function: isexpired_systick
 * Zeigt an, ob eine Timerperiode abgelaufen ist.
 * Diese Flag wird mit dem Dekrementieren des Zählerstandes von 1 auf 0 gesetzt.
 * Der Interrupt wird unabhängig gleichzeitig mit dem Setzen dieses Flags ausgelöst,
 * er ist aber von diesem Unabhängig. D.h. wird dieses Flag nicht durch Lesen desselben
 * zurückgesetzt, wird der Interrupt trotzdem nicht mehrfach ausgeführt, sondern immer nur
 * beim nächsten Übergang des Zählers von 1 bis 0.
 *
 * Returns:
 * 0 - System Timer nicht abgelaufen.
 * 1 - System Timer abgelaufen. Flag, welches das Ende der Periode anzeigt, wurde zurückgesetzt. */
static inline unsigned isexpired_systick(void)
{
   return (HW_REGISTER(SYSTICK, CTRL) / HW_REGISTER_BIT_SYSTICK_CTRL_COUNTFLAG) & 1u;
}

static inline void enable_interrupt_systick(void)
{
   HW_REGISTER(SYSTICK, CTRL) |= HW_REGISTER_BIT_SYSTICK_CTRL_TICKINT;
}

static inline void disable_interrupt_systick(void)
{
   HW_REGISTER(SYSTICK, CTRL) &= ~HW_REGISTER_BIT_SYSTICK_CTRL_TICKINT;
}

static inline void stop_systick(void)
{
   HW_REGISTER(SYSTICK, CTRL) &= ~HW_REGISTER_BIT_SYSTICK_CTRL_ENABLE;
}

static inline void start_systick(void)
{
   HW_REGISTER(SYSTICK, CTRL) &= ~HW_REGISTER_BIT_SYSTICK_CTRL_ENABLE; // disable
   HW_REGISTER(SYSTICK, VAL)   = 0; // reset counter ==> ensure full period before expiration
   HW_REGISTER(SYSTICK, CTRL) |= HW_REGISTER_BIT_SYSTICK_CTRL_ENABLE; // enable
}

static inline void continue_systick(void)
{
   HW_REGISTER(SYSTICK, CTRL) |= HW_REGISTER_BIT_SYSTICK_CTRL_ENABLE; // enable without reset of counter
}

#endif
