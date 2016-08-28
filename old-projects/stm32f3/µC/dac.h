/* title: Digital-to-Analog-Converter

   Gibt Zugriff auf

      o 2-Kanal Digital-Analog-Wandler an Pins PA4(Kanal 1) und PA5(Kanal 2).

   Der Wandler läuft mit der Taktfrequenz von Bus APB1 (PCLK1).

   Pinout:
   ┌─────┬─────────────────────┐
   │ PA4 │ DAC1_OUT1, Channel1 │
   ├─────┼─────────────────────┤
   │ PA5 │ DAC1_OUT2, Channel2 │
   └─────┴─────────────────────┘

   Der Digital-Analog-Wandler besitzt 12-Bit Genauigkeit.
   Der Wandler akzeptiert auch 8-Bit-Werte zur Ausgabe, die
   dann in 12-Bit Wert gewandelt werden, indem diese 4-Bits
   nach links verschoben und die niederwertigsten 4-Bits auf
   0 gesetzt werden.

   Trigger:
   Er unterstützt das Konvertieren der Werte direkt nach dem Schreiben
   in einem Bustakt oder er wartet mit der Ausgabem, bis ein Signal
   eintrifft (Trigger) warten, das z.B. durch einen Timer periodisch
   generiert wird. Nach Eintreffen des Signals werden 3 Bus-Takte benötigt,
   bis der Ausgang auf den neuen Wert gesetzt ist.

   Ein Trigger besitzt den Vorteil, dass Daten zu periodisch richtigen
   Zeitpunkten mit sehr geringen Abweichungen ausgegeben werden können.
   Des Weiteren kann ein Trigger zusammen mit dem DMA-Baustein verwendet werden,
   so dass die Werte ohne CPU-Intervention direkt vom Speicher an den DAC
   weitergereicht werden können.

   DMA:
   Der DAC1 ist fest mit dem DMA2-Controller verbunden.
   Mit der Syscfg-HWUnit ist es möglich, diese Verbindung
   auf den DMA1-Controller umzuschalten.
   Der DAC1-Kanal1 wird durch Kanal3 des DMA-Controllers (DMA2 oder DMA1)
   und DAC1-Kanal2 wird durch Kanal4 des DMA-Controllers (DMA2 oder DMA1)
   unterstützt.

   Wenn der DAC mit dem Flag daccfg_DMA und einem externen Trigger konfiguriert wird,
   muss nur noch der entsprechende Kanal des DMA-Bausteins konfiguriert werden.

   Mit Aktivierung des externen Triggers (z.B. TIMER6) wird die Soundausgabe dann gestartet.

   DMA-Konfiguration für zwei unabhängige Kanäle für 8-Bit Daten:
   > config_readflash_dma(DMA2, DMA2_CHANNEL_DAC1_CH1, get8bitaddr_dac(DAC1, dac_channel_1), sounddata1, sizeof(sounddata1), dmacfg_ENABLE|dmacfg_LOOP|dmacfg_MEM_INCRADDR|dmacfg_HW_32BITDATA|dmacfg_MEM_8BITDATA);
   > config_readflash_dma(DMA2, DMA2_CHANNEL_DAC1_CH2, get8bitaddr_dac(DAC1, dac_channel_2), sounddata2, sizeof(sounddata2), dmacfg_ENABLE|dmacfg_LOOP|dmacfg_MEM_INCRADDR|dmacfg_HW_32BITDATA|dmacfg_MEM_8BITDATA);

   DMA-Konfiguration für zwei Kanäle mit gleichen 8-Bit Daten mit nur einem DMA-Kanal:
   > config_readflash_dma(DMA2, DMA2_CHANNEL_DAC1_CH1, get8bitaddr_dac(DAC1, dac_channel_DUAL), sounddata, sizeof(sounddata), dmacfg_ENABLE|dmacfg_LOOP|dmacfg_MEM_INCRADDR|dmacfg_HW_8BITDATA|dmacfg_MEM_8BITDATA);

   Die Funktion get8bitaddr_dac(DAC1, dac_channel_DUAL) gibt die Adress des 8-Bit-Datenregisters fr beide Kanäle zurück.
   Wenn der DMA darauf im 8-Bit-Modus schreibt (dmacfg_HW_8BITDATA), dann werden die 8-Bit Daten
   viermal nacheinander an den 32-Bit-Datenbus ausgegeben. Das duale Datenregister des DAC erwartet Kanal1-Daten in
   den Bits 7:0 und die Daten für Kanal 2 in den Bits 15:8. Da der DMA die 8-Bit Daten viermal in die Bytes
   7:0, 15:8, 23:16 und 31:24 spiegelt, werden auf beiden Kanaälen dieselben Daten ausgegeben.

   Typische Verwendung:
   > enable_dac_clockcntrl();
   > enable_gpio_clockcntrl(GPIOA_BIT);
   > config_analog_gpio(GPIOA, GPIO_PIN4);
   >
   > config_dac(DAC1, dac_channel_1, daccfg_DISABLE_TRIGGER|daccfg_ENABLE_CHANNEL);
   >
   > config_systick((8000000+11025/2) / 11025, systickcfg_CORECLOCK); // 11KHz
   > enable_interrupt_systick();
   > start_systick();
   > ...
   > // Systick Interrupt
   > void systick_interrupt(void)
   > {
   >    static uint32_t counter = 0;
   >    set_8bit_dac(DAC1, dac_channel_1, sounddata[counter++]);
   >    ++ counter;
   >    if (counter == sizeof(sounddata)) { counter = 0; stop_systick(); }
   > }

   Precondition:
    - Include "µC/hwmap.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/hwmap.h
    Header file <Hardware-Memory-Map>.

   file: µC/dac.h
    Header file <Digital-to-Analog-Converter>.

   file: TODO: dac.c
    Implementation file <Digital-to-Analog-Converter impl>.
*/
#ifndef STM32F303xC_MC_DAC_HEADER
#define STM32F303xC_MC_DAC_HEADER

