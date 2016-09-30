#include "konfig.h"
#include "device/lcd_ili9341.h"
#include <math.h>

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

   Das Testprogramm zeichnet einen einfachen Text in verschiedener Ausrichtung uaf den Bildschirm.
   Dazu wurde der lcd Treiber um einen Font erweitert. Im Verzeichnis embedded/util/font/ befindet
   sich ein Hilfsscript, das einen Bitmap-Font aus einem freien Android-Font erzeugt.

 */
int main(void)
{
   enable_fpu(1);
   enable_gpio_clockcntrl(GPIOA_BIT|GPIOE_BIT|getportconfig_lcd());
   config_input_gpio(GPIOA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIOE, GPIO_PINS(15, 8));
   write1_gpio(GPIOE, GPIO_PIN8);

   // Setzt Taktfrequenz auf 72 MHz
   // Mit dieser Taktfrequenz können maximal 5 ganze Bildschirmseiten
   // pro Sekunde befüllt werden. Theoretisch möglich wären ca. 6 bis 7.
   setsysclock_clockcntrl(clock_PLL);

   init_lcd();

   fillscreen_lcd(0xffff);

   config_systick(getHZ_clockcntrl()/8 /*1sec*/, systickcfg_CORECLOCKDIV8);
   char chr = 'A';

   uint8_t width  = fontwidth_lcd();
   uint8_t height = fontheight_lcd();

   while (1) {
      if (read_gpio(GPIOA, GPIO_PIN0) != 0) {
         write1_gpio(GPIOE, GPIO_PINS(15, 8));
         while (read_gpio(GPIOA, GPIO_PIN0)) ;
         write0_gpio(GPIOE, GPIO_PINS(15, 9));
      }

      for (uint8_t rotate = 0; rotate < 4; ++rotate) {
         for (int i = 0; i < 3; ++i) {
            drawascii_lcd(20 + i * width, 0, chr, 0, rotate);
         }
      }
      drawascii_lcd(120-width*2, 160-height*2, chr, 4, 0);
      ++ chr;
      if (chr == 127) chr = 32;

      start_systick();
      while (!isexpired_systick() && read_gpio(GPIOA, GPIO_PIN0) == 0) ;
   }
}
