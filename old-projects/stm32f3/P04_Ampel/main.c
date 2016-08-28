#include "konfig.h"

int main(void)
{
   // Zwei Ampeln: LED Pins rot,gelb,grün: ampel[][0..2]
   uint16_t ampel[2][3] = {
      { GPIO_PIN(13), GPIO_PIN(14), GPIO_PIN(15) },
      { GPIO_PIN(9), GPIO_PIN(10), GPIO_PIN(11) }
   };
   enum { Rot, Gelb, Gruen };
   enum { Nord_Gruen, Nord_Gelb, Nord_Rot, Ost_Gruen, Ost_Gelb, Ost_Rot };

   struct state_t {
      uint16_t licht;
      uint8_t  wartezeit; /* in Einheiten 200msec */
      uint8_t  weiter[2];
   } states[6] = {
      [Nord_Gruen] = { .licht = ampel[0][Gruen]+ampel[1][Rot], .wartezeit = 5*5, .weiter = { Nord_Gruen, Nord_Gelb}},
      [Nord_Gelb] = { .licht = ampel[0][Gelb]+ampel[1][Rot], .wartezeit = 5*2, .weiter = { Nord_Rot, Nord_Rot}},
      [Nord_Rot] = { .licht = ampel[0][Rot]+ampel[1][Rot]+ampel[1][Gelb], .wartezeit = 5*2, .weiter = { Ost_Gruen, Ost_Gruen}},
      [Ost_Gruen] = { .licht = ampel[0][Rot]+ampel[1][Gruen], .wartezeit = 5*5, .weiter = { Ost_Gruen, Ost_Gelb}},
      [Ost_Gelb] = { .licht = ampel[0][Rot]+ampel[1][Gelb], .wartezeit = 5*2, .weiter = { Ost_Rot, Ost_Rot}},
      [Ost_Rot] = { .licht = ampel[1][Rot]+ampel[0][Rot]+ampel[0][Gelb], .wartezeit = 5*2, .weiter = { Nord_Gruen, Nord_Gruen}},
   };

   /*  Kreuzung mit Autovorkehr von Nord und Ost.

       ampel[0]
       | ↓  |
     ---    ---
             ←  ampel[1]
     ---    ---
       |    |

       Der gedrückte Userbutton zeigt an, dass der Verkehr nun auf die
       andere Verkehrsrichtung wechseln soll.
       Der Verkehr im Gruen-Zustamd soll mindestens 5 Sekunden fahren dürfen.
       Im Gelb-Zustand soll exakt 2+2 Sekunden verblieben werden.
   */

   enable_gpio_clockcntrl(GPIO_PORTA_BIT|GPIO_PORTE_BIT);
   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   config_systick(8000000/5/*wait 200msec if 8Mhz bus clock*/, systickcfg_CORECLOCK);

   uint8_t s = Nord_Gruen;
   uint8_t s_old = Ost_Gelb;

   while (1) {
      uint16_t aus = GPIO_PINS(15,8);
      uint16_t an  = 0;
      aus &= ~ states[s].licht;
      an  |= states[s].licht;
      write_gpio(GPIO_PORTE, an, aus);
      start_systick();
      for (unsigned i = 0; i < states[s].wartezeit; ++i) {
         while (!isexpired_systick()) ;
         if (s == s_old && read_gpio(GPIO_PORTA, GPIO_PIN0) != 0) break;
      }
      uint8_t isSwitchTraffic = (read_gpio(GPIO_PORTA, GPIO_PIN0) != 0) ;
      s_old = s;
      s = states[s].weiter[isSwitchTraffic];
   }

}