// == exported Peripherals/HW-Units
#define DAC1 ((dac_t*)HW_REGISTER_BASEADDR_DAC1)

// == exported types
struct dac_t;

/* enums: daccfg_e
 * Der Trigger legt das Event fest, das den in das dhr-Register geschriebenen Datenwert in das dor-Register übertragt.
 * Falls kein Trigger oder ein Software-Trigger ausgewählt wird, wird der Wert in einem APB1 Bus-Takt übertragen.
 * Alle anderen Trigger benötigen 3 Bus-Takte.
 *
 * daccfg_DISABLE_TRIGGER - Der Wert der in eines der dhr? Register geschrieben wird (für Kanal 1,2 oder d für beide)
 *                          wird nach einem Takt in eines oder beide Ausgabe-Register (dor?) übertragen.
 * daccfg_ENABLE_TRIGGER  - Der Wert der in eines der dhr? Register geschrieben wird (für Kanal 1,2 oder d für beide)
 *                          wird beim Signalisieren des ausgewählten Events in eines oder beide Ausgabe-Register (dor?) übertragen.
 * daccfg_TRIGGER_TIMER6  - Der Trigger Output des TIMER6 legt den Zeitpunkt der Übertragung fest.
 * daccfg_TRIGGER_TIMER7  - Der Trigger Output des TIMER7 legt den Zeitpunkt der Übertragung fest.
 * daccfg_TRIGGER_TIMER15 - Der Trigger Output des TIMER15 legt den Zeitpunkt der Übertragung fest.
 * daccfg_TRIGGER_TIMER2  - Der Trigger Output des TIMER2 legt den Zeitpunkt der Übertragung fest.
 * daccfg_TRIGGER_TIMER4  - Der Trigger Output des TIMER4 legt den Zeitpunkt der Übertragung fest.
 * daccfg_TRIGGER_EXTI_LINE9 - Der externe Pin, gesteuert über über EXTI HWUnit legt den Zeitpunkt der Übertragung fest.
 * daccfg_TRIGGER_SOFTWARE - Ein Sofwtwareimpuls legt den Zeitpunkt der Übertragung fest. Genauer gesagt der Aufruf
 *                           der Funktion swtrigger_dac. Dieser Trigger unterstützt keinen DMA-Zugriff.
 * daccfg_DMA             - Schaltet die DMA-Unterstützung ein. Zusätzlich muss daccfg_ENABLE_TRIGGER eingeschaltet
 *                          werden und ein anderer Trigger als daccfg_TRIGGER_SOFTWARE ausgewählt werden.
 *                          DMA-Übertragungen werden zusammen mit Triggern unterstützt.
 * daccfg_INTERRUPT_DMA_UNDERRUN - Erzeugt einen Interupt, wenn der DAC1 seit der letzten Ausgabe keine neuen Daten
 *                                 über DMA empfangen hat. Dies kann passieren wenn zuwenig Busbandbreite vorhanden ist.
 *                                 !! Dieses Flag ist *noch nicht implementiert* !!
 * daccfg_ENABLE_CHANNEL  - Schaltet den DAC am Ende der Funktion config_dac zusätzlich ein, so dass ein zusätzlicher
 *                          Aufruf von enable_dac nicht mehr nötig ist.
 * */
