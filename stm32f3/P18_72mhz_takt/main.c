#include "konfig.h"

/*
  Dieses Testprogramm schaltet den PLL Taktgeber ein,
  der sich aus der externen 8 MHZ Clock speist und die
  Takfrequenz auf maximal 72 MHZ hochtaktet (Multiplikator 9).

  Der erste Druck auf den User-Button schaltet den Takt um
  von internen 8 MHZ auf PLL mit 72 MHZ.

  Ein weiterer Druck schaltet wieder zurück auf den internen Takt.
*/

int main(void)
{
   enable_gpio_clockcntrl(GPIOA_BIT|GPIOE_BIT);
   config_input_gpio(GPIOA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIOE, GPIO_PINS(15,8));
   config_systick(8000000/5, systickcfg_CORECLOCK|systickcfg_START);

   // Interner 8-MHZ Crystal wird als Systemclock benutzt (nach Reset)
   if (getsysclock_clockcntrl() != clock_INTERNAL) goto ONERR;
   // kann nicht abgeschaltet werden, da als Systemclock in Benutzung
   if (disable_clock_clockcntrl(clock_INTERNAL) != EBUSY) goto ONERR;
   if (getHZ_clockcntrl() != 8000000) goto ONERR;

   // teste externen Taktgeber
   setsysclock_clockcntrl(clock_EXTERNAL);
   if (getsysclock_clockcntrl() != clock_EXTERNAL) goto ONERR;
   // kann nicht abgeschaltet werden, da als Systemclock in Benutzung
   if (disable_clock_clockcntrl(clock_EXTERNAL) != EBUSY) goto ONERR;
   if (getHZ_clockcntrl() != 8000000) goto ONERR;

   uint16_t mask = GPIO_PINS(15,12);

   while (1) {
      write_gpio(GPIOE, mask, GPIO_PINS(15,8)&~mask);
      mask >>= 1;
      mask = (mask & GPIO_PINS(15,8)) | ((mask & GPIO_PINS(7,0)) << 8);

      /* Warte, bis User Button gedrückt */
      if (read_gpio(GPIOA, GPIO_PIN0) == 1) {
         if (getsysclock_clockcntrl() == clock_PLL) {
            setsysclock_clockcntrl(clock_INTERNAL); // 8 MHZ
            if (disable_clock_clockcntrl(clock_PLL) != 0) goto ONERR;
            if (getHZ_clockcntrl() != 8000000) goto ONERR;
         } else {
            setsysclock_clockcntrl(clock_PLL); // 72 MHZ
            // PLL verwendet externe Clock
            if (disable_clock_clockcntrl(clock_EXTERNAL) != EBUSY) goto ONERR;
            // Der UART benutzt momentan die interne Clock,
            // so dass nach Ausschalten von clock:_INTERNAL kein UART mehr
            // funktionieren würde.
            if (disable_clock_clockcntrl(clock_INTERNAL) != 0) goto ONERR;
            if (getHZ_clockcntrl() != 72000000) goto ONERR;
         }
         write1_gpio(GPIOE, GPIO_PINS(15,8));
         while (read_gpio(GPIOA, GPIO_PIN0) == 1) ;
      }

      /* Warte 1/5 bzw. 1/45 Sekunde */
      while (!isexpired_systick() && read_gpio(GPIOA, GPIO_PIN0) == 0) ;
   }

ONERR:
   write1_gpio(GPIOE, GPIO_PINS(15,8));
   while (1) ;
}
