#include "konfig.h"

// forward
static void test_queue(uint32_t size, uint32_t dma, dma_channel_e channel, uint32_t counter[size], uint32_t state[size]);

#define DATA2(offset)   offset, offset+1
#define DATA4(offset)   DATA2(offset), DATA2((offset+2))
#define DATA8(offset)   DATA4(offset), DATA4((offset+4))
#define DATA16(offset)  DATA8(offset), DATA8((offset+8))
#define DATA32(offset)  DATA16(offset), DATA16((offset+16))
#define DATA64(offset)  DATA32(offset), DATA32((offset+32))
#define DATA128(offset) DATA64(offset), DATA64((offset+64))

const static uint32_t romdata[256] = {
   DATA128(1),
   DATA128(129)
};

static uint32_t ramdata1[4096];
static uint32_t ramdata2[4096];

typedef struct dma_interrupt_t {
   uint32_t          dma;
   uint32_t          channel;
   uint32_t          counter;
   uint32_t          state;
} dma_interrupt_t;

typedef volatile struct dma_interrupt_queue_t {
   uint32_t          size;
   dma_interrupt_t   entry[10];
} dma_interrupt_queue_t;

dma_interrupt_queue_t queue;


void assert_failed_exception(const char *filename, int linenr)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(GPIO_PORTE, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
      setpins_gpio(GPIO_PORTE, GPIO_PIN15, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

static void switch_led(void)
{
   static uint32_t lednr1;
   static uint32_t lednr2;
   static uint32_t counter1;
   static uint32_t counter2;
   uint16_t off = GPIO_PIN(8 + lednr2) | GPIO_PIN(8 + lednr1);
   counter1 = (counter1 + 1) % 2;
   counter2 = (counter2 + 1) % 3;
   lednr1 = (lednr1 + (counter1 == 0)) % 8;
   lednr2 = (lednr2 + (counter2 == 0)) % 8;
   setpins_gpio(GPIO_PORTE, GPIO_PIN(8 + lednr1) | GPIO_PIN(8 + lednr2), off);
   if (getHZ_clockcntrl() > 8000000) {
      for (volatile int i = 0; i < 100000; ++i) ;
   } else {
      for (volatile int i = 0; i < 20000; ++i) ;
   }
}

#define IMPLEMENT_INTERRUPT(dma_nr, chan_nr) \
   void dma##dma_nr##_channel##chan_nr##_interrupt(void) \
   {                                                     \
      dma_t *dma = dma_nr == 1 ? DMA1 : DMA2;            \
      uint32_t counter = counter_dma(dma, chan_nr-1);    \
      uint32_t state = state_dma(dma, chan_nr-1);        \
      if (queue.size < lengthof(queue.entry)) {          \
         queue.entry[queue.size].dma     = dma_nr;       \
         queue.entry[queue.size].channel = chan_nr;      \
         queue.entry[queue.size].counter = counter;      \
         queue.entry[queue.size].state   = state;        \
         ++ queue.size;                                  \
      }                                                  \
      clearstate_dma(dma, chan_nr-1, state);             \
   }

IMPLEMENT_INTERRUPT(1, 1)
IMPLEMENT_INTERRUPT(1, 2)
IMPLEMENT_INTERRUPT(1, 3)
IMPLEMENT_INTERRUPT(1, 4)
IMPLEMENT_INTERRUPT(1, 5)
IMPLEMENT_INTERRUPT(1, 6)
IMPLEMENT_INTERRUPT(1, 7)

IMPLEMENT_INTERRUPT(2, 1)
IMPLEMENT_INTERRUPT(2, 2)
IMPLEMENT_INTERRUPT(2, 3)
IMPLEMENT_INTERRUPT(2, 4)
IMPLEMENT_INTERRUPT(2, 5)


/*
   Testet config_copy_dma, config_copyflash_dma zusammen mit Speicher-Speicher Transfer.

   Teste config_dma und config_flash_dma mit externem Trigger von TIMER6.

   Das Testprogramm wird immerfort für alle DMA-Controller und Kanäle durchlaufen.

   Tritt ein Fehler auf, blinken alle LEDs auf.

   Ein fehlerfreier Testdurchlauf bewegt zwei LEDS im Uhrzeigersinn ein Position weiter.
*/
int main(void)
{
   uint32_t channel = 0;
   dma_t   *dma = DMA1;

   enable_dma_clockcntrl(DMA1_BIT|DMA2_BIT);
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*user-switch*/|GPIO_PORTE_BIT/*user-LEDs*/);
   enable_basictimer_clockcntrl(TIMER6_BIT);

   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));

   // Schalte alle DMA interrupts für jeden Kanal einzeln
   for (uint32_t i = 0; i < 7; ++i) {
      enable_interrupt_nvic(interrupt_DMA1_CHANNEL1 + i);
      if (i < 5) enable_interrupt_nvic(interrupt_DMA2_CHANNEL1 + i);
   }

   // Test EINVAL
   assert( EINVAL == config_copyflash_dma(DMA1, dma_channel_7+1, ramdata1, romdata, lengthof(romdata), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == config_copyflash_dma(DMA2, dma_channel_5+1, ramdata1, romdata, lengthof(romdata), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == config_copy_dma(DMA1, dma_channel_7+1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == config_copy_dma(DMA2, dma_channel_5+1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == config_flash_dma(DMA1, dma_channel_7+1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == config_flash_dma(DMA2, dma_channel_5+1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == config_dma(DMA1, dma_channel_7+1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == config_dma(DMA2, dma_channel_5+1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
   assert( EINVAL == enable_dma(DMA1, dma_channel_7+1));
   assert( EINVAL == enable_dma(DMA2, dma_channel_5+1));
   assert( EINVAL == disable_dma(DMA1, dma_channel_7+1));
   assert( EINVAL == disable_dma(DMA2, dma_channel_5+1));
   assert( EINVAL == enable_interrupt_dma(DMA1, dma_channel_7+1, dmacfg_INTERRUPT));
   assert( EINVAL == enable_interrupt_dma(DMA2, dma_channel_5+1, dmacfg_INTERRUPT));
   assert( EINVAL == disable_interrupt_dma(DMA1, dma_channel_7+1, dmacfg_INTERRUPT));
   assert( EINVAL == disable_interrupt_dma(DMA2, dma_channel_5+1, dmacfg_INTERRUPT));
   assert( 0 == counter_dma(DMA1, dma_channel_7+1));
   assert( 0 == counter_dma(DMA2, dma_channel_5+1));
   assert( 0 == isenabled_dma(DMA1, dma_channel_7+1));
   assert( 0 == isenabled_dma(DMA2, dma_channel_5+1));

   for (;;) {

      switch_led();

      if (getHZ_clockcntrl() > 8000000) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
         ++ channel;
         if (dma == DMA1 && channel > dma_channel_7) {
            dma = DMA2;
            channel = 0;
         } else if (dma == DMA2 && channel > dma_channel_5) {
            dma = DMA1;
            channel = 0;
         }
      }

      for (uint32_t i = 0; i < lengthof(ramdata1); ++i) {
         ramdata1[i] = 0;
         ramdata2[i] = 2*i;
      }

      // TEST config_copyflash_dma: kopiere romdata nach ramdata1
      assert( 0 == config_copyflash_dma(dma, channel, ramdata1, romdata, lengthof(romdata), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_INTERRUPT));
      assert( 0 == isenabled_dma(dma, channel));
      assert( lengthof(romdata) == counter_dma(dma, channel));
      assert( 0 == enable_dma(dma, channel));
      assert( 1 == isenabled_dma(dma, channel));
      // counter_dma zählt abwärts bis 0
      for (uint32_t c, i = lengthof(romdata); i; i = c) {
         c = counter_dma(dma, channel);
         assert(c == 0 || c <= i-1);   // Bei jedem Schleifendurchlauf wurde mindestens 1 Wort kopiert
      }
      assert( 0 == counter_dma(dma, channel));    // Alles kopiert
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < lengthof(romdata); ++i) {
         assert( romdata[i] == ramdata1[i]);
      }
      assert( 1 == isenabled_dma(dma, channel));  // Kanal ist immer noch an
      test_queue(2, dma==DMA1?1:2, channel+1, (uint32_t[2]){ 128, 0 }, (uint32_t[2]){ dma_state_HALF, dma_state_COMPLETE });

      // TEST config_flash_dma: kopiere romdata nach ramdata1 mit TIMER6 (TIMER6 <-> DMA2/dma_channel_3)
      config_basictimer(TIMER6, 1000, 1, basictimercfg_REPEAT|basictimercfg_DMA);
      assert( 0 == config_flash_dma(DMA2, dma_channel_3, ramdata1+1, romdata, lengthof(romdata), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_MEM_INCRADDR|dmacfg_HW_INCRADDR|dmacfg_INTERRUPT));
      assert( 0 == enable_dma(DMA2, dma_channel_3));
      start_basictimer(TIMER6);
      for (volatile uint32_t i = lengthof(romdata); i; --i) {
         assert( 1 == isenabled_dma(DMA2, dma_channel_3)); // enabled ==> NO ERROR
         assert( 1 == isstarted_basictimer(TIMER6));
         assert( i == counter_dma(DMA2, dma_channel_3));
         while (!isexpired_basictimer(TIMER6)) ;
         clear_isexpired_basictimer(TIMER6);
         assert(queue.size == (i==1)+(i <= lengthof(romdata)/2+1));
      }
      stop_basictimer(TIMER6);
      assert( 1 == isenabled_dma(dma, channel));  // Kanal ist immer noch an
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < lengthof(romdata); ++i) {
         assert( romdata[i] == ramdata1[i+1]);
      }
      test_queue(2, 2, 3, (uint32_t[2]){ lengthof(romdata)/2, 0 }, (uint32_t[2]){ dma_state_HALF, dma_state_COMPLETE });

      // TEST config_copy_dma: kopiere ramdata2 nach ramdata1 in loop
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 1024, dmacfg_ENABLE|dmacfg_LOOP|dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 1 == isenabled_dma(dma, channel));
      // counter_dma zählt abwärts bis 0
      for (uint32_t c, i = 1024; i; i = c) {
         c = counter_dma(dma, channel);
         if (i <= 20 && c >= 1000) break; // Wiederholung (dmacfg_LOOP)
         assert(c == 0 || c <= i-1);      // Bei jedem Schleifendurchlauf wurde mindestens 1 Wort kopiert
      }
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < 1024; ++i) {
         assert( 2*i == ramdata1[i]);
      }
      assert( 0 == disable_dma(dma, channel));
      assert( 0 == isenabled_dma(dma, channel));
      assert( (dma_state_HALF|dma_state_COMPLETE) == state_dma(dma, channel));

      // TEST config_copy_dma: konvertiere von 8 nach 16 bit
      for (uint32_t i = 0; i < 1024; ++i) {
         ramdata1[i] = 0xffffffff;
      }
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 1024, dmacfg_ENABLE|dmacfg_HW_16BITDATA|dmacfg_MEM_8BITDATA));
      // Warte, bis alle Daten kopiert wurden
      while (counter_dma(dma, channel)) {
         assert( 1 == isenabled_dma(dma, channel));
      }
      disable_dma(dma, channel);
      assert( 0 == isenabled_dma(dma, channel));
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < 1024; ++i) {
         assert( ((uint8_t*)ramdata2)[i] == ((uint16_t*)ramdata1)[i]);
      }

      // TEST config_copy_dma: konvertiere von 8 nach 32 bit
      for (uint32_t i = 0; i < 1024; ++i) {
         ramdata1[i] = 0xffffffff;
      }
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 1024, dmacfg_ENABLE|dmacfg_HW_32BITDATA|dmacfg_MEM_8BITDATA));
      // Warte, bis alle Daten kopiert wurden
      while (counter_dma(dma, channel)) {
         assert( 1 == isenabled_dma(dma, channel));
      }
      disable_dma(dma, channel);
      assert( 0 == isenabled_dma(dma, channel));
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < 1024; ++i) {
         assert( ((uint8_t*)ramdata2)[i] == ramdata1[i]);
      }

      // TEST config_copy_dma: konvertiere von 16 nach 8 bit
      for (uint32_t i = 0; i < 1024; ++i) {
         ramdata1[i] = 0xffffffff;
      }
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 1024, dmacfg_ENABLE|dmacfg_HW_8BITDATA|dmacfg_MEM_16BITDATA));
      // Warte, bis alle Daten kopiert wurden
      while (counter_dma(dma, channel)) {
         assert( 1 == isenabled_dma(dma, channel));
      }
      disable_dma(dma, channel);
      assert( 0 == isenabled_dma(dma, channel));
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < 1024; ++i) {
         assert( (uint8_t)((uint16_t*)ramdata2)[i] == ((uint8_t*)ramdata1)[i]);
      }

      // TEST config_copy_dma: konvertiere von 16 nach 16 bit
      for (uint32_t i = 0; i < 1024; ++i) {
         ramdata1[i] = 0xffffffff;
      }
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 1024, dmacfg_ENABLE|dmacfg_HW_16BITDATA|dmacfg_MEM_16BITDATA));
      // Warte, bis alle Daten kopiert wurden
      while (counter_dma(dma, channel)) {
         assert( 1 == isenabled_dma(dma, channel));
      }
      disable_dma(dma, channel);
      assert( 0 == isenabled_dma(dma, channel));
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < 1024; ++i) {
         assert( ((uint16_t*)ramdata2)[i] == ((uint16_t*)ramdata1)[i]);
      }

      // TEST config_copy_dma: konvertiere von 16 nach 32 bit
      for (uint32_t i = 0; i < 1024; ++i) {
         ramdata1[i] = 0xffffffff;
      }
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 1024, dmacfg_ENABLE|dmacfg_HW_32BITDATA|dmacfg_MEM_16BITDATA));
      // Warte, bis alle Daten kopiert wurden
      while (counter_dma(dma, channel)) {
         assert( 1 == isenabled_dma(dma, channel));
      }
      disable_dma(dma, channel);
      assert( 0 == isenabled_dma(dma, channel));
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < 1024; ++i) {
         assert( ((uint16_t*)ramdata2)[i] == ramdata1[i]);
      }

      // TEST config_copy_dma: dmacfg_PRIORITY_MAX <-> dmacfg_PRIORITY_MIN
      uint32_t CH1 = channel == 3 ? 4 : 3;
      uint32_t CH2 = channel == 2 ? 4 : 2;
      uint32_t CH3 = channel == 1 ? 4 : 1;
      assert( 0 == config_copy_dma(dma, CH1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_PRIORITY_MAX));
      assert( 0 == config_copy_dma(dma, CH2, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_PRIORITY_HIGH));
      assert( 0 == config_copy_dma(dma, CH3, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_PRIORITY_LOW));
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_PRIORITY_MIN));
      assert( 3<<12 == (dma->channel[CH1].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 2<<12 == (dma->channel[CH2].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 1<<12 == (dma->channel[CH3].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0<<12 == (dma->channel[channel].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0 == enable_dma(dma, channel));
      assert( 0 == enable_dma(dma, CH3));
      assert( 0 == enable_dma(dma, CH2));
      assert( 0 == enable_dma(dma, CH1));
      assert( 1 == isenabled_dma(dma, channel));
      assert( 1 == isenabled_dma(dma, CH1));
      assert( 1 == isenabled_dma(dma, CH2));
      assert( 1 == isenabled_dma(dma, CH3));
      while (counter_dma(dma, CH1)) ;
      assert( 0 == disable_dma(dma, channel));
      assert( 0 == disable_dma(dma, CH1));
      assert( 0 == disable_dma(dma, CH2));
      assert( 0 == disable_dma(dma, CH3));
      volatile uint32_t counter3 = counter_dma(dma, CH3);
      volatile uint32_t counter = counter_dma(dma, channel);
      assert( counter3 < counter);
      assert( 1000 < counter);

      // TEST config_copy_dma: default priority dmacfg_PRIORITY_MIN
      CH1 = channel == 0 ? 1 : 0;
      CH2 = channel == 2 ? 1 : 2;
      CH3 = channel == 3 ? 1 : 3;
      assert( 0 == config_copy_dma(dma, CH1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 0 == config_copy_dma(dma, CH2, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 0 == config_copy_dma(dma, CH3, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 0<<12 == (dma->channel[CH1].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0<<12 == (dma->channel[CH2].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0<<12 == (dma->channel[CH3].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0<<12 == (dma->channel[channel].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0 == enable_dma(dma, CH3));
      assert( 0 == enable_dma(dma, CH2));
      assert( 0 == enable_dma(dma, CH1));
      assert( 0 == enable_dma(dma, channel));
      assert( 1 == isenabled_dma(dma, channel));
      assert( 1 == isenabled_dma(dma, CH1));
      assert( 1 == isenabled_dma(dma, CH2));
      assert( 1 == isenabled_dma(dma, CH3));
      while (counter_dma(dma, CH1)) ;
      assert( 0 == disable_dma(dma, channel));
      assert( 0 == disable_dma(dma, CH1));
      assert( 0 == disable_dma(dma, CH2));
      assert( 0 == disable_dma(dma, CH3));
      if (channel == 0 || channel == 1) {
         counter3 = counter_dma(dma, CH3);
         counter = counter_dma(dma, CH2);
      } else if (channel == 3) {
         counter3 = counter_dma(dma, channel);
         counter = counter_dma(dma, CH2);
      } else {
         counter3 = counter_dma(dma, CH3);
         counter = counter_dma(dma, channel);
      }
      assert( 1000 < counter);
      assert( 1000 < counter3);

      // TEST config_copy_dma: DMA1 <-> DMA2 round robin !!
      CH1 = dma_channel_1;
      CH2 = dma_channel_2;
      assert( 0 == config_copy_dma(DMA1, CH1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_PRIORITY_MAX));
      assert( 0 == config_copy_dma(DMA1, CH2, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_PRIORITY_MAX));
      assert( 0 == config_copy_dma(DMA2, CH1, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 0 == config_copy_dma(DMA2, CH2, ramdata1, ramdata2, lengthof(ramdata2), dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 3<<12 == (DMA1->channel[CH1].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 3<<12 == (DMA1->channel[CH2].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0<<12 == (DMA2->channel[CH1].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0<<12 == (DMA2->channel[CH2].ccr & HW_REGISTER_BIT_DMA_CCR_PL_MASK))
      assert( 0 == enable_dma(DMA1, CH1));
      assert( 0 == enable_dma(DMA1, CH2));
      assert( 0 == enable_dma(DMA2, CH1));
      assert( 0 == enable_dma(DMA2, CH2));
      assert( 1 == isenabled_dma(DMA1, CH1));
      assert( 1 == isenabled_dma(DMA1, CH2));
      assert( 1 == isenabled_dma(DMA2, CH1));
      assert( 1 == isenabled_dma(DMA2, CH2));
      while (counter_dma(DMA1, CH1)) ;
      assert( 0 == disable_dma(DMA2, CH2));
      assert( 0 == disable_dma(DMA2, CH1));
      assert( 0 == disable_dma(DMA1, CH2));
      assert( 0 == disable_dma(DMA1, CH1));
      assert( 0 == counter_dma(DMA1, CH1));  // Alle Kanäle haben die Übertragung abgeschlossen.
      assert( 0 == counter_dma(DMA1, CH2));  // Es gibt keine Bevorzugung, d.h. Kanäle zwischen
      assert( 0 == counter_dma(DMA2, CH1));  // DMA1 und DMA2 werden nicht nach Priorität
      assert( 0 == counter_dma(DMA2, CH2));  // sondern im round-robin Verfahren gesteuert.

      // TEST config_dma: TIMER6 unterbreche Kopieraktion und fahre danach fort
      for (uint32_t i = 0; i < 256; ++i) {
         ramdata1[i] = 0;
         ramdata2[i] = 5 + 3*i;
      }
      config_basictimer(TIMER6, 1000, 1, basictimercfg_REPEAT|basictimercfg_DMA);
      assert( 0 == config_dma(DMA2, dma_channel_3, ramdata1, ramdata2, 256, dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA|dmacfg_MEM_INCRADDR|dmacfg_HW_INCRADDR|dmacfg_MEM_READ));
      assert( 0 == enable_dma(DMA2, dma_channel_3));
      start_basictimer(TIMER6);
      for (uint32_t i = 256; i > 128; --i) {
         assert( 1 == isenabled_dma(DMA2, dma_channel_3)); // enabled ==> NO ERROR
         assert( 1 == isstarted_basictimer(TIMER6));
         assert( i == counter_dma(DMA2, dma_channel_3));
         while (!isexpired_basictimer(TIMER6)) ;
         clear_isexpired_basictimer(TIMER6);
      }
      // disable_dma: behält konfigurierte Werte
      disable_dma(DMA2, dma_channel_3);
      assert( 0 == isenabled_dma(DMA2, dma_channel_3));
      // Werte werden nach stop-Aufruf bewahrt
      assert( (uintptr_t)ramdata1 == DMA2->channel[dma_channel_3].cpar);
      assert( (uintptr_t)ramdata2 == DMA2->channel[dma_channel_3].cmar);
      assert( 128 == counter_dma(DMA2, dma_channel_3));
      // Prüfe Inhalt von ramdata1
      for (uint32_t i = 0; i < 128; ++i) {
         assert( 5 + 3*i == ramdata1[i]);
      }
      for (uint32_t i = 128; i < 256; ++i) {
         assert( 0 == ramdata1[i]); // noch nicht kopiert
      }
      // enable_dma: führt gestoppte Transaktion fort
      enable_dma(DMA2, dma_channel_3);
      while (counter_dma(DMA2, dma_channel_3)) ;
      assert( 0 == disable_dma(DMA2, dma_channel_3));
      stop_basictimer(TIMER6);
      // Prüfe Inhalt von ramdata1
      for (volatile uint32_t i = 0; i < 256/*256*/; ++i) {
         assert( 5 + 3*i == ramdata1[i]);
      }

      // TEST config_copy_dma: clears state flags (HALF + COMPLETE)
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 10, dmacfg_ENABLE));
      // Warte, bis alle Daten kopiert wurden
      while (counter_dma(dma, channel)) ;
      assert( 0 == disable_dma(dma, channel));
      assert( (dma_state_HALF|dma_state_COMPLETE) == state_dma(dma, channel));
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 10, dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 0 == state_dma(dma, channel)); // state wurde zurückgesetzt

      // TEST config_copy_dma: clears state flags (ERROR)
      assert( 0 == config_copy_dma(dma, channel, ramdata1, romdata/*wrong addr*/, 10, dmacfg_ENABLE)); // generiert error
      assert( 0 == isenabled_dma(dma, channel));
      assert( dma_state_ERROR == state_dma(dma, channel));
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 10, dmacfg_HW_32BITDATA|dmacfg_MEM_32BITDATA));
      assert( 0 == state_dma(dma, channel)); // state wurde zurückgesetzt

      // TEST clearstate_dma: clears state flags(HALF + COMPLETE)
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 10, dmacfg_ENABLE));
      while (counter_dma(dma, channel)) ;       // Warte, bis alle Daten kopiert wurden
      assert( 0 == disable_dma(dma, channel));
      assert( (dma_state_HALF|dma_state_COMPLETE) == state_dma(dma, channel));
      clearstate_dma(dma, channel, 0);
      assert( (dma_state_HALF|dma_state_COMPLETE) == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_ERROR);
      assert( (dma_state_HALF|dma_state_COMPLETE) == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_HALF);
      assert( (dma_state_COMPLETE) == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_COMPLETE);
      assert( 0 == state_dma(dma, channel));    // state wurde zurückgesetzt
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 10, dmacfg_ENABLE));
      while (counter_dma(dma, channel)) ;       // Warte, bis alle Daten kopiert wurden
      assert( 0 == disable_dma(dma, channel));
      assert( (dma_state_HALF|dma_state_COMPLETE) == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_COMPLETE);
      assert( (dma_state_HALF) == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_HALF);
      assert( 0 == state_dma(dma, channel));    // state wurde zurückgesetzt

      // TEST clearstate_dma: clears state flags (ERROR)
      assert( 0 == config_copy_dma(dma, channel, ramdata1, romdata/*wrong addr*/, 10, dmacfg_ENABLE)); // generiert error
      assert( 0 == isenabled_dma(dma, channel));
      assert( dma_state_ERROR == state_dma(dma, channel));
      clearstate_dma(dma, channel, 0);
      assert( dma_state_ERROR == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_HALF);
      assert( dma_state_ERROR == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_COMPLETE);
      assert( dma_state_ERROR == state_dma(dma, channel));
      clearstate_dma(dma, channel, dma_state_ERROR);
      assert( 0 == state_dma(dma, channel));    // state wurde zurückgesetzt

      // TEST disable_dma: Interrupts werden trotzdem aufgerufen
      assert( 0 == config_copy_dma(dma, channel, ramdata1, romdata/*wrong addr*/, 10, dmacfg_ENABLE));
      assert( 0 == isenabled_dma(dma, channel));
      assert( dma_state_ERROR == state_dma(dma, channel));
      assert( 0 == queue.size);
      assert( 0 == enable_interrupt_dma(dma, channel, dmacfg_INTERRUPT));  // Schalte Interrupts ein
      for (volatile unsigned i = 0; i < 100; ++i) ;
      assert( 1 == queue.size);                    // Interrupt wurde aufgerufen
      assert( 0 == state_dma(dma, channel));
      assert( 0 == config_copy_dma(dma, channel, ramdata1, ramdata2, 10, dmacfg_ENABLE));
      while (counter_dma(dma, channel)) ;          // Warte, bis alle Daten kopiert wurden
      assert( 0 == disable_dma(dma, channel));     // Kanal ausschalten
      assert( 0 == isenabled_dma(dma, channel));
      assert( (dma_state_HALF|dma_state_COMPLETE) == state_dma(dma, channel));
      queue.size = 0;
      assert( 0 == enable_interrupt_dma(dma, channel, dmacfg_INTERRUPT));  // Schalte Interrupts ein
      for (volatile unsigned i = 0; i < 100; ++i) ;
      assert( 1 == queue.size);                    // Interrupt wurde aufgerufen
      assert( 0 == state_dma(dma, channel));
      queue.size = 0;

   }

}

static void test_queue(uint32_t size, uint32_t dma, dma_channel_e channel, uint32_t counter[size], uint32_t state[size])
{
   assert(queue.size == size);

   for (uint32_t i = 0; i < size; ++i) {
      assert(queue.entry[i].dma     == dma);
      assert(queue.entry[i].channel == channel);
      if (counter[i] >= 10) {
         assert(queue.entry[i].counter <= counter[i] && counter[i]-10 <= queue.entry[i].counter);
      } else {
         assert(queue.entry[i].counter == counter[i]);
      }
      assert(queue.entry[i].state   == state[i]);
   }

   queue.size = 0;
}
