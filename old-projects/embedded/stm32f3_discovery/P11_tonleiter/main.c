#include "konfig.h"

#define C2_Hz   523.251
#define H1_Hz   493.883
#define AIS1_Hz 466.164
#define A1_Hz   440.000
#define GIS1_Hz 415.305
#define G1_Hz   391.995
#define FIS1_Hz 369.994
#define F1_Hz   349.228
#define E1_Hz   329.628
#define DIS1_Hz 311.127
#define D1_Hz   293.665
#define CIS1_Hz 277.183
#define C1_Hz   261.626

#define PERIOD(f) ((uint32_t) ((8000000+f##_Hz) / (2*f##_Hz)))

const uint32_t note_period[13] = {
   PERIOD(C1),
   PERIOD(CIS1),
   PERIOD(D1),
   PERIOD(DIS1),
   PERIOD(E1),
   PERIOD(F1),
   PERIOD(FIS1),
   PERIOD(G1),
   PERIOD(GIS1),
   PERIOD(A1),
   PERIOD(AIS1),
   PERIOD(H1),
   PERIOD(C2),
};

typedef enum note_e {
   C1,
   CIS1,
   D1,
   DIS1,
   E1,
   F1,
   FIS1,
   G1,
   GIS1,
   A1,
   AIS1,
   H1,
   C2
} note_e;

volatile unsigned isON;
volatile unsigned counter;

void systick_interrupt(void)
{
   counter += period_systick();
   if (isON) {
      write0_gpio(GPIO_PORTA, GPIO_PIN4);
      isON = 0;
   } else {
      write1_gpio(GPIO_PORTA, GPIO_PIN4);
      isON = 1;
   }
}

/*
   Gibt ein paar Töne der Tonleiter aus.

   Ein Rechtecksignal der Frequenz des Tons wird mit dem
   Systicktimer erzeugt.

   Der Timer startet mit der Frequenz 2*Frequenz_des_Tons und
   pro Schwingung 2 Mal aufgerufen. Beim ersten Mal wird das
   Rechtecksignal ein- und beim zweiten Mal wieder ausgeschatlet.

   Die Tonausgabe liegt an am Pin PA4.
   Es wird eine einfache Tonleiter, pro Ton 1/2 Sekunde, ausgegeben.

   Schaltung:
      Ausgabe-Pin --> 2K -> Kopfhörer -> GND
   Am Ausgabepin muss ein etwa 1K bis 2K grosser Vorwiderstand vor
   dem Kopfhörer angeschlossen sein. Danach der Kopfhörer und dann
   0V (-, Erde, GND).

   Grösse des Wiederstandes:

   Sei Impedanz des Kopfhörers 240 Ohm Rk:

   Die Leistung P sollte kleiner 1mW betragen.

   P = I * V = V/(Rk+Rv) * (I*Rk) = (V/(Rk+Rv))**2 * Rk
   V = 3.3V, Rk = 240 Ohm, und Vorwiederstand Rv = 2K
   ==> P = 0.5mW

   Am besten wird der Kopfhörer über einen "Audio jack" /
   Klinkenbuchse 3.5mm angeschlossen.

*/
int main(void)
{
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch+audio out*/|GPIO_PORTE_BIT/*led*/);

   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTA, GPIO_PIN4);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   config_systick(8000000/*dummy*/, systickcfg_CORECLK|systickcfg_INTERRUPT);

   write1_gpio(GPIO_PORTE, GPIO_PIN8);

   while (1) {
      if (read_gpio(GPIO_PORTA, GPIO_PIN0) == 1) {
         setperiod_systick(note_period[C1]);
         counter = 0;
         isON = 1;
         start_systick();
         for (unsigned tone = C1+1; tone <= C2; ++tone) {
            unsigned led = 8 + (tone & 7);
            setpins_gpio(GPIO_PORTE, GPIO_PINS(led,led), GPIO_PINS(15,8)&~led);
            while (4000000 > counter) ;
            while (! isON) ;
            while (isON) ;
            counter = 0;
            setperiod_systick(note_period[tone]);
         }
         stop_systick();
         while (read_gpio(GPIO_PORTA, GPIO_PIN0) == 1) ;
      }
   }

}