typedef enum daccfg_e {
   // -- 1. choose one of
   daccfg_DISABLE_TRIGGER  = 0,  // (default)
   daccfg_ENABLE_TRIGGER   = 8,
   // -- 2. choose one of (only if daccfg_TRIGGER_ENABLE is set)
   daccfg_TRIGGER_TIMER6   = 0,  // (default)
   daccfg_TRIGGER_TIMER3   = 1,  //  or Timer 8 TRGO event depending on the value of DAC_TRIG_RMP bit in SYSCFG_CFGR1 register
   daccfg_TRIGGER_TIMER7   = 2,
   daccfg_TRIGGER_TIMER15  = 3,
   daccfg_TRIGGER_TIMER2   = 4,
   daccfg_TRIGGER_TIMER4   = 5,
   daccfg_TRIGGER_EXTI_LINE9 = 6,
   daccfg_TRIGGER_SOFTWARE = 7,
   // -- additonal flags ored to others. Default is disabled --
   daccfg_DMA = 16,
   daccfg_INTERRUPT_DMA_UNDERRUN = 32,
   daccfg_ENABLE_CHANNEL = 64,
   // -- do not use MASK values --
   daccfg_TRIGGER_MASK = 7,
} daccfg_e;

typedef enum dac_channel_e {
   dac_channel_1 = 1, // DAC1_OUT1 == GPIO_PIN4/GPIOA
   dac_channel_2 = 2, // DAC1_OUT2 == GPIO_PIN5/GPIOA
   dac_channel_DUAL = dac_channel_1|dac_channel_2
} dac_channel_e;


// == exported functions

static inline void config_dac(volatile struct dac_t *dac, dac_channel_e channel, daccfg_e config);
static inline void enable_dac(volatile struct dac_t *dac, dac_channel_e channel);
static inline void disable_dac(volatile struct dac_t *dac, dac_channel_e channel);
static inline int isenabled_dac(const volatile struct dac_t *dac, dac_channel_e channel);
static inline uint32_t value_dac(const volatile struct dac_t *dac, dac_channel_e c);
static inline void set_8bit_dac(volatile struct dac_t *dac, dac_channel_e c, uint32_t value8bit);
static inline void set_12bit_dac(volatile struct dac_t *dac, dac_channel_e c, uint32_t value12bit);
static inline void swtrigger_dac(volatile struct dac_t *dac, dac_channel_e channel);
static inline volatile uint32_t* get8bitaddr_dac(volatile struct dac_t *dac, dac_channel_e c);
static inline volatile uint32_t* get12bitaddr_dac(volatile struct dac_t *dac, dac_channel_e c);


// == definitions

