#include "konfig.h"

unsigned isLigthON = 0;
unsigned offmultiple = 2;

void systick_interrupt(void)
{
   if (isLigthON) {
      // Aus-Zeit ist jetzt in current value geladen
      write0_gpio(GPIO_PORTE, GPIO_PINS(15,12));
      isLigthON = 0;
      // An-Zeit wird beim nächsten Interrupt in value Register geladen
      setperiod_systick(8000000/10000);
   } else {
      // An-Zeit ist jetzt in current value geladen
      write1_gpio(GPIO_PORTE, GPIO_PINS(15,12));
      isLigthON = 1;
      // Aus-Zeit wird beim nächsten Interrupt in value Register geladen
      setperiod_systick(8000000/10000*offmultiple);
   }
}

/*
   Schaltet die Onboard LEDs ein und aus, um einen PWM-Timer
   mit dem systick Timer zu emulieren.

   Die Hälfte der LEDs leuchtet mit voller Stärke, um einen
   Vergleich zu haben.

   Mit einem Druck auf den User-Button werden die LEDs dunkler.
   Nach mehreren Tastendrücken werden diese wieder auf ihre
   Anfangsleuchtstärke zurückgesetzt.
*/
int main(void)
{
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch*/|GPIO_PORTE_BIT/*led*/);

   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   config_systick(8000000/10000/*0.1msec An-Zeit*/, systickcfg_CORECLOCK|systickcfg_INTERRUPT|systickcfg_START);

   write1_gpio(GPIO_PORTE, GPIO_PINS(11,8));

   while (1) {
      if (read_gpio(GPIO_PORTA, GPIO_PIN0) == 1) {
         offmultiple += offmultiple / 2;
         if (offmultiple > 200) offmultiple = 2;
         for (volatile int i = 0; i < 100000; ++i) ;
         // while (read_gpio(GPIO_PORTA, GPIO_PIN0) == 1) ;
      }
   }

}
