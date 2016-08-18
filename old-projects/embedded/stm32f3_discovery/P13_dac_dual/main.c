#include "konfig.h"
#include "sounds.c"

volatile unsigned counter;
volatile unsigned soundnr;

// Selektiere, ob und welcher Trigger aktiv sein soll
// #define USE_SWTRIGGER
// #define USE_EXTI_LINE9

void systick_interrupt(void)
{
   // Falls ein Trigger konfiguriert wirde,
   // wird hiermit der Wert aus dem letzten Aufruf von systick
   // an den Ausgang des DAC übertragen

#if defined(USE_SWTRIGGER)
   swtrigger_dac(DAC1, dac_channel_DUAL);
#elif defined(USE_EXTI_LINE9)
   // EXTI_LINE9 ist dem Pin9 eines beliebigen Ports zugeordnet
   // und falls die HWUnit syscfg nicht eingeschaltet wurde und konfiguriert,
   // wird immer PortA verwendet
   write1_gpio(GPIO_PORTA, GPIO_PIN9);
   __asm( "nop" );
   write0_gpio(GPIO_PORTA, GPIO_PIN9);
#endif

   if (soundnr == 0) {
      set_8bit_dac(DAC1, dac_channel_DUAL, (shoot[counter] << 8) | shoot[counter]);
      ++counter;
      if (counter >= lengthof(shoot)) {
         counter = 0;
         soundnr = 1;
      }
   } else if (soundnr == 1) {
      set_8bit_dac(DAC1, dac_channel_DUAL, (ufo[counter] << 8) | ufo[counter]);
      ++counter;
      if (counter >= lengthof(ufo)) {
         counter = 0;
         soundnr = 2;
      }
   } else {
      set_8bit_dac(DAC1, dac_channel_DUAL, (explosion[counter] << 8) | explosion[counter]);
      ++counter;
      if (counter >= lengthof(explosion)) {
         // Falls mit Trigger gearbeitet wird, dann wird der letzte Wert nicht ausgegeben,
         // erst mit dem erneuten Starten von systick
         stop_systick();
         counter = 0;
         soundnr = 0;
      }
   }
}

/*
   Gibt mit 11025 Hz gesampeltes Geräusch an Pin PA4 und PA5 mittels Digital-Analog-Converter aus.

   Eine 3.5mm Kopfhörerbuchse sollte an die Anschlüsse GND, PA4 und PA5
   angeschlossen werden per Steckbrett und Jumper-Kabel. Ein Vorwiderstand
   zwischen GND und Buchse von mind. 100Ohm und fertig.
   Kopfhörer angeschlossen, nun kann der Sound akustisch wahrgenommen werden.

   Ein Druck auf den User-Button erzeugt ein Schussgeräusch, dann ein UFO und danach erklingt eine Explosion.
   Wird die Usertaste erneut gedrückt, bevor das Geräusch ausgeklungen ist, wird die Ausgabe
   wiederholt, beginnend mit dem Schussgeräusch.

*/
int main(void)
{
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch+audio-out*/|GPIO_PORTE_BIT/*led*/);
   enable_dac_clockcntrl();

   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   // Wichtig: Zuerst IOPIN auf analog umschalten, damit keine parasitären Ströme fliessen
   config_analog_gpio(GPIO_PORTA, GPIO_PIN4);
#if defined(USE_EXTI_LINE9)
   config_output_gpio(GPIO_PORTA, GPIO_PIN9);
#endif
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));
#if defined(USE_SWTRIGGER)
   config_dac(DAC1, dac_channel_DUAL, daccfg_ENABLE_CHANNEL|daccfg_ENABLE_TRIGGER|daccfg_TRIGGER_SOFTWARE);
#elif defined(USE_EXTI_LINE9)
   config_dac(DAC1, dac_channel_DUAL, daccfg_ENABLE_CHANNEL|daccfg_ENABLE_TRIGGER|daccfg_TRIGGER_EXTI_LINE9);
#else
   config_dac(DAC1, dac_channel_DUAL, daccfg_ENABLE_CHANNEL|daccfg_DISABLE_TRIGGER);
#endif
   if (isenabled_dac(DAC1, dac_channel_1) != 1
      || isenabled_dac(DAC1, dac_channel_2) != 1
      || isenabled_dac(DAC1, dac_channel_DUAL) != 1) {
      // error
      write1_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   }

   config_systick((8000000+11025/2) / 11025, systickcfg_CORECLK|systickcfg_INTERRUPT);

   write1_gpio(GPIO_PORTE, GPIO_PIN8);

   while (1) {
      if (read_gpio(GPIO_PORTA, GPIO_PIN0) == 1) {
         stop_systick();
         counter = 0;
         soundnr = 0;
         start_systick();
         write1_gpio(GPIO_PORTE, GPIO_PIN9);
         while (read_gpio(GPIO_PORTA, GPIO_PIN0) == 1) ;
         write0_gpio(GPIO_PORTE, GPIO_PIN9);
      }
   }

}
