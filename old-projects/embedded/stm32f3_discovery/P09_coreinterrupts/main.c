#include "konfig.h"

volatile uint32_t counter = 0;
volatile uint32_t pos[16];

// schaltet eine LED an und alle anderen 7 aus
static void turn_on_led(uint8_t nrled)
{
   uint32_t led = 1u << (8+(nrled&0x7));
   setpins_gpio(GPIO_PORTE, led/*on*/, GPIO_PINS(15,8)&~led/*off*/);
}

void nmi_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_NMI] = counter;
}

void fault_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_FAULT] = counter;
}

void mpufault_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_MPUFAULT] = counter;
}

void busfault_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_BUSFAULT] = counter;
}

void usagefault_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_USAGEFAULT] = counter;
}

void svcall_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_SVCALL] = counter;
}

void debugmonitor_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_DEBUGMONITOR] = counter;
}

void pendsv_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_PENDSV] = counter;
}

void systick_interrupt(void)
{
   ++ counter;
   pos[coreinterrupt_SYSTICK] = counter;
}

/*
   Testet coreinterrupt_e

   Die Funktion generate_coreinterrupt wird danach getestet, ob ein Interrupt tatsächlich generiert
   und ausgeführt wird.

   Danach werden die Interrupts maskiert und is_coreinterrupt und clear_coreinterrupt
   werden getestet.

   Die Funktion ispending liefert 1, wenn ein generierter Interrupt auf Ausführung
   wartet, d.h. die Priorität ist nicht ausreichend oder alle Interrupts wurden maskiert.

   Die Funktion clearpending löscht das Pending-Flag des Interrupts, so dass ispending 0 liefert und der Interrupt
   daher nicht mehr ausgeführt wird.
*/
int main(void)
{
   uint8_t  led = 0;
   unsigned i;

   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch*/|GPIO_PORTE_BIT/*led*/);
   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));

   // Teste Interrupt Ausführung

   for (i = 0; i <= 16; ++i) {
      for (unsigned p = 0; p < sizeof(pos)/sizeof(pos[0]); ++p) {
         pos[p] = 0;
      }
      switch (i) {
      case coreinterrupt_NMI:
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         if (generate_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         if ( i == coreinterrupt_MPUFAULT
                     || i == coreinterrupt_BUSFAULT
                     || i == coreinterrupt_USAGEFAULT) {
            if (counter != 0) goto ONERR; // pending
            if (enable_coreinterrupt(i) != 0) goto ONERR;
            if (counter != 1 || pos[i] != 1) goto ONERR; // interrupt executed
            if (disable_coreinterrupt(i) != 0) goto ONERR;
         } else {
            if (counter != 1 || pos[i] != 1) goto ONERR; // interrupt executed
            if (EINVAL != enable_coreinterrupt(i)) goto ONERR;
            if (EINVAL != disable_coreinterrupt(i)) goto ONERR;
         }
         if (counter != 1) goto ONERR; // no second interrupt
         turn_on_led(led++);
         for (int i = 0; i < 50000; ++i) ;
         counter = 0;
         break;
      default:
         if (generate_coreinterrupt((coreinterrupt_e)i) != EINVAL) goto ONERR;
         if (counter != 0) goto ONERR;
         break;
      }
   }

   // Teste Interrupt Pending

   for (i = 0; i <= 16; ++i) {
      for (unsigned p = 0; p < sizeof(pos)/sizeof(pos[0]); ++p) {
         pos[p] = 0;
      }
      switch (i) {
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         enable_coreinterrupt(i);
         disable_all_interrupt();
         if (generate_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         if (is_coreinterrupt((coreinterrupt_e)i) != 1) goto ONERR;
         for (unsigned p = 0; p < 16; ++p) {
            if (is_coreinterrupt((coreinterrupt_e)p) != (p==i)) goto ONERR;
         }
         if (clear_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         if (is_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         for (unsigned p = 0; p < 16; ++p) {
            if (is_coreinterrupt((coreinterrupt_e)p) != 0) goto ONERR;
         }
         enable_all_interrupt();
         if (counter != 0) goto ONERR;
         disable_coreinterrupt(i);
         turn_on_led(led++);
         for (int i = 0; i < 50000; ++i) ;
         break;
      case coreinterrupt_NMI: // not maskable
      default:
         if (is_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         if (clear_coreinterrupt((coreinterrupt_e)i) != EINVAL) goto ONERR;
         if (counter != 0) goto ONERR;
         break;
      }
   }

   // Teste Interrupt Priority

   for (i = 0; i <= 16; ++i) {
      switch (i) {
      case coreinterrupt_MPUFAULT:
      case coreinterrupt_BUSFAULT:
      case coreinterrupt_USAGEFAULT:
      case coreinterrupt_SVCALL:
      case coreinterrupt_DEBUGMONITOR:
      case coreinterrupt_PENDSV:
      case coreinterrupt_SYSTICK:
         enable_coreinterrupt(i);
         if (setpriority_coreinterrupt((coreinterrupt_e)i, 3) != 0) goto ONERR;
         if (getpriority_coreinterrupt((coreinterrupt_e)i) != 3) goto ONERR;
         setbasepriority_interrupt(3);
         if (generate_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         if (is_coreinterrupt((coreinterrupt_e)i) != 1) goto ONERR;
         if (setpriority_coreinterrupt((coreinterrupt_e)i, 2) != 0) goto ONERR;
         if (getpriority_coreinterrupt((coreinterrupt_e)i) != 2) goto ONERR;
         if (is_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         if (counter != 1 || pos[i] != 1) goto ONERR; // interrupt executed
         counter = 0;
         // reset
         if (setpriority_coreinterrupt((coreinterrupt_e)i, 0) != 0) goto ONERR;
         if (getpriority_coreinterrupt((coreinterrupt_e)i) != 0) goto ONERR;
         disable_coreinterrupt(i);
         turn_on_led(led++);
         for (int i = 0; i < 50000; ++i) ;
         break;
      case coreinterrupt_NMI: // not maskable with priority
      default:
         break;
      }
   }

   // Teste disable_coreinterrupt bei MPUFAULT, BUSFAULT, USAGEFAULT

   generate_coreinterrupt(coreinterrupt_MPUFAULT);
   generate_coreinterrupt(coreinterrupt_BUSFAULT);
   generate_coreinterrupt(coreinterrupt_USAGEFAULT);
   if (is_coreinterrupt(coreinterrupt_MPUFAULT) != 1) goto ONERR;
   if (is_coreinterrupt(coreinterrupt_BUSFAULT) != 1) goto ONERR;
   if (is_coreinterrupt(coreinterrupt_USAGEFAULT) != 1) goto ONERR;
   clear_coreinterrupt(coreinterrupt_MPUFAULT);
   clear_coreinterrupt(coreinterrupt_BUSFAULT);
   clear_coreinterrupt(coreinterrupt_USAGEFAULT);
   if (is_coreinterrupt(coreinterrupt_MPUFAULT) != 0) goto ONERR;
   if (is_coreinterrupt(coreinterrupt_BUSFAULT) != 0) goto ONERR;
   if (is_coreinterrupt(coreinterrupt_USAGEFAULT) != 0) goto ONERR;
   if (counter != 0) goto ONERR;

   // 2 grüne LEDs
   turn_on_led(3);
   write1_gpio(GPIO_PORTE, GPIO_PIN11|GPIO_PIN15);
   while (1) ;

ONERR:
   // 2 rote LEDs
   enable_all_interrupt();
   enable_fault_interrupt();
   write1_gpio(GPIO_PORTE, GPIO_PIN9|GPIO_PIN13);
   while (1) ;
}
