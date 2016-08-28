#include "konfig.h"

/*
  Dieses Testprogramm schaltet die Pins 8 bis 15 von PortE als Ausgang.
  An diesen 8 Pins sind die LEDs- des STM32F3-Discovery Boards angeschlossen.

  Pin 0 des Ports A wird als Eingang konfiguriert. An diesem Pin ist
  der blaue User-Button des Boards angeschlossen. Der Knopf besitzt
  positive Logik, wird er gedrückt, liegt am Eingang 3.3V, was als
  1-Wert gelesen wird.

  Nach Start des Programms leuchten die LEDs der Reihe nach kurz auf.
  Danach wartet das Programm auf das Drücken des User-Buttons.

  Wurde er gedrückt, werden die LEDs der Reihe nach eingechaltet und
  wieder ausgeschaltet "bis ans Ende aller Tage".

*/

int main(void)
{
   enable_gpio_clockcntrl(GPIOA_BIT|GPIOE_BIT);
   config_input_gpio(GPIOA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIOE, GPIO_PINS(15,8));

   uint16_t mask = GPIO_PINS(15,12);
   for (int i = 0; i < 10; ++i) {
      write_gpio(GPIOE, mask, GPIO_PINS(15,8)&~mask);
      for (volatile int i = 0; i < 50000; ++i) ;
      mask >>= 1;
      mask = (mask & GPIO_PINS(15,8)) | ((mask & GPIO_PINS(7,0)) << 8);
   }
   write_gpio(GPIOE, 0, GPIO_PINS(15,8));

   /* Warte, bis User Button gedrückt */
   while (read_gpio(GPIOA, GPIO_PIN0) == 0) ;

   while (1) {
      for (int round = 0; round < 1; ++round) {
         for (int pin = 15; pin >= 8; --pin) {
            write1_gpio(GPIOE, GPIO_PIN(pin));
            for (volatile int i = 0; i < 50000; ++i) ;
         }
         for (int pin = 8; pin <= 15; ++pin) {
            write_gpio(GPIOE, 0, GPIO_PIN(pin)/*off*/);
            for (volatile int i = 0; i < 50000; ++i) ;
         }
      }
      for (int round = 0; round < 1; ++round) {
         for (int pin = 8; pin <= 15; ++pin) {
            write_gpio(GPIOE, GPIO_PIN(pin)/*on*/, 0);
            for (volatile int i = 0; i < 50000; ++i) ;
         }
         for (int pin = 15; pin >= 8; --pin) {
            write0_gpio(GPIOE, GPIO_PIN(pin));
            for (volatile int i = 0; i < 50000; ++i) ;
         }
      }
   }
}
