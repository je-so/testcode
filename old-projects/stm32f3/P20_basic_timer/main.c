#include "konfig.h"

static volatile uint32_t s_lednr;
static volatile uint32_t s_leddesc;
static volatile uint32_t s_counter6;
static volatile uint32_t s_counter7;

// Testvariablen, die in main benutzt werden.
// Wird ein Breakpoint auf assert_failed_exception gesetzt,
// kann im Debugger diese Information einfacher gelesen werden.
static struct {
   uint32_t count;
   uint32_t time1;
   uint32_t time2;
} s_main;

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

void timer6_dac_interrupt(void)
{
   // Würde der Interrupt nicht bestätigt, käme es zu einer Endlosschleife,
   // d.h. der Interrupt würde nach Beendigung dieser Funktion sofort wieder aktiviert.
   assert(isexpired_basictimer(TIMER6));
   clear_expired_basictimer(TIMER6);
   assert(! isexpired_basictimer(TIMER6));
   ++s_counter6;
}

void timer7_interrupt(void)
{
   // Würde der Interrupt nicht bestätigt, käme es zu einer Endlosschleife,
   // d.h. der Interrupt würde nach Beendigung dieser Funktion sofort wieder aktiviert.
   assert(isexpired_basictimer(TIMER7));
   clear_expired_basictimer(TIMER7);
   assert(! isexpired_basictimer(TIMER7));
   ++s_counter7;
}

static void switch_used_led(void)
{
   write0_gpio(GPIOE, GPIO_PIN(8 + s_lednr));
   if (s_leddesc) {
      --s_lednr;
      if (s_lednr > 7) s_lednr = 7;
   } else {
      ++s_lednr;
      if (s_lednr == 8) s_lednr = 0;
   }
   write1_gpio(GPIOE, GPIO_PIN(8 + s_lednr));
   if (getHZ_clockcntrl() > 8000000) {
      for (volatile int i = 0; i < 250000; ++i) ;
   } else {
      for (volatile int i = 0; i < 50000; ++i) ;
   }
}

/* Dieses Programm ist ein reines Testprogramm des basictimer_t Typs.
 *
 * Nach jedem ausgeführten Test wird die User-LED eine Position weiter im Kreis bewegt.
 *
 * Im Fehlerfall wird durch assert die Funktion assert_failed_exception aufgerufen.
 * Diese lässt alle LEDs aufblinken.
 * Dieses Programm ist zur Ausführung im Debugger gedacht.
 *
 * Ein Ausführungssequenz mit einer fehlerhafter assert Bedingung könnte so aussehen:
 * $ make debug
 * (gdb) break assert_failed_exception
 * Breakpoint 1 at 0x438: file main.c, line 19
 * (gdb) cont
 * Continuing.
 *
 *  Breakpoint 1, assert_failed_exception (filename=0x134c "main.c", linenr=201)
 *      at main.c:19
 *  19      {
 * (gdb) print s_main
 * ...
 */