typedef volatile struct dac_t {
   /* DAC control register; Address offset: 0x00; Reset value: 0x00000000 */
   uint32_t cr;
   /* DAC software trigger register; Address offset: 0x04; Reset value: 0x00000000
    * Bit 1 SWTRIG2: DAC channel2 software trigger (sw set, cleared hw), 1: Software trigger enabled
    * Bit 0 SWTRIG1: DAC channel1 software trigger (sw set, cleared hw), 1: Software trigger enabled
    * These bits are cleared by hardware (one APB1 clock cycle later) once the DAC_DHR2 or DAC_DHR1
    * register value has been loaded into the DAC_DOR2 or DAC_DOR1 register. */
   uint32_t swtrigr;
   struct {
      /* DAC channel1/channel2 12-bit right-aligned data holding register; Address offset: 0x08; Reset value: 0x00000000
       * Bits 11:0: DAC channel1/2 12-bit right-aligned data
       * Bits 27:16: DAC channel2 12-bit right-aligned data (only channel[dac_channel_DUAL-1]) */
      uint32_t dhr12r;
      /* DAC channel1/channel2 12-bit left-aligned data holding register; Address offset: 0x0C; Reset value: 0x00000000
       * Bits 15:4: DAC channel1/2 or channel2 12-bit left-aligned data
       * Bits 31:20: DAC channel2 12-bit left-aligned data (only channel[dac_channel_DUAL-1]) */
      uint32_t dhr12l;
      /* DAC channel1/channel2 8-bit right-aligned data holding register; Address offset: 0x10; Reset value: 0x00000000
       * Bits 7:0 DAC channel1/2 8-bit right-aligned data
       * Bits 15:8: DAC channel2 8-bit right-aligned data (only channel[dac_channel_DUAL-1]) */
      uint32_t dhr8r;
   } channel[3/*1,2,d(ual)*/];
   /* DAC channel1 data output register; Address offset: 0x2C; Reset value: 0x00000000
    * Bits 11:0 readonly DAC channel1 data output */
   uint32_t dor1;
   /* DAC channel2 data output register; Address offset: 0x30; Reset value: 0x00000000
    * Bits 11:0 readonly DAC channel2 data output */
   uint32_t dor2;
   /* DAC status register; Address offset: 0x34; Reset value: 0x00000000
    * Bit 29 DMAUDR2: DAC channel2 DMA underrun flag (cleared by writing 1)
    * Bit 13 DMAUDR1: DAC channel1 DMA underrun flag (cleared by writing 1) */
   uint32_t sr;
} dac_t;

#define HW_REGISTER_BIT_DAC_CR_DMAUDRIE2 (1u << 29)// DMAUDRIE2: DAC channel2 DMA underrun interrupt enable
#define HW_REGISTER_BIT_DAC_CR_DMAEN2  (1u << 28)  // DMAEN2: DAC channel2 DMA enable
#define HW_REGISTER_BIT_DAC_CR_TSEL2_POS     19u   // TSEL2[2:0]: DAC channel2 trigger selection (bits cannot be changed when the EN2 bit is set).
#define HW_REGISTER_BIT_DAC_CR_TSEL2_BITS    0x7
#define HW_REGISTER_BIT_DAC_CR_TSEL2_MASK    HW_REGISTER_MASK(DAC, CR, TSEL2)
#define HW_REGISTER_BIT_DAC_CR_TEN2    (1u << 18)  // TEN2: DAC channel2 trigger enable
#define HW_REGISTER_BIT_DAC_CR_BOFF2   (1u << 17)  // BOFF2: DAC channel2 output buffer disable
#define HW_REGISTER_BIT_DAC_CR_EN2     (1u << 16)  // EN2: DAC channel2 enable
#define HW_REGISTER_BIT_DAC_CR_DMAUDRIE1 (1u << 13)// DMAUDRIE1: DAC channel1 DMA Underrun Interrupt enable
#define HW_REGISTER_BIT_DAC_CR_DMAEN1  (1u << 12)  // DMAEN1: DAC channel1 DMA enable
#define HW_REGISTER_BIT_DAC_CR_TSEL1_POS     3u    // TSEL1[2:0]: DAC channel1 trigger selection (bits cannot be changed when the EN1 bit is set).
#define HW_REGISTER_BIT_DAC_CR_TSEL1_BITS    0x7
#define HW_REGISTER_BIT_DAC_CR_TSEL1_MASK    HW_REGISTER_MASK(DAC, CR, TSEL1)
#define HW_REGISTER_BIT_DAC_CR_TEN1    (1u << 2)   // TEN1: DAC channel1 trigger enable
#define HW_REGISTER_BIT_DAC_CR_BOFF1   (1u << 1)   // BOFF1: DAC channel1 output buffer disable
#define HW_REGISTER_BIT_DAC_CR_EN1     (1u << 0)   // EN1: DAC channel1 enable


