#include "konfig.h"
#include "sounds.c"

// Wenn auskommentiert, dann werden zwei DMA Kanäle verwendet, ansonsten nur einer für beide DAC-Kanäle
#define USE_SINGLE_DMACHANNEL

void assert_failed_exception(const char *filename, int linenr)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(GPIOE, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
      write_gpio(GPIOE, GPIO_PIN15, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

/*
   Gibt mit 11025 Hz gesampeltes Geräusch an Pin PA4 (Kanal 1) und Pin PA5 (Kanal 2)
   mittels Digitl-Analg-Converter aus.

   Eine 3.5mm Kopfhörerbuchse sollte an die Anschlüsse GND und PA4/PA5
   angeschlossen werden per Steckbrett und Jumper-Kabel. Ein Vorwiderstand
   zwischen GND und Buchse von mind. 100Ohm und fertig.
   Kopfhörer angeschlossen, nun kann der Sound akustisch wahrgenommen werden.

   Der Sound wird per DMA in einer Endlosschleife ausgegeben.

*/
int main(void)
{
   enable_dma_clockcntrl(DMA2_BIT);
   enable_gpio_clockcntrl(GPIOA_BIT/*switch+audio-out*/|GPIOE_BIT/*led*/);
   enable_basictimer_clockcntrl(TIMER6_BIT);
   enable_dac_clockcntrl();

   // Wichtig: Zuerst IOPIN auf analog umschalten, damit keine parasitären Ströme fliessen
   config_input_gpio(GPIOA, GPIO_PIN0, GPIO_PULL_OFF);
   config_analog_gpio(GPIOA, GPIO_PIN4|GPIO_PIN6);
   config_output_gpio(GPIOE, GPIO_PINS(15,8));

   // Test config_dac
   config_dac(DAC1, dac_channel_1, daccfg_ENABLE_TRIGGER|daccfg_TRIGGER_TIMER7|daccfg_ENABLE_CHANNEL);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 2 << HW_BIT_DAC_CR_TSEL1_POS);
   config_dac(DAC1, dac_channel_2, daccfg_ENABLE_TRIGGER|daccfg_TRIGGER_TIMER4|daccfg_DMA|daccfg_ENABLE_CHANNEL);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 2 << HW_BIT_DAC_CR_TSEL1_POS);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL2_MASK) == 5 << HW_BIT_DAC_CR_TSEL2_POS);
   config_dac(DAC1, dac_channel_DUAL, daccfg_DISABLE_TRIGGER);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 0 << HW_BIT_DAC_CR_TSEL1_POS);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL2_MASK) == 0 << HW_BIT_DAC_CR_TSEL2_POS);
   config_dac(DAC1, dac_channel_1, daccfg_DISABLE_TRIGGER);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 0 << HW_BIT_DAC_CR_TSEL1_POS);
   config_dac(DAC1, dac_channel_2, daccfg_ENABLE_TRIGGER|daccfg_TRIGGER_SOFTWARE|daccfg_DMA|daccfg_ENABLE_CHANNEL);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 0 << HW_BIT_DAC_CR_TSEL1_POS);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL2_MASK) == 7 << HW_BIT_DAC_CR_TSEL2_POS);
   config_dac(DAC1, dac_channel_2, daccfg_TRIGGER_SOFTWARE|daccfg_DMA|daccfg_ENABLE_CHANNEL);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 0 << HW_BIT_DAC_CR_TSEL1_POS);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL2_MASK) == 7 << HW_BIT_DAC_CR_TSEL2_POS);
   config_dac(DAC1, dac_channel_2, daccfg_DISABLE_TRIGGER);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 0 << HW_BIT_DAC_CR_TSEL1_POS);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL2_MASK) == 0 << HW_BIT_DAC_CR_TSEL2_POS);
   config_dac(DAC1, dac_channel_1, daccfg_ENABLE_TRIGGER|daccfg_TRIGGER_TIMER6|daccfg_DMA|daccfg_ENABLE_CHANNEL);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) != 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 0 << HW_BIT_DAC_CR_TSEL1_POS);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL2_MASK) == 0 << HW_BIT_DAC_CR_TSEL2_POS);
   config_dac(DAC1, dac_channel_DUAL, daccfg_DISABLE_TRIGGER);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_DMAEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_EN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN1) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TEN2) == 0);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL1_MASK) == 0 << HW_BIT_DAC_CR_TSEL1_POS);
   assert( (DAC1->cr & HW_BIT_DAC_CR_TSEL2_MASK) == 0 << HW_BIT_DAC_CR_TSEL2_POS);

   // Initialisiere DMA, DAC1-Kanal-1 ist fest mit Kanal3 von DMA2 verbunden
   // und DAC1-Kanal-2 ist fest mit Kanal4 von DMA2 verbunden
   // Da die Sounddaten im Flashrom liegen, muss die Speicheradresse angepasst werden.
   // Der DMA-Controller kann nur von Adressbasis 0x08000000 aus auf das Flashrom zugreifen.
