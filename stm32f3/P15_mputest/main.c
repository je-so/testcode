#include "konfig.h"

volatile uint32_t faultcount = 0;
volatile uint32_t cpustate[10];
volatile const char *filename;
volatile int         linenr;

void assert_failed_exception(const char *_filename, int _linenr)
{
   filename = _filename;
   linenr   = _linenr;
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write_gpio(GPIOE, GPIO_PIN9|GPIO_PIN13, GPIO_PINS(15,8)-GPIO_PIN9-GPIO_PIN13);
      for (volatile int i = 0; i < 80000; ++i) ;
      write1_gpio(GPIOE, GPIO_PINS(15,8));
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

static void switch_unprivileged(void)
{
   __asm( "mrs r0, CONTROL\n"
          "orrs r0, #1\n"
          "msr CONTROL, r0\n" ::: "cc", "r0" );
}

static void switch_privileged(void)
{
   __asm( "mrs r0, CONTROL\n"
          "bics r0, #1\n"
          "msr CONTROL, r0" ::: "cc", "r0" );
}

static void turn_on_led(uint8_t nrled)
{
   uint32_t led = 1u << (8+(nrled&0x7));
   write_gpio(GPIOE, led/*on*/, GPIO_PINS(15,8)&~led/*off*/);
}

void nmi_interrupt(void)
{
   uint8_t led = 0;
   while (1) {
      turn_on_led(led++);
      for (volatile int i = 0; i < 100000; ++i) ;
   }
}

// Wird nicht benutzt, könnte aber mittels enable_coreinterrupt(coreinterrupt_MPUFAULT) eingeschaltet werden.
void mpufault_interrupt(void)
{
   uint8_t led = 0;
   while (1) {
      turn_on_led(led++);
      for (volatile int i = 0; i < 100000; ++i) ;
   }
}

// Wird nicht benutzt, könnte aber mittels enable_coreinterrupt(coreinterrupt_BUSFAULT) eingeschaltet werden.
void busfault_interrupt(void)
{
   uint8_t led = 0;
   while (1) {
      turn_on_led(led--);
      for (volatile int i = 0; i < 100000; ++i) ;
   }
}

void fault_interrupt(void)
{
   ++ faultcount;
   // visuelles Feedback alle LED an, dann wieder aus
   write1_gpio(GPIOE, GPIO_PINS(15,8));
   for (volatile int i = 0; i < 100000; ++i) ;
   write0_gpio(GPIOE, GPIO_PINS(15,8));

   // Schalte zurück in privlegierten Superuser (Supervisor/Kernel/...) Modus
   switch_privileged();

   if (faultcount == 3) {
      // lädt SP (Stackpointer) von cpustate[0]
      // und pusht die geretteten Register (r0-r3,r12,lr,pc,PSR) in cpustate[1..8] auf den Stack
      // lr wird auf 0xfffffff9 gesetzt (return from interrupt),
      // d.h. beim Ausführen von "bx lr" wird aus dem Interrupt zurückgekehrt und
      // die Register r0-r3,r12,lr,pc,psr vom Stack gelesen und an der Adresse in pc
      // mit der Ausführung im Thread-Mode fortgefahren.
      // stm = stmia, ldm = ldmia, (ia = increment after)
      __asm(   "ldr r0, =cpustate\n"
               "ldr r1, [r0], #4\n"
               "ldr r7, [r0], #4\n"
               "sub r1, #8*4\n"
               "mov sp, r1\n"
               "ldm r0!, {r1-r3,r12}\n" // copy regs {r0-r3}
               "stm sp, {r1-r3,r12}\n"  // to stack
               "ldm r0, {r1-r3,r12}\n"  // copy regs {r12,lr,pc,psr}
               "add r0, sp, #4*4\n"     // r0 points to sp + #4*4
               "stm r0, {r1-r3,r12}\n"  // to stack
               "mov lr, #0xfffffff9\n"  // ret from interrupt
               "bx  lr\n"
            );
   } else if (faultcount == 6) {
      clear_mpu(3, 1);
   }
}

/*
   Das Programm schaltet in den unprivilegierten Modus und
   beim Zugriff auf vor dem unprivilegierten Zugriff geschützte
   Speicherbereiche wird ein fault_interrupt ausgelöst (wird auch HardFault genannt).

   Die MPU wird benutzt, um den Zugriff auf nur bestimmte Speicherbereiche
   im unprivilegierten Modus einzuschränken.
   Der Zugriff auf privilegiert Bereiche löst eine Exception aus und das
   Programm testet, ob dies auch tatsächlich so ist.


   Allgemeine Erklärungen:

   Das Program Status Register (PSR) kombiniert:

    * Application Program Status Register (APSR)
    * Interrupt Program Status Register (IPSR)
    * Execution Program Status Register (EPSR).

   Diese Register belegen nicht überlappende Bitfelder innerhalb von PSR. Die Belegung sieht so aus:
       |<-              -- PSR  --                  ->|
       ┌─┬─┬─┬─┬─┬────────────────────────────────────┐
   APSR│N│Z│C│V│Q│              reserviert            │
       ├─┴─┴─┴─┴─┴─────────────────────────┬──────────┤
   IPSR│         reserviert                │  ISR-Nr  │
       ├─────────┬──────┬─┬───────┬──────┬─┴──────────┤
   EPSR│ res.    │ICI/IT│T│  res. │ICI/IT│ reserviert │
       └─────────┴──────┴─┴───────┴──────┴────────────┘
   Bits [31] N	Negative Flag
   Bits [30] Z	Zero Flag
   Bits [29] C	Carry or Borrow Flag
   Bits [28] V	Overflow Flag
   Bits [27] Q	Saturation Flag
   Bits [26:25], [15:10] ICI/IT Entweder Position innerhalb unterbrochener Instruktion (LDM, STM, PUSH, or POP)
                         bzw. Zustand einer IT Instruktion (bedingte Ausführung von Instruktionen)
   Bits [24] T	Thumb State. Diese Bit ist immer 1, da Cortex-M4 nur Thumb-1 und Thumb-2 Instruktionen unterstützt.
   Bits [8:0]	ISR_NUMBER (0 = Thread-Mode, 2 = NMI-Interrupt, 3 = (Hard)Fault-Interrupt, ...)

   Der Versuch, das EPSR Register (bzw. Bitfelder in PSR) mittels MRS-Instruktion in Software zu lesen
   resultiert immer im Wert 0. Schreibversuche mittels MSR-Instrukion werden ignoriert.

   Interrupt

   Wird ein Interrupt ausgeführt (ohne FPU), dann legt der Prozessor
   die 8 Register R0-R3,R12,LR,PC und PSP auf den aktuellen Stack (PSP oder MSP),
   wobei [sp] = R0, [sp, #4] = R1 ... und [sp, #7*4] = PSP.
   Der Interrupt-Vektor wird aus der Vektortabelle in PC geladen, die ISR-Nr. wird in IPSR gesetzt,
   das LR-Register wird mit speziellen Wert 0xfffffff9 (MSP) geladen, bzw. 0xfffffffd (PSP), wenn
   der Prozessor aus dem Thread-Mode (ohne FPU) unterbrochen wird.

   Ein normaler Returnbefehl "bx lr" am Ende der Interruptroutine lässt den Prozessor wieder
   zurückkehren an die vormals unterbrochene Stelle.

   Da der Aufruf-Standard von ARM vorsieht, dass in einer Funktion R0-R3,R12 und APSR verändert werden
   dürfen, kann ein Interrupt-Handler als normale C-Funktion geschrieben werden.

*/
int main(void)
{
   enable_gpio_clockcntrl(GPIOA_BIT/*switch*/|GPIOE_BIT/*led*/);
   config_input_gpio(GPIOA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIOE, GPIO_PINS(15,8));

   // teste Funktionswerte
   assert( 1 == isavailable_mpu());
   assert( 8 == nrregions_mpu());

   // blaue/blue LED
   turn_on_led(0);
   for (volatile int i = 0; i < 100000; ++i) ;

   switch_unprivileged();

   turn_on_led(0); // Peripherie erlaubt unprivilegierten Zugriff per Default
   assert( 0 ==  faultcount);
   // Der Zugriff auf PPB aber ist privilegiert ==> generiert fault_interrupt (da busfault_interrupt nicht eingeschaltet!)
   assert( 1 == isavailable_mpu());
   // fault_interrupt schaltet zurück in privilegierten Modus
   assert( 1 == faultcount);

   turn_on_led(0);
   for (volatile int i = 0; i < 100000; ++i) ;

   // aktiviere MPU
   mpu_region_t regs[2] = {
      // Erlaube Zugriff auf Flash Memory
      mpu_region_INITROM(0, mpu_size_256K, 0, mpu_access_READ),
      // Erlaube Zugriff auf SRAM Memory
      mpu_region_INITSRAM(mpu_size_64K, mpu_access_RW),
   };
   assert( 0 == config_mpu(2, regs, mpucfg_NONE));
   assert( 0 == isenabled_mpu());
   // Schalte MPU ein und erlaube privilegierten Zugriff auf alle nicht definierten Regionen
   enable_mpu(mpucfg_ALLOWPRIVACCESS);
   assert( 1 == isenabled_mpu());

   switch_unprivileged();
   // kein Fehler nach Umschalten auf unprivilegierten Modus
   assert( 1 == faultcount);
   // generiert MPU fault da Peripherie nicht als MPU-Region definiert
   // ==> fault_interrupt bei Zugriff auf GPIO (da mpufault_interrupt nicht eingeschaltet!)
   turn_on_led(0); // aber fault_interrupt schaltet zurück in privilegierten Modus, darum geht es weiter
   for (volatile int i = 0; i < 100000; ++i) ;
   assert( 2 == faultcount);

   // Verbiete unprivilegierten Zugriff auf STACK
   assert( 2 == nextfreeregion_mpu(0));
   assert( 2 == nextfreeregion_mpu(1));
   assert( 2 == nextfreeregion_mpu(2));
   mpu_region_t conf2 = mpu_region_INITSRAM(mpu_size_512, mpu_access_NONE);
   update_mpu(nextfreeregion_mpu(0), 1, &conf2);
   assert( 3 == nextfreeregion_mpu(0));

   // Sichere aktuellen Zustand der CPU in cpustate (Interrupt-kompatibel).
   // Warum? Da der Stack nicht beschreibbar oder lesbar ist (MPU-Schutz)
   // wird ein fault_interrupt ausgelöst, der aber die Register nicht auf den
   // Stack retten kann. Also nutzt in diesem Fall der fault_interrupt die
   // in cpustate geretteten Werte, um die Ausführung an der Stelle "next_pc: "
   // fortzuführen.
   assert( 2 == faultcount);
   __asm(   "ldr r0, =cpustate\n"
            "str sp, [r0], #4\n"
            "str r7, [r0], #4\n" // wird von gcc benutzt
            "stm r0, {r0-r3,r12,lr}\n"
            "add r0, #6*4\n"
            "adr r1, next_pc\n"
            "str r1, [r0], #4\n"
            "mrs r1, psr\n"
            // Thumb State Bit muss manuell gesetzt werden, da Cortex-M4 immer ausschließlich Thumb unterstützt.
            // Warum ? Da Lesezugriffe auf PSR für den EPSR-Anteil (enthält Thumb-Bit) immer 0 zurückliefern.
            "orr r1, #(1<<24)\n"
            "str r1, [r0]\n"
            "next_pc: \n"  // an diese Stelle kehrt der fault_interrupt zurück.
            ::: "r0", "r1" );
   if (faultcount == 2) { // falls noch kein fault_interrupt
      // generiert mpu fault
      switch_unprivileged();
      // never reached
      while (1);
   }
   assert( 3 == faultcount);

   turn_on_led(0);
   for (volatile int i = 0; i < 100000; ++i) ;

   // Erlaube unprivilegierten Zugriff auf PPB (geht nicht, da MPU nur Rechte wegnehmen und keine hinzufügen kann!)
   conf2 = (mpu_region_t) mpu_region_INIT(0xE0000000, mpu_size_1MB, 0, mpu_mem_ORDERED, mpu_access_RW, mpu_access_RW);
   update_mpu(2, 1, &conf2);
   switch_unprivileged();
   // Der Zugriff ist privilegiert und bleibt es auch ==> busfault !
   assert( 3 == faultcount);
   assert( 1 == isavailable_mpu());
   assert( 4 == faultcount);

   turn_on_led(0);
   for (volatile int i = 0; i < 100000; ++i) ;

   // Verbiete unprivilegierten Schreibzugriff auf SRAM
   assert( 0x20000000+512 <= (uint32_t)cpustate && (uint32_t)cpustate < 0x20000000+1024);
   conf2 = (mpu_region_t) mpu_region_INIT(0x20000000+512, mpu_size_512, 0, mpu_mem_NORMAL(mpu_cache_WB), mpu_access_RW, mpu_access_READ);
   update_mpu(2, 1, &conf2);
   cpustate[0] = 0;  // OK
   switch_unprivileged();
   assert( 4 == faultcount);
   cpustate[0] = 1; // MPU Fault
   assert( 5 == faultcount);

   turn_on_led(0);
   for (volatile int i = 0; i < 100000; ++i) ;

   // Verbiete privilegierten Schreibzugriff auf SRAM
   assert(0x20000000+512 <= (uint32_t)cpustate && (uint32_t)cpustate < 0x20000000+1024);
   conf2 = (mpu_region_t) mpu_region_INIT(0x20000000+512, mpu_size_512, 0, mpu_mem_NORMAL(mpu_cache_WB), mpu_access_READ, mpu_access_READ);
   update_mpu(3, 1, &conf2);
   assert( 5 == faultcount);
   cpustate[0] = 1; // MPU Fault
   assert( 6 == faultcount);

   // OK: 2 grüne/green LEDs
   write_gpio(GPIOE, GPIO_PIN11|GPIO_PIN15, GPIO_PINS(14,8)-GPIO_PIN11);
   while (1) ;
}