// section: inline implementation

static inline void assert_offset_dac(void)
{
   static_assert(offsetof(dac_t, cr)      == 0);
   static_assert(offsetof(dac_t, swtrigr) == 0x04);
   static_assert(offsetof(dac_t, channel[0].dhr12r) == 0x08);
   static_assert(offsetof(dac_t, channel[0].dhr12l) == 0x0c);
   static_assert(offsetof(dac_t, channel[0].dhr8r)  == 0x10);
   static_assert(offsetof(dac_t, channel[1].dhr12r) == 0x08+1*0xc);
   static_assert(offsetof(dac_t, channel[1].dhr12l) == 0x0c+1*0xc);
   static_assert(offsetof(dac_t, channel[1].dhr8r)  == 0x10+1*0xc);
   static_assert(offsetof(dac_t, channel[2].dhr12r) == 0x08+2*0xc);
   static_assert(offsetof(dac_t, channel[2].dhr12l) == 0x0c+2*0xc);
   static_assert(offsetof(dac_t, channel[2].dhr8r)  == 0x10+2*0xc);
   static_assert(offsetof(dac_t, dor1) == 0x2c);
   static_assert(offsetof(dac_t, dor2) == 0x30);
   static_assert(offsetof(dac_t, sr)   == 0x34);
}

/* Konfiguriert einen oder beide durch Parameter channel festgelegte Kanäle des dac.
 * Die möglichen Konfigurionswerte werden in dem letzten Paramter config zusammengefasst.
 * Die Erklärung aller Konfigurationsbits steht in der Dokumentation zum enum-Typ daccfg_e.
 */
