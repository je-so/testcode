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
#define hSYSTICK    ((core_systick_t*) HW_BASEADDR_SYSTICK)

// == exported types

typedef enum systickcfg_e {
   // 1. -- select one of --
   systickcfg_CORECLOCKDIV8 = 0,      // (default) HCLK divided by 8
   systickcfg_CORECLOCK     = 1 << 2, // HCLK (CPU clock)
   // -- flags could be added to others. disabled is default */
   systickcfg_INTERRUPT     = 1 << 1, // enable systick interrupt ("void systick_interrupt(void)")
   systickcfg_START         = 1 << 0, // starts systick timer
} systickcfg_e;

// == exported functions

static inline int config_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/, systickcfg_e config);
static inline int setperiod_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/);
static inline uint32_t period_systick(void);
static inline uint32_t value_systick(void);
static inline int isexpired_systick(void);   // returns 1: Timer expired at least once and flag is reset to 0. 0: Timer is not expired.
static inline int isstarted_systick(void);   // returns 1: Systick timer is started 0: Systick rimer is stopped. BUT: Resets also the expired flag returned by isexpired_systick.
static inline int isenabled_interrupt_systick(void);  // returns 1: Systick interrupt enabled. 0: Systick interrupt disabled. BUT: Resets also the expired flag returned by isexpired_systick.
static inline void enable_interrupt_systick(void);
static inline void disable_interrupt_systick(void);
static inline void stop_systick(void);
static inline void start_systick(void);
static inline void continue_systick(void);

// == definitions


// section: inline implementation

static inline void assert_config_systick(void)
{
   static_assert(systickcfg_CORECLOCKDIV8 == 0);
   static_assert(systickcfg_CORECLOCK  == (uint32_t) HW_BIT(SYSTICK, CSR, CLKSOURCE));
   static_assert(systickcfg_INTERRUPT  == (uint32_t) HW_BIT(SYSTICK, CSR, TICKINT));
   static_assert(systickcfg_START      == (uint32_t) HW_BIT(SYSTICK, CSR, ENABLE));
}

static inline int config_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/, systickcfg_e config)
{
   uint32_t ticks = nrticks_per_period-1;
   if (ticks == 0 || (ticks & 0xff000000) != 0) {
      return EINVAL;
   }
   hSYSTICK->csr = 0;  // disable + default config values
   hSYSTICK->rvr = ticks;
   hSYSTICK->cvr = 0;  // reset counter ==> ensure full period before expiration
   hSYSTICK->csr = (uint32_t) (config);
   return 0;
}

/* function: setperiod_systick
 * Setzt die neue Periode des Timers.
 * Die Neue wird erst ab dem nächsten Interrupt, bzw. wenn isexpired_systick true liefert, aktiv.
 * Solange bleibt die noch die Alte aktiv. */
static inline int setperiod_systick(uint32_t nrticks_per_period/*[2..(1<<24)]*/)
{
   uint32_t ticks = nrticks_per_period-1;
   if (ticks == 0 || (ticks & ~HW_BIT(SYSTICK, RVR, RELOAD_MAX)) != 0) return EINVAL;
   static_assert( 0 == HW_BIT(SYSTICK, RVR, RELOAD_POS));
   hSYSTICK->rvr = ticks;
   return 0;
}

static inline uint32_t period_systick(void)
{
   return hSYSTICK->rvr + 1;
}

/* Liest den aktuellen Zählerstand aus. Läuft von period_systick()-1 bis 0. */
static inline uint32_t value_systick(void)
{
   return hSYSTICK->cvr;
}

/* function: isexpired_systick
 * Zeigt an, ob eine Timerperiode abgelaufen ist und setzt das Flag zurück.
 * Dieses Flag wird gesetzt mit dem Dekrementieren des Zählerstandes von 1 auf 0.
 * Der Interrupt wird gleichzeitig mit dem Setzen dieses Flags ausgelöst,
 * er ist aber von diesem Unabhängig. D.h. wird dieses Flag nicht durch Lesen desselben
 * zurückgesetzt, wird der Interrupt trotzdem nicht mehrfach ausgeführt, sondern immer nur
 * beim nächsten Übergang des Zählers von 1 bis 0.
 *
 * Returns:
 * 0 - System Timer nicht abgelaufen.
 * 1 - System Timer abgelaufen. Flag, welches das Ende der Periode anzeigt, wurde zurückgesetzt. */
static inline int isexpired_systick(void)
{
   return (hSYSTICK->csr >> HW_BIT(SYSTICK, CSR, COUNTFLAG_POS)) & 1;
}

/* function: isstarted_systick
 * Zeigt an, ob der Timer gestartet wurde. Setzt aber gleichzeitig das expired-Flag zurück! */
static inline int isstarted_systick(void)
{
   return (hSYSTICK->csr >> HW_BIT(SYSTICK, CSR, ENABLE_POS)) & 1;
}

/* function: isenabled_interrupt_systick
 * Zeigt an, ob der Timerinterrupt angeschaltet ist. Setzt aber gleichzeitig das expired-Flag zurück! */
static inline int isenabled_interrupt_systick(void)
{
   return (hSYSTICK->csr >> HW_BIT(SYSTICK, CSR, TICKINT_POS)) & 1;
}

static inline void enable_interrupt_systick(void)
{
   hSYSTICK->csr |= HW_BIT(SYSTICK, CSR, TICKINT);
}

static inline void disable_interrupt_systick(void)
{
   hSYSTICK->csr &= ~HW_BIT(SYSTICK, CSR, TICKINT);
}

static inline void stop_systick(void)
{
   hSYSTICK->csr &= ~HW_BIT(SYSTICK, CSR, ENABLE);
}

static inline void start_systick(void)
{
   hSYSTICK->csr &= ~HW_BIT(SYSTICK, CSR, ENABLE);   // disable
   hSYSTICK->cvr  = 0;                               // reset counter ==> ensure full period before expiration
   hSYSTICK->csr |= HW_BIT(SYSTICK, CSR, ENABLE);    // enable
}

static inline void continue_systick(void)
{
   hSYSTICK->csr |= HW_BIT(SYSTICK, CSR, ENABLE);    // enable without reset of counter
}

#endif