#ifdef USE_SINGLE_DMACHANNEL
   // Beide DAC Kanäle können mit nur einem einzigen DMA-Kanal angesteuert werden
   // Das funktioniert, weil bei einer 8-Bit Übertragung der Wert 0xXY 4 Mal als 0xXYXYXYXY übertragen wird.
   // Das DAC1-Register erwartet 2 Kanalwerte. Der erste Kanal belegt den Bitbereich 0x000000FF und
   // der zweite Kanal den Bereich 0x0000FF00.
   config_flash_dma(DMA2, DMA2_CHANNEL_DAC1_CH1, get8bitaddr_dac(DAC1, dac_channel_DUAL), ufo, sizeof(ufo), dmacfg_ENABLE|dmacfg_LOOP|dmacfg_MEM_INCRADDR|dmacfg_HW_8BITDATA|dmacfg_MEM_8BITDATA);
#else
   // Die DAC Kanäle können auch einzeln mit zwei DMA-Kanälen angesteuert werden
   config_flash_dma(DMA2, DMA2_CHANNEL_DAC1_CH1, get8bitaddr_dac(DAC1, dac_channel_1), ufo, sizeof(ufo), dmacfg_ENABLE|dmacfg_LOOP|dmacfg_MEM_INCRADDR|dmacfg_HW_32BITDATA|dmacfg_MEM_8BITDATA);
   config_flash_dma(DMA2, DMA2_CHANNEL_DAC1_CH2, get8bitaddr_dac(DAC1, dac_channel_2), ufo, sizeof(ufo), dmacfg_ENABLE|dmacfg_LOOP|dmacfg_MEM_INCRADDR|dmacfg_HW_32BITDATA|dmacfg_MEM_8BITDATA);
#endif

   // Konfiguriere DAC mit DMA-Unterstützung und Zeitgesteuert mittels Timer6
   config_dac(DAC1, dac_channel_DUAL, daccfg_ENABLE_TRIGGER|daccfg_TRIGGER_TIMER6|daccfg_DMA|daccfg_ENABLE_CHANNEL);

   // Starte Timer6 mit einer Frequenz von 11025 Hertz
   assert(config_basictimer(TIMER6, (8000000 + 11025/2) / 11025, 1, basictimercfg_TRIGOUT_UPDATE|basictimercfg_REPEAT) == 0);
   start_basictimer(TIMER6);

   // Schalte blaue LED ein
   write1_gpio(GPIOE, GPIO_PIN8);

   while (1) {
      // Stelle sicher, dass DMA noch aktiv ist (kein Fehler)
      assert(isenabled_dma(DMA2, dma_channel_3) == 1);
      if (read_gpio(GPIOA, GPIO_PIN0) == 1) {
         // Schalte alle LED ein, falls der User-Button gedrückt wird
         write1_gpio(GPIOE, GPIO_PINS(15,9));
         while (read_gpio(GPIOA, GPIO_PIN0) == 1) ;
         write0_gpio(GPIOE, GPIO_PINS(15,9));
      }
   }

}
