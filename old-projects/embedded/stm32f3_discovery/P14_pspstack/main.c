#include "konfig.h"

volatile uint32_t nmi_counter = 0;
volatile uint32_t fault_counter = 0;

// schaltet eine LED an und alle anderen 7 aus
static void turn_on_led(uint8_t nrled)
{
   uint32_t led = 1u << (8+(nrled&0x7));
   setpins_gpio(GPIO_PORTE, led/*on*/, GPIO_PINS(15,8)&~led/*off*/);
}

void nmi_interrupt(void)
{
   ++ nmi_counter;
   turn_on_led(1);
   while (1);
}

void fault_interrupt(void)
{
   ++ fault_counter;

   // Schalte nacheinander 5 LEDs ein
   for (uint8_t led = 1; led < 6; ++led) {
      for (volatile int i = 0; i < 100000; ++i) ;
      turn_on_led(led);
   }

   __asm("push {r0-r7}\n\t"
         // Lade Funktionsadresse, die nach Rückkehr aus Interrupt ausgeführt werden soll
         "ldr  r0, =executed_after_fault_interrupt\n\t"
         "str  r0, [sp, #20]\n\t"   // lr
         "str  r0, [sp, #24]\n\t"   // PC
         "mov  r0, #0x01000000\n\t"
         "str  r0, [sp, #28]\n\t"    // PSR
         "mov  lr, #0xfffffff9\n\t"  // Interrupt Return mit MSP Stack Pointer, da PSP korrupt !
         "bx   lr\n\t"
         );
}

#define set_psp(value)   __asm( "ldr r0, =" #value); __asm( "msr PSP, r0" ::: "r0" )

static inline void select_psp(void)
{
   __asm( "mov r1, sp\n"
          "mrs r0, CONTROL\n"
          "orrs r0, #2\n"
          "msr CONTROL, r0\n"
          "mov sp, r1" ::: "cc", "r0", "r1" );
}

static inline void select_msp(void)
{
   __asm( "mrs r0, CONTROL\n"
          "bics r0, #2\n"
          "msr CONTROL, r0" ::: "cc", "r0" );
}

uint32_t calc_fib(uint32_t f)
{
   return f ? calc_fib(f-1) : 0;
}

// Schalte um auf Process Stack Pointer (PSP)
// Setze PSP auf Anfangsadresse des RAM.
// D.h. beim nächsten Schreibzugriff auf den Stack tritt ein Busfehler auf (fault_interrupt)
void gen_fault(void)
{
   select_psp(); // auf Process Stack Pointer umschalten
   set_psp(0x20000000/*RAM - Start*/);
   __asm( "mrs r0, MSP"); // Debugger: MSP
   calc_fib(10); // generiere Stackzugriffe
}

// Funktion, die nach Rückkehr aus fault_interrupt ausgeführt wird
// Der fault_interrupt wird mittels gen_fault provoziert
void executed_after_fault_interrupt(void)
{
   // Schalte alle LEDs ein
   write1_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   while (1) ;
}

/*
   Der Cortex-M4 kennt zwei Stacks, den Main-Stack (MSP) für Interrupt-Handler
   und privilegierten Code und den Process-Stack, für User-Programme.

   Dieses Programm schaltet auf den Process-Stack-Pointer um.
   Setzt ihn jedoch auf die Startadresse des RAM, so dass beim nächsten Schreibzugriff
   ein Bus-Fault Exception generiert wird.

   Der Fault-Handler (fault_interrupt) schaltet dann wieder zurück auf den MSP und
   kehrt zurück in die Funktion executed_after_fault_interrupt.

   Diese könnte eine Reinitialisierung des Systems durchführen, stattdessen
   schaltet sie nur alle LEDs ein.

   Am Ende sollten alle LEDs brennen, dann hat das Programm fehlerfrei funktioniert.

*/
int main(void)
{
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch*/|GPIO_PORTE_BIT/*led*/);
   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));

   // test Startadresse des MSP nach Reset
   if (*((uint32_t*)0) != 0x20000000 + 512) goto ONERR;

   // blaue LED
   turn_on_led(0);
   for (volatile int i = 0; i < 100000; ++i) ;

   gen_fault();

   // dieser Code wird nicht mehr ausgeführt

   // 2 grüne LEDs
   setpins_gpio(GPIO_PORTE, GPIO_PIN11|GPIO_PIN15, GPIO_PINS(14,8)-GPIO_PIN11);
   while (1) ;

ONERR:
   // 2 rote LEDs
   setpins_gpio(GPIO_PORTE, GPIO_PIN9|GPIO_PIN13, GPIO_PINS(15,8)-GPIO_PIN9-GPIO_PIN13);
   while (1) ;
}
