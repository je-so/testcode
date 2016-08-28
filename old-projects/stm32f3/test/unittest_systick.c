
static volatile uint32_t s_cyclecount;
static volatile int      s_systick_counter;
static void  (* volatile s_systick_fct) (void);

void systick_interrupt(void)
{
   ++ s_systick_counter;
   if (s_systick_fct) {
      s_systick_fct();
   }
}

static void wait_systick_interrupt(void)
{
   for (volatile int i = 0; i < 100; ++i) ;

   // next interrupt occurred during this one
   stop_systick();
   assert(is_coreinterrupt(coreinterrupt_SYSTICK));
   clear_coreinterrupt(coreinterrupt_SYSTICK);
}

static void setperiod_systick_interrupt(void)
{
   setperiod_systick(10000);
}

static void isexpired_systick_interrupt(void)
{
   s_systick_counter = isexpired_systick();
   stop_systick();
}

static void wait_buscycles(uint32_t cycles)
{
   start_dwtdbg(dwtdbg_CYCLECOUNT);
   while (cyclecount_dwtdbg() < cycles) __asm volatile ("nop");
   stop_dwtdbg(dwtdbg_CYCLECOUNT);
}

int unittest_systick(void)
{
   // TEST setperiod_systick: EINVAL
   assert( EINVAL == setperiod_systick(0));
   assert( EINVAL == setperiod_systick(1));
   assert( 0 == setperiod_systick(2));
   assert( 0 == setperiod_systick(0x01000000));
   assert( EINVAL == setperiod_systick(0x01000000+1));

   // TEST setperiod_systick
   for (uint32_t p = 2; p <= 0x1000000; p <<= 1) {
      assert( 0 == setperiod_systick(p));
      assert( p == period_systick());
   }

   // TEST isstarted_systick
   config_systick(10000, systickcfg_CORECLOCK);
   assert( 0 == isstarted_systick());
   config_systick(10000, systickcfg_CORECLOCK|systickcfg_START);
   assert( 1 == isstarted_systick());
   config_systick(10000, systickcfg_CORECLOCK);
   start_systick();
   assert( 1 == isstarted_systick());
   stop_systick();
   assert( 0 == isstarted_systick());
   continue_systick();
   assert( 1 == isstarted_systick());
   stop_systick();
   assert( 0 == isstarted_systick());

   // TEST start_systick: resets value
   config_systick(10000, systickcfg_CORECLOCK|systickcfg_START);
   while (! value_systick()) __asm volatile("nop");
   while (value_systick() > 5000) __asm volatile("nop");
   stop_systick();
   assert( 5000 >= value_systick());
   assert( 2000 <= value_systick());
   start_systick();
   s_systick_counter = value_systick();
   assert (9990 <= s_systick_counter);
   stop_systick();

   // TEST continue_systick: value not changed
   config_systick(10000, systickcfg_CORECLOCK|systickcfg_START);
   while (! value_systick()) __asm volatile("nop");
   while (value_systick() > 9000) __asm volatile("nop");
   stop_systick();
   s_systick_counter = value_systick();
   continue_systick();
   assert( s_systick_counter >= value_systick());
   assert( s_systick_counter <= value_systick()+100);
   stop_systick();

   // TEST isenabled_interrupt_systick
   config_systick(10000, systickcfg_CORECLOCK);
   assert( 0 == isenabled_interrupt_systick());
   config_systick(10000, systickcfg_CORECLOCK|systickcfg_INTERRUPT);
   assert( 1 == isenabled_interrupt_systick());
   disable_interrupt_systick();
   assert( 0 == isenabled_interrupt_systick());
   enable_interrupt_systick();
   assert( 1 == isenabled_interrupt_systick());

   // TEST isexpired_systick: reading clears flag
   config_systick(1000, systickcfg_CORECLOCK|systickcfg_START);
   assert( 0 == isexpired_systick());
   for (int r = 0; r < 10; ++r) {
      wait_buscycles(1000);
      assert( 1 == isexpired_systick());  // Timer abgelaufen
      assert( 0 == isexpired_systick());  // Lesen hat Flag gelöscht
   }

   // TEST isexpired_systick: stop clears flag
   config_systick(1000, systickcfg_CORECLOCK|systickcfg_START);
   wait_buscycles(1000);
   stop_systick();
   assert( 0 == isexpired_systick());  // Stop löscht Flag

   // TEST isexpired_systick: config clears flag
   config_systick(1000, systickcfg_CORECLOCK|systickcfg_START);
   wait_buscycles(1000);
   config_systick(1000, systickcfg_CORECLOCK|systickcfg_START);
   assert( 0 == isexpired_systick());  // Config löscht Flag

   // TEST value_systick
   config_systick(1000, systickcfg_CORECLOCKDIV8|systickcfg_START);
   start_dwtdbg(dwtdbg_CYCLECOUNT);
   for (uint32_t cc, v; (cc=cyclecount_dwtdbg()) < 7000; ) {
      v = value_systick();
      cc /= 8;
      assert( 999-cc >= v);
      assert( 999-cc <= v+5);
   }
   // reset
   stop_systick();
   stop_dwtdbg(dwtdbg_CYCLECOUNT);

   // TEST systick_interrupt: Periode ändert sich erst ab nächstem Interrupt
   s_systick_counter = 0;
   s_systick_fct = &setperiod_systick_interrupt;
   config_systick(1000, systickcfg_CORECLOCK|systickcfg_START|systickcfg_INTERRUPT);
   start_dwtdbg(dwtdbg_CYCLECOUNT);
   while (cyclecount_dwtdbg() < 1010) __asm volatile ("nop");
   assert( 1 == s_systick_counter);    // Periode wurde geändert auf 10000, aber alte Periode 1000 weiter aktiv
   while (cyclecount_dwtdbg() < 2010) __asm volatile ("nop");
   assert( 2 == s_systick_counter);    // Jetzt wird neue Periode verwendet
   (void) isexpired_systick();         // Lösche Flag
   while (cyclecount_dwtdbg() < 3010) __asm volatile ("nop");
   assert( 2 == s_systick_counter);    // Nach 1000 Takten wurde Interrupt noch nicht ausgelöst
   while (!isexpired_systick()) __asm volatile ("nop");
   stop_dwtdbg(dwtdbg_CYCLECOUNT);
   assert( 10000 <= cyclecount_dwtdbg());
   assert( 13000 >= cyclecount_dwtdbg());
   // reset
   stop_systick();

   // TEST systick_interrupt: während Interrupt läuft, wird ein weiterer systick Interruptimpuls ausgelöst
   s_systick_counter = 0;
   s_systick_fct = &wait_systick_interrupt;
   config_systick(100, systickcfg_CORECLOCK|systickcfg_START|systickcfg_INTERRUPT);
   while (isstarted_systick()) ;
   assert( 1 == s_systick_counter);

   // TEST systick_interrupt: isexpired kann während interrupt gesetzt sein oder auch nicht
   s_systick_fct = &isexpired_systick_interrupt;
   config_systick(1000, systickcfg_CORECLOCK|systickcfg_START|systickcfg_INTERRUPT);
   while (isstarted_systick()) ;
   assert( 1 >= s_systick_counter); // isexpired kann gesetzt sein oder auch nicht

   // reset
   s_systick_counter = 0;
   s_systick_fct = 0;
   stop_systick();

   return 0;
}