static inline void config_dac(volatile struct dac_t *dac, dac_channel_e channel, daccfg_e config)
{
   // disable dac
   uint32_t cr = dac->cr;
   if ((channel&dac_channel_1) != 0) {
      dac->cr &= ~HW_REGISTER_BIT_DAC_CR_EN1; // disable to allow change of configuration
      // for channel1: disable channel and trigger and enable output buffer
      cr &= ~( HW_REGISTER_BIT_DAC_CR_DMAUDRIE1    // disable channel1 DMA Underrun Interrupt
               |HW_REGISTER_BIT_DAC_CR_DMAEN1      // disable DAC channel1 DMA
               |HW_REGISTER_BIT_DAC_CR_TSEL1_MASK  // clear DAC channel1 trigger selection
               |HW_REGISTER_BIT_DAC_CR_TEN1        // disable DAC channel1 trigger
               |HW_REGISTER_BIT_DAC_CR_BOFF1       // enable DAC channel1 output buffer
               |HW_REGISTER_BIT_DAC_CR_EN1         // disable DAC channel1 enable
               );
      cr |= (config & daccfg_TRIGGER_MASK) << HW_REGISTER_BIT_DAC_CR_TSEL1_POS;
      static_assert(daccfg_DMA < HW_REGISTER_BIT_DAC_CR_DMAEN1);
      cr |= (config & daccfg_DMA) * (HW_REGISTER_BIT_DAC_CR_DMAEN1/daccfg_DMA);
      static_assert(daccfg_ENABLE_TRIGGER > HW_REGISTER_BIT_DAC_CR_TEN1);
      cr |= (config & daccfg_ENABLE_TRIGGER) / (daccfg_ENABLE_TRIGGER/HW_REGISTER_BIT_DAC_CR_TEN1);
      static_assert(daccfg_ENABLE_CHANNEL > HW_REGISTER_BIT_DAC_CR_EN1);
      cr |= (config & daccfg_ENABLE_CHANNEL) / (daccfg_ENABLE_CHANNEL/HW_REGISTER_BIT_DAC_CR_EN1);
   }
   if ((channel&dac_channel_2) != 0) {
      dac->cr &= ~HW_REGISTER_BIT_DAC_CR_EN2; // disable to allow change of configuration
      cr &= ~( HW_REGISTER_BIT_DAC_CR_DMAUDRIE2    // disable channel2 DMA Underrun Interrupt
               |HW_REGISTER_BIT_DAC_CR_DMAEN2      // disable DAC channel2 DMA
               |HW_REGISTER_BIT_DAC_CR_TSEL2_MASK  // clear DAC channel2 trigger selection
               |HW_REGISTER_BIT_DAC_CR_TEN2        // disable DAC channel2 trigger
               |HW_REGISTER_BIT_DAC_CR_BOFF2       // enable DAC channel2 output buffer
               |HW_REGISTER_BIT_DAC_CR_EN2         // disable DAC channel2 enable
               );
      cr |= (config & daccfg_TRIGGER_MASK) << HW_REGISTER_BIT_DAC_CR_TSEL2_POS;
      static_assert(daccfg_DMA < HW_REGISTER_BIT_DAC_CR_DMAEN2);
      cr |= (config & daccfg_DMA) * (HW_REGISTER_BIT_DAC_CR_DMAEN2/daccfg_DMA);
      static_assert(daccfg_ENABLE_TRIGGER < HW_REGISTER_BIT_DAC_CR_TEN2);
      cr |= (config & daccfg_ENABLE_TRIGGER) * (HW_REGISTER_BIT_DAC_CR_TEN2/daccfg_ENABLE_TRIGGER);
      static_assert(daccfg_ENABLE_CHANNEL < HW_REGISTER_BIT_DAC_CR_EN2);
      cr |= (config & daccfg_ENABLE_CHANNEL) * (HW_REGISTER_BIT_DAC_CR_EN2/daccfg_ENABLE_CHANNEL);
   }
   dac->cr = cr;
}

/* Schaltet alle in channel aufgeführten Kanäle an.
 * Die Funktion isenabled_channel liefert danach 1 zurück. */
static inline void enable_dac(dac_t *dac, dac_channel_e channel)
{
   uint32_t cr = dac->cr;
   if ((channel&dac_channel_1) != 0) {
      cr |= HW_REGISTER_BIT_DAC_CR_EN1;
   }
   if ((channel&dac_channel_2) != 0) {
      cr |= HW_REGISTER_BIT_DAC_CR_EN2;
   }
   dac->cr = cr;
}

/* Schaltet alle in channel aufgeführten Kanäle aus.
 * Die Funktion isenabled_channel liefert danach 0 zurück. */
static inline void disable_dac(dac_t *dac, dac_channel_e channel)
{
   uint32_t cr = dac->cr;
   if ((channel&dac_channel_1) != 0) {
      cr &= ~HW_REGISTER_BIT_DAC_CR_EN1;
   }
   if ((channel&dac_channel_2) != 0) {
      cr &= ~HW_REGISTER_BIT_DAC_CR_EN2;
   }
   dac->cr = cr;
}

/* Gibt 1 zurück, wenn alle in channel aufgeführten Kanäle aktiviert sind, sonst 0.
 * Mit enable_dac und disable_dac können einzelne Kanäle ein- und ausgeschaltet werden. */