int main(void)
{
   enable_gpio_clockcntrl(GPIOA_BIT/*user-switch*/|GPIOE_BIT/*user-LEDs*/);
   config_input_gpio(GPIOA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIOE, GPIO_PINS(15,8));

   // Teste enable_timer_clockcntrl und disable_timer_clockcntrl
   switch_used_led();

   enable_basictimer_clockcntrl(TIMER6_BIT);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM6EN) != 0);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM7EN) == 0);
   enable_basictimer_clockcntrl(TIMER7_BIT);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM6EN) != 0);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM7EN) != 0);
   disable_basictimer_clockcntrl(TIMER6_BIT);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM6EN) == 0);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM7EN) != 0);
   disable_basictimer_clockcntrl(TIMER7_BIT);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM6EN) == 0);
   assert((HW_REGISTER(RCC, APB1ENR) & HW_REGISTER_BIT_RCC_APB1ENR_TIM7EN) == 0);

   enable_basictimer_clockcntrl(TIMER6_BIT|TIMER7_BIT);

   for (unsigned clock = 0; 1/*repeat forever*/; ++clock) {

      s_leddesc = ~s_leddesc;

      if ((clock&1) == 0) {
         setsysclock_clockcntrl(clock_INTERNAL/*8MHz*/);
      } else {
         setsysclock_clockcntrl(clock_PLL/*72MHz*/);
      }

      for (unsigned t = 0; t < 2; ++t) {
         basictimer_t * const timer = t == 0 ? TIMER6 : TIMER7;
         const int is6 = (timer == TIMER6);
         const int is7 = (timer == TIMER7);

         // Teste getconfig_basictimer: Teste verschiedene Kombinationen
         switch_used_led();
         for (uint32_t isonce = 0; isonce < 2; ++isonce) {
            for (uint32_t isdma = 0; isdma < 2; ++isdma) {
               for (uint32_t isint = 0; isint < 2; ++isint) {
                  for (uint32_t isint = 0; isint < 2; ++isint) {
                     for (uint32_t trig = 0; trig < 3; ++trig) {
                        uint32_t config = (isonce ? basictimercfg_ONCE : basictimercfg_REPEAT)
                                        | (isdma ? basictimercfg_DMA : 0)
                                        | (isint ? basictimercfg_INTERRUPT : 0)
                                        | (trig == 0 ? basictimercfg_TRIGOUT_RESET : 0)
                                        | (trig == 1 ? basictimercfg_TRIGOUT_START : 0)
                                        | (trig == 2 ? basictimercfg_TRIGOUT_UPDATE : 0);
                        assert(config_basictimer(timer, 2, 1, (basictimercfg_e) config) == 0);
                        assert(config == getconfig_basictimer(timer));
                     }
                  }
               }
            }
         }

         // Teste Timer zählt einmalig aufwärts
         switch_used_led();
         assert(config_basictimer(timer, 65536, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT) == 0);
         assert(! isstarted_basictimer(timer));    // Timer ist abgeschaltet
         assert(!isexpired_basictimer(timer));     // Abgelaufen-Flag gelöscht
         assert(isenabled_interrupt_basictimer(timer)); // Interrupt wird an NVIC weitergemeldet
         start_basictimer(timer);                  // schalte Timer an
         {
            uint32_t val = 0;
            while (isstarted_basictimer(timer)) {
               s_main.count = exvalue_basictimer(timer);
               if (s_main.count != 0) {
                  assert(val <= s_main.count);
                  val = s_main.count + 1;
               }
            }
         }
         assert(! isstarted_basictimer(timer));    // Timer hat sich selbst abgeschaltet
         assert(isexpired_basictimer(timer));      // Abgelaufen-Flag gesetzt
         assert(exvalue_basictimer(timer) == 0x80000000);  // Timer count == 0 + Abgelaufen
         clear_expired_basictimer(timer);          // lösche Abgelaufen-Flag
         assert(exvalue_basictimer(timer) == 0);   // Timer count == 0 ohne Abgelaufen

         // Teste Timer: stop_basictimer löscht isexpired_basictimer  flag
         switch_used_led();
         assert(config_basictimer(timer, 10000, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT) == 0);
         start_basictimer(timer);                  // schalte Timer an
         while (! isexpired_basictimer(timer)) ;   // Warte bis abgelaufen
         assert(! isstarted_basictimer(timer));    // Timer hat sich selbst abgeschaltet
         assert(isexpired_basictimer(timer));      // Abgelaufen-Flag gesetzt
         assert(exvalue_basictimer(timer) == 0x80000000);  // Timer count == 0 + Abgelaufen
         stop_basictimer(timer);                   // Löscht zusätzlich Abgelaufenflag
         assert(! isexpired_basictimer(timer));    // Abgelaufen-Flag gelöscht
         assert(exvalue_basictimer(timer) == 0);   // Timer count == 0 ohne Abgelaufen

         // Teste Timer läuft gleich dem CPU Takt (prescaler == 1)
         // Taktet HCLK mit 72MHz, dann Timerclock == 2*PCLK1 == 2*36MHz == 72MHz
         // Taktet HCLK mit 8MHz, dann Timerclock == PCLK1 == 8MHz
         switch_used_led();
         config_systick(65000, systickcfg_CORECLOCK);
         assert(config_basictimer(timer, 65000, 1, basictimercfg_INTERRUPT) == 0);
         assert(!isstarted_basictimer(timer));  // Timer nach config gestoppt
         start_systick();                       // schalte Systick an
         start_basictimer(timer);               // schalte Timer an
         while (!isexpired_systick()) ;         // Warte bis 65000 HCLK Takte vergangen (HCLK == PCLK1)
         s_main.count = exvalue_basictimer(timer); // Timer zählt aufwärts
         assert(s_main.count >= 64980);         // maximal 20 Takte Unterschied
         if ((s_main.count & 0x80000000)) {     // Flag ? Ja ==> Timer ist übergelaufen
            s_main.count = (uint16_t) s_main.count;
            assert(s_main.count <= 10);         // maximal 10 Takte Unterschied
         }
         assert(isstarted_basictimer(timer));   // Timer läuft noch
         stop_systick();                        // schalte Systick aus
         stop_basictimer(timer);                // stoppe Timer
         assert(! isstarted_basictimer(timer)); // Timer gestoppt

         // Teste Timer läuft gleich dem CPU Takt (prescaler > 1)
         for (int i = 0; i < 2; ++i) {
            switch_used_led();
            if (i == 0) {
               config_systick(2*65000, systickcfg_CORECLOCK);
               assert(config_basictimer(timer, 2, 65000, basictimercfg_INTERRUPT) == 0);
            } else {
               config_systick(50000, systickcfg_CORECLOCK);
               config_basictimer(timer, 50, 1000, basictimercfg_INTERRUPT);
            }
            start_systick();                       // schalte Systick an
            start_basictimer(timer);               // schalte Timer an
            while (!isexpired_basictimer(timer)) ; // Warte bis 2*65000 HCLK Takte vergangen (HCLK == PCLK1)
            assert(isexpired_basictimer(timer));   // Timer abgelaufen
            assert(isexpired_systick());           // Systick abgelaufen
            stop_systick();                        // schalte Systick aus
            stop_basictimer(timer);                // stoppe Timer
         }

         // Teste Timer stop und continue bezüglich Count und Prescale == 1
         switch_used_led();
         config_systick(60000, systickcfg_CORECLOCK);
         assert(config_basictimer(timer, 65536, 1, basictimercfg_INTERRUPT) == 0);
         assert(!isstarted_basictimer(timer));              // Timer nach config gestoppt
         assert(!isexpired_basictimer(timer));              // Abgelaufen-Flag gelöscht
         start_basictimer(timer);                           // schalte Timer an
         start_systick();                                   // schalte Systick nach Timer an
         while (!isexpired_systick()) ;                     // Warte bis Systick abgelaufen (60000 Takte)
         stop_basictimer(timer);                            // stoppe Timer
         assert(!isexpired_basictimer(timer));              // Abgelaufen-Flag gelöscht
         while (!isexpired_systick()) {                     // Teste, dass Timer gestoppt hat
            s_main.count = exvalue_basictimer(timer);
            assert(s_main.count >= 60000);                  // Timer Count ca. 60000
            assert(s_main.count <= 60100);                  // aber nicht mehr als 100 Takte Unterschied
         }
         s_main.count = 65536 - s_main.count;
         config_systick(s_main.count, systickcfg_CORECLOCK); // Warte Restperiode
         continue_basictimer(timer);                        // continue Timer
         start_systick();                                   // schalte Systick nach Timer an
         while (!isexpired_systick()) ;                     // Warte bis Systick abgelaufen (Restzeit)
         assert(isexpired_basictimer(timer));               // Timer ist auch abgelaufen
         assert((exvalue_basictimer(timer) & 0x80000000) != 0);   // Timer ist auch abgelaufen
         stop_systick();                                    // schalte Systick aus
         stop_basictimer(timer);                            // stoppe Timer

         // Teste Timer stop und continue mit internem Prescale Counter
         switch_used_led();
         config_systick(60000, systickcfg_CORECLOCK);
         assert(config_basictimer(timer, 2, 65536, basictimercfg_INTERRUPT) == 0);
         start_basictimer(timer);                           // schalte Timer an
         start_systick();                                   // schalte Systick nach Timer an
         while (!isexpired_systick()) ;                     // Warte bis Systick abgelaufen (60000 Takte)
         stop_basictimer(timer);                            // stoppe Timer
         while (!isexpired_systick()) {                     // Teste, dass Timer gestoppt hat
            assert(exvalue_basictimer(timer) == 0);        // Timer noch nicht gezählt (Prescale Count ca. 60000)
         }
         config_systick(2*65536-60000, systickcfg_CORECLOCK); // Warte Restperiode
         continue_basictimer(timer);                        // continue Timer
         start_systick();                                   // schalte Systick nach Timer an
         while (!isexpired_systick()) ;                     // Warte bis Systick abgelaufen (Restzeit)
         assert(exvalue_basictimer(timer) == 0x80000000);   // Timer ist auch abgelaufen
         assert(value_basictimer(timer) == 0);              // Timer count == 0
         stop_systick();                                    // schalte Systick aus
         stop_basictimer(timer);                            // schalte Timer aus

         // Teste einmaliger Timerablauf
         switch_used_led();
         assert(config_basictimer(timer, 10000, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT) == 0);
         assert(! isstarted_basictimer(timer));             // Timer gestoppt
         assert(! isexpired_basictimer(timer));             // Abgelaufen-Flag gelöscht
         clear_interrupt(interrupt_TIMER6_DAC);             // Lösche Pending Interrupt von Timer6
         clear_interrupt(interrupt_TIMER7);                 // Lösche Pending Interrupt von Timer7
         start_basictimer(timer);                           // Starte Timer
         assert(isstarted_basictimer(timer));               // Timer läuft
         while (isstarted_basictimer(timer)) ;              // Warte auf Timereende
         assert(value_basictimer(timer) == 0);              // Timer count == 0
         assert(exvalue_basictimer(timer) == 0x80000000);   // Timer count == 0 + Abgelaufen
         assert(isexpired_basictimer(timer));               // Timer abgelaufen

         // Teste Interrupt pending
         switch_used_led();
         assert(is_interrupt(interrupt_TIMER6_DAC) == is6);  // Interrupt an NVIC gemeldet
         assert(is_interrupt(interrupt_TIMER7) == is7);      // Interrupt an NVIC gemeldet
         clear_interrupt(interrupt_TIMER6_DAC);                   // Lösche Pending Interrupt von Timer6
         clear_interrupt(interrupt_TIMER7);                       // Lösche Pending Interrupt von Timer7
         for (volatile int i = 0; i < 2; ++i) ;
         assert(is_interrupt(interrupt_TIMER6_DAC) == is6);  // Interrupt an NVIC erneut gemeldet
         assert(is_interrupt(interrupt_TIMER7) == is7);      // Interrupt an NVIC erneut gemeldet
         assert(isexpired_basictimer(timer));               // Timer abgelaufen
         clear_expired_basictimer(timer);                   // Lösche Abgelaufen-Flag
         assert(! isexpired_basictimer(timer));             // Abgelaufen-Flag gelöscht
         assert(exvalue_basictimer(timer) == 0);            // Timer count == 0 ohne Abgelaufenflag
         clear_interrupt(interrupt_TIMER6_DAC);        // Lösche Pending Interrupt von Timer6
         clear_interrupt(interrupt_TIMER7);            // Lösche Pending Interrupt von Timer7
         assert(! is_interrupt(interrupt_TIMER6_DAC)); // NVIC Interruptflag gelöscht
         assert(! is_interrupt(interrupt_TIMER7));     // NVIC Interruptflag gelöscht

         // Teste NVIC Interrupts ausgeschaltet
         switch_used_led();
         assert(s_counter6 == 0);
         assert(s_counter7 == 0);

         // Teste Interruptroutine wird aufgerufen
         switch_used_led();
         // schalte Interrupt ein und Timer stoppt sich nach Ablauf selbst
         assert(config_basictimer(timer, 2, 1, basictimercfg_ONCE|basictimercfg_INTERRUPT) == 0);
         clear_interrupt(interrupt_TIMER6_DAC);     // Vorherige Interrupts haben pending-Flag in NVIC gesetzt,
         clear_interrupt(interrupt_TIMER7);         // d.h. Interruptflags löschen, so dass nur ein erneuter Interrupt gemeldet wird
         enable_interrupt(interrupt_TIMER6_DAC);       // Schalte NVIC Interrupt für Timer6 ein
         enable_interrupt(interrupt_TIMER7);           // Schalte NVIC Interrupt für Timer7 ein
         assert(s_counter6 == 0);                           // Zähler Timer6 ist 0
         assert(s_counter7 == 0);                           // Zähler Timer7 ist 0
         start_basictimer(timer);                           // Starte Timer
         while (s_counter6 == 0 && s_counter7 == 0) ;       // Warte bis Interrupt
         assert(! isstarted_basictimer(timer));             // Timer hat sich selbst abgeschaltet
         assert(! isexpired_basictimer(timer));             // Abgelaufen-Flag gelöscht von Interruptroutine
         assert(s_counter6 == is6);                         // Nur der s_counter des eingeschalteten Timers wurde erhöht
         assert(s_counter7 == is7);                         // dito
         disable_interrupt(interrupt_TIMER6_DAC);      // Schalte NVIC Interruptbehandlung aus für Timer6
         disable_interrupt(interrupt_TIMER7);          // Schalte NVIC Interruptbehandlung aus für Timer7
         assert(is_interrupt(interrupt_TIMER6_DAC) == 0);  // keinen Interrupt an NVIC gemeldet
         assert(is_interrupt(interrupt_TIMER7) == 0);      // keinen Interrupt an NVIC gemeldet
         s_counter6 = 0;
         s_counter7 = 0;

         // Teste kein Interrupt
         switch_used_led();
         assert(config_basictimer(timer, 10000, 1, basictimercfg_ONCE) == 0);
         assert(! isstarted_basictimer(timer));             // Timer ist abgeschaltet
         assert(! isenabled_interrupt_basictimer(timer));   // Interrupt wird nicht an NVIC weitergemeldet
         clear_interrupt(interrupt_TIMER6_DAC);        // Vorherige Interrupts habe pending-Flag in NVIC gesetzt,
         clear_interrupt(interrupt_TIMER7);            // d.h. Interruptflags löschen, so dass nur ein erneuter Interrupt gemeldet wird
         start_basictimer(timer);                           // Starte Timer
         assert(isstarted_basictimer(timer));               // Timer ist gestartet
         while (isstarted_basictimer(timer)) ;              // Warte bis Timerende
         assert(isexpired_basictimer(timer));               // Timer abgelaufen
         assert(value_basictimer(timer) == 0);              // Timer count == 0
         assert(exvalue_basictimer(timer) == 0x80000000);   // Timer count == 0 + Abgelaufen
                                                            // ABER:
         for (int i = 0; i < 5; ++i) {
            assert(!is_interrupt(interrupt_TIMER6_DAC));  // keinen Interrupt an NVIC gemeldet
            assert(!is_interrupt(interrupt_TIMER7));      // keinen Interrupt an NVIC gemeldet
            enable_interrupt_basictimer(timer);                // schalte Interrupt an
            assert(isenabled_interrupt_basictimer(timer));     // Interrupt wird jetzt an NVIC weitergemeldet
            assert(is_interrupt(interrupt_TIMER6_DAC) == is6);  // Interrupt an NVIC gemeldet
            assert(is_interrupt(interrupt_TIMER7) == is7);      // Interrupt an NVIC gemeldet
            disable_interrupt_basictimer(timer);               // schalte Interrupt wieder ab
            clear_interrupt(interrupt_TIMER6_DAC);        // Lösche NVIC Interruptflag Timer6
            clear_interrupt(interrupt_TIMER7);            // Lösche NVIC Interruptflag Timer7
         }

         // Teste reset_basictimer: ein Reset setzt den Timer auf 0 zurück, ohne dass er abläuft
         switch_used_led();
         assert(config_basictimer(timer, 65536, 1000, basictimercfg_INTERRUPT) == 0);
         start_basictimer(timer);                           // Starte Timer
         while (value_basictimer(timer) < 30) ;             // Warte bis 30000 Clockticks vergangen sind
         assert(!is_interrupt(interrupt_TIMER6_DAC));  // keinen Interrupt an NVIC gemeldet
         assert(!is_interrupt(interrupt_TIMER7));      // keinen Interrupt an NVIC gemeldet
         reset_basictimer(timer);                           // Setze Timer zurück
         while (value_basictimer(timer) != 0) ;             // Es dauert ein paar Takte bis das passiert
         assert(value_basictimer(timer) == 0);              // count zurückgesetzt
         assert(isstarted_basictimer(timer));               // Timer läuft noch
         assert(!isexpired_basictimer(timer));              // Timer nicht abgelaufen
         stop_basictimer(timer);                            // stoppe Timer
         assert(!is_interrupt(interrupt_TIMER6_DAC));  // keinen Interrupt an NVIC gemeldet
         assert(!is_interrupt(interrupt_TIMER7));      // keinen Interrupt an NVIC gemeldet

         // Teste resetandexpire_basictimer: setzt den Timer zurück und kennzeichnet ihn als abgelaufen
         switch_used_led();
         assert(config_basictimer(timer, 65536, 1000, basictimercfg_INTERRUPT) == 0);
         start_basictimer(timer);                           // Starte Timer
         while (value_basictimer(timer) < 30) ;             // Warte bis 30000 Clockticks vergangen sind
         assert(!is_interrupt(interrupt_TIMER6_DAC));  // keinen Interrupt an NVIC gemeldet
         assert(!is_interrupt(interrupt_TIMER7));      // keinen Interrupt an NVIC gemeldet
         resetandexpire_basictimer(timer);                  // Setze Timer zurück und kennzeichne ihn als abgelaufen
         while (value_basictimer(timer) != 0) ;             // Es dauert ein paar Takte bis das passiert
         assert(value_basictimer(timer) == 0);              // count zurückgesetzt
         assert(isstarted_basictimer(timer));               // Timer läuft noch
         assert(isexpired_basictimer(timer));               // Timer ist auch abgelaufen
         stop_basictimer(timer);                            // stoppe Timer
         assert(! isexpired_basictimer(timer));             // Stopp löscht auch Abgelaufen-Flag
         assert(is_interrupt(interrupt_TIMER6_DAC) == is6);  // Interrupt an NVIC gemeldet
         assert(is_interrupt(interrupt_TIMER7) == is7);      // Interrupt an NVIC gemeldet
         clear_interrupt(interrupt_TIMER6_DAC);        // Lösche NVIC Interruptflag Timer6
         clear_interrupt(interrupt_TIMER7);            // Lösche NVIC Interruptflag Timer7
         assert(! is_interrupt(interrupt_TIMER6_DAC)); // NVIC Interruptflag gelöscht
         assert(! is_interrupt(interrupt_TIMER7));     // NVIC Interruptflag gelöscht


         // Teste config_basictimer: Alte Timerperiod wird bei Verwendung von continue_basictimer weiterverwendet
         switch_used_led();
         config_systick(10000, systickcfg_CORECLOCK);
         assert(config_basictimer(timer, 20, 1000, basictimercfg_REPEAT) == 0);
         start_basictimer(timer);
         start_systick();
         while (!isexpired_systick()) ;         // Warte auf abgelaufenenen Systick (10000 Takte)
         assert(config_basictimer(timer, 200, 10000, basictimercfg_REPEAT) == 0); // Setze neue Timerperiode
         assert(!isstarted_basictimer(timer));  // config hat den Timer gestoppt
         assert(!isexpired_basictimer(timer));  // Timer auch noch nicht abgelaufen
         s_main.count = exvalue_basictimer(timer); // Lese Zählerstand von Timer
         assert(9  <= s_main.count);            // Mehr als 9000 Takte wurden gezählt,
         assert(10 >= s_main.count);            // aber nicht mehr als 10999 Takte
         continue_basictimer(timer);            // Starte Timer erneut, aber mittels continue
         start_systick();                       // Starte Systick für erneute 10000er Periode
         while (!isexpired_systick()) ;         // Warte auf abgelaufenenen Systick (10000 Takte)
         assert(isexpired_basictimer(timer));   // Timer ist abgelaufen, d.h. alte Werte für Periode und alter Zählerstand wurden weiterverwendet
         stop_basictimer(timer);
         stop_systick();

         // Teste update_basictimer: Werte werden erst verwendet, wenn abgelaufen
         switch_used_led();
         config_systick(0xffffff, systickcfg_CORECLOCK);
         assert(config_basictimer(timer, 10, 100, basictimercfg_REPEAT) == 0);
         start_basictimer(timer);
         start_systick();
         update_basictimer(timer, 7, 1000);     // setze neue Werte
         while (!isexpired_basictimer(timer)) ; // Warte auf abgelaufenenen Timer
         s_main.time1 = value_systick();        // ab hier - nach Timerablauf - werden die neuen Werte genutzt
         clear_expired_basictimer(timer);
         while (!isexpired_basictimer(timer)) ;
         s_main.time2 = value_systick();
         stop_systick();
         stop_basictimer(timer);
         s_main.time2 = s_main.time1 - s_main.time2;
         s_main.time1 = 0xffffff - s_main.time1;
         assert(980 < s_main.time1 && s_main.time1 <= 1005);   // Prüfe, ob durch config gesetzte Werte genutzt wurden
         assert(6980 < s_main.time2 && s_main.time2 <= 7005);  // Prüfe, ob beim zweiten DUrchlauf neue Werte genutzt wurden

         // Teste update_basictimer: Werte werden nach start_basictimer verwendet
         switch_used_led();
         config_systick(0xffffff, systickcfg_CORECLOCK);
         assert(config_basictimer(timer, 10, 100, basictimercfg_REPEAT) == 0);
         update_basictimer(timer, 7, 1000);
         start_basictimer(timer);                  // hier werden die zuletzt gesetzten Werte geladen
         start_systick();
         while (!isexpired_basictimer(timer)) ;
         s_main.time1 = value_systick();
         clear_expired_basictimer(timer);
         while (!isexpired_basictimer(timer)) ;
         s_main.time2 = value_systick();
         stop_systick();
         stop_basictimer(timer);
         s_main.time2 = s_main.time1 - s_main.time2;
         s_main.time1 = 0xffffff - s_main.time1;
         assert(6980 < s_main.time1 && s_main.time1 <= 7005); // Prüfe, ob neue Werte genutzt wurden
         assert(6980 < s_main.time2 && s_main.time2 <= 7005);

         // Teste update_basictimer: Werte werden nach reset_basictimer verwendet
         switch_used_led();
         for (int i = 0; i < 2; ++i) {
            config_systick(0xffffff, systickcfg_CORECLOCK);
            assert(config_basictimer(timer, 10, 100, basictimercfg_REPEAT) == 0);
            start_basictimer(timer);
            update_basictimer(timer, 7, 1000);
            if (i) {
               resetandexpire_basictimer(timer);   // hier werden neue Werte geladen
               clear_expired_basictimer(timer);  // Expirationflag wurde auch gesetzt, also löschen
                                                   // Dieser befehl verbraucht etwas Zeit, daher muss
                                                   // die Testgrenze von time1 angepasst werden.
            } else {
               reset_basictimer(timer);            // hier werden neue Werte geladen
            }
            start_systick();
            while (!isexpired_basictimer(timer)) ;
            s_main.time1 = value_systick();
            clear_expired_basictimer(timer);
            while (!isexpired_basictimer(timer)) ;
            s_main.time2 = value_systick();
            stop_systick();
            stop_basictimer(timer);
            s_main.time2 = s_main.time1 - s_main.time2;
            s_main.time1 = 0xffffff - s_main.time1;
            assert(6950 < s_main.time1 && s_main.time1 <= 7005);  // Prüfe, ob neue Werte genutzt wurden
            assert(6980 < s_main.time2 && s_main.time2 <= 7005);
         }

         // Teste Interruptroutine wird aufgerufen auch bei gestopptem Timer
         switch_used_led();
         // schalte Interrupt ein und Timer stoppt sich nach Ablauf selbst
         assert(config_basictimer(timer, 55555, 1, basictimercfg_INTERRUPT) == 0);
         clear_interrupt(interrupt_TIMER6_DAC);        // Lösche mögliches pending Interruptflag Timer6
         clear_interrupt(interrupt_TIMER7);            // Lösche mögliches pending Interruptflag Timer7
         enable_interrupt(interrupt_TIMER6_DAC);       // Schalte NVIC Interrupt für Timer6 ein
         enable_interrupt(interrupt_TIMER7);           // Schalte NVIC Interrupt für Timer7 ein
         assert(s_counter6 == 0);                           // Zähler Timer6 ist 0
         assert(s_counter7 == 0);                           // Zähler Timer7 ist 0
         resetandexpire_basictimer(timer);                  // Löst manuell (Timer abgeschaltet) einen Interrupt aus
         assert(! isstarted_basictimer(timer));             // Timer ist immer noch abgeschaltet
         assert(! isexpired_basictimer(timer));             // Abgelaufen-Flag gelöscht von Interruptroutine
         assert(s_counter6 == is6);                         // Nur der s_counter des zurückgesetzten Timers wurde erhöht
         assert(s_counter7 == is7);                         // dito
         disable_interrupt(interrupt_TIMER6_DAC);      // Schalte NVIC Interruptbehandlung aus für Timer6
         disable_interrupt(interrupt_TIMER7);          // Schalte NVIC Interruptbehandlung aus für Timer7
         assert(is_interrupt(interrupt_TIMER6_DAC) == 0);  // kein weiterer Interrupt an NVIC gemeldet
         assert(is_interrupt(interrupt_TIMER7) == 0);      // kein weiterer Interrupt an NVIC gemeldet
         s_counter6 = 0;
         s_counter7 = 0;

      }
   }

}
