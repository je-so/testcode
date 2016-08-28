#include "konfig.h"
#include "hwunit/lcd_ili9341.h"
#include <math.h>

// forward
static void loop();

/*
   Ansteurung des ILI9341 TFT LCD Single Chip Driver
   240x320 Auflösung mit 262K Farben.

   Das Modul lcd_ili9341.c steuert den Controller per Software-SPI an
   (die Ansteuerung der SPI HW-Unit wird Thema eines anderen Testprojektes sein).

   Ein günstiges 2.2 LCD TFT Display mit Controller ILI9341 kann über Amazon bezogen werden,
   z.B. das »DAOKAI® 2,2-Zoll-Serial Port TFT SPI« oder das »2,2" QVGA TFT LCD Display mit SPI
   und SD-Karten-Slot für Arduino, Raspberry Pi«.

   Das LCD - Modul wird auf eine Lochrasterplatine gesteckt.
   Von links nach rechts sollten die Anschlüsse folgendermaßen belegt sein:

   MISO, LED, SCK, MOSI, D/C, RESET, CS, GND, VCC

   Pinout:
   3.3V    -> LED, VCC
   0V/GND  -> GND
   PA1     -> SCK  // Serielle Takt (Clock) für synchrone Datenübertragung
   PA3     -> MOSI // Master out Slave in, Datenübertragung von µC zu LCD-Modul
   PA2     -> D/C  // kennzeichnet gesendetes Bytes als Data(High) oder Command(Low)
   PA5     -> RESET // Reset des LCD-Moduls, Low aktiv
   PA7     -> CS   // Chip Select Leitung, Low aktiv, d.h. Chip ist aktiv zur Datenübernahme


   Das Testprogramm zeichnet eine einfache Sinuskurve auf den Bildschirm.
   Mit Druck auf den User-Button wird die AUsgabe angehalten.

 */
int main(void)
{
   enable_fpu(1);
   enable_gpio_clockcntrl(GPIO_PORTA_BIT|GPIO_PORTE_BIT|getportconfig_lcd());
   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15, 8));
   write1_gpio(GPIO_PORTE, GPIO_PIN8);

#if 1
   // Setzt Taktfrequenz auf 72 MHz
   // Mit dieser Taktfrequenz können maximal 5 ganze Bildschirmseiten
   // pro Sekunde befüllt werden. Theoretisch möglich wären ca. 6 bis 7.
   setsysclock_clockcntrl(clock_PLL);
#endif

   init_lcd();

   fillscreen_lcd(0xffff);

   // In main Typ float zu benutzen ist nicht erlaubt,
   // da erst in main die FPU angeschaltet wird.

   loop();
}

static void loop()
{
   uint16_t offset = 0;
   float a = 0, b = 0, c = 0;

   config_systick(getHZ_clockcntrl() / (1000/5) /*5ms*/, systickcfg_CORECLOCK|systickcfg_START);

   while (1) {
      if (read_gpio(GPIO_PORTA, GPIO_PIN0) != 0) {
         write1_gpio(GPIO_PORTE, GPIO_PINS(15, 8));
         while (read_gpio(GPIO_PORTA, GPIO_PIN0)) ;
         write0_gpio(GPIO_PORTE, GPIO_PINS(15, 9));
      }

      scrolly_lcd(offset);

      uint16_t x = (uint16_t) (120.0f + (115.0f-40.0f)*sinf(a) + 30.0f*sinf(b) + 10.0f*sinf(c));
      uint16_t y = (320-offset) % 320;
      fillrect_lcd(0, y, x-4, y, 0);
      fillrect_lcd(x-4, y, x+3, y, 0xffff);
      fillrect_lcd(x+4, y, 239, y, 0);
      a += 0.05f;
      if (a > (2*3.1415926f)) {
         a = 0;
      }
      b += 2.1f*0.05f;
      if (b > (2*3.1415926f)) {
         b = 0;
      }
      c += 3.3f*0.05f;
      if (c > (2*3.1415926f)) {
         c = 0;
      }
      ++ offset;
      if (offset >= 320) {
         offset = 0;
      }

      if (getHZ_clockcntrl() == 72000000) {
         while (!isexpired_systick()) ;
         start_systick();
      }
   }
}