static inline int isenabled_dac(const dac_t *dac, dac_channel_e channel)
{
   uint32_t cr = dac->cr;
   uint32_t mask = (channel&dac_channel_1) != 0 ? HW_REGISTER_BIT_DAC_CR_EN1 : 0;
   if ((channel&dac_channel_2) != 0) {
      mask |= HW_REGISTER_BIT_DAC_CR_EN2;
   }
   return (cr & mask) == mask;
}

/* Gibt den am Ausgangspin aktiven 12-Bit-Wert zurück.
 * Wurde der aktive Wert als 8-Bit Wert geschrieben, dann sind die kleinsten 4 Bits immer 0. */
static inline uint32_t value_dac(const dac_t *dac, dac_channel_e c)
{
   return (c & dac_channel_2) ? dac->dor2 : dac->dor1;
}

/* Gibt 8-Bit Wert auf Kanal 1 oder 2 oder beiden Kanälen aus.
 * Falls c==dac_channel_DUAL, dann enthält (uint8_t)value8bit den ersten Kanal
 * und (uint8_t)(value8bit >> 8) den zweiten Kanalwert.
 * Der Wert wird intern in ein "Holding"-Register geschrieben. Wurde kein Trigger konfiguriert,
 * ist dieser Wert ein Takt später in das Ausgangsregister übertragen und am Ausgang verfügbar.
 * Ansonsten muss die Übertragung noch explizit mit einem Trigger angestossen werden.
 */
static inline void set_8bit_dac(dac_t *dac, dac_channel_e c, uint32_t value8bit)
{
   dac->channel[c-1].dhr8r = value8bit; // written to dor1/dor2 on next trigger or after 1 clock cycle (notrigger)
}

/* Gibt 12-Bit Wert auf Kanal 1 oder 2 oder beiden Kanälen aus.
 * Falls c==dac_channel_DUAL, dann enthält (uint16_t)value12bit den ersten Kanal
 * und (uint16_t)(value12bit >> 16) den zweiten Kanalwert.
 * Der Wert wird intern in ein "Holding"-Register geschrieben. Wurde kein Trigger konfiguriert,
 * ist dieser Wert ein Takt später in das Ausgangsregister übertragen und am Ausgang verfügbar.
 * Ansonsten muss die Übertragung noch explizit mit einem Trigger angestossen werden.
 */
static inline void set_12bit_dac(dac_t *dac, dac_channel_e c, uint32_t value12bit)
{
   dac->channel[c-1].dhr12r = value12bit; // written to dor1/dor2 on next trigger or after 1 clock cycle (notrigger)
}

/* Triggert die Übertragung vom internen "Holding"-Register zum Ausgaberegister.
 * Diese Funktion kommt nur dann zur Anwendung, wenn der dac mit daccfg_TRIGGER_SOFTWARE
 * konfiguriert wurde. 1-Takt nach Aufruf diese Funktion steht der zuletztz mit set_8bit_dac/set_12bit_dac
 * in das "Holding-"Register geschriebene Wert am AUsgang zur Verfügung. */
static inline void swtrigger_dac(dac_t *dac, dac_channel_e channel)
{
   dac->swtrigr = channel;
}

/* Ermittelt die Registeradresse zum Schreiben von 8-Bit Werten für den entsprechenden Kanal.
 * Wurde c==dac_channel_DUAL gesetzt, dann werden die Bits 7:0 als Wert für Kanal 1
 * verwendet und die Bits 15:8 als Wert für Kanal 2. */
static inline volatile uint32_t* get8bitaddr_dac(dac_t *dac, dac_channel_e c)
{
   return &dac->channel[c-1].dhr8r;
}

/* Ermittelt die Registeradresse zum Schreiben von 12-Bit Werten für den entsprechenden Kanal.
 * Wurde c==dac_channel_DUAL gesetzt, dann werden die Bits 11:0 als Wert für Kanal 1
 * verwendet und die Bits 27:16 als Wert für Kanal 2. */
static inline volatile uint32_t* get12bitaddr_dac(dac_t *dac, dac_channel_e c)
{
   return &dac->channel[c-1].dhr12r;
}

#endif
