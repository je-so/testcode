#include "konfig.h"
#include "µC/cpustate.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS
#define LED_MAXPIN      GPIO_PIN(HW_KONFIG_USER_LED_MAXNR)
#define LED_MINPIN      GPIO_PIN(HW_KONFIG_USER_LED_MINNR)

/* set by assert_failed_exception */
volatile const char *filename;
volatile int         linenr;
cpustate_t           cpustate;

void assert_failed_exception(const char *_filename, int _linenr)
{
   filename = _filename;
   linenr   = _linenr;
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
      write_gpio(LED_PORT, LED_MAXPIN, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void switch_led(void)
{
   static uint32_t lednr1 = 0;
   static uint32_t lednr2 = 0;
   static uint32_t counter1 = 0;
   static uint32_t counter2 = 0;
   uint16_t off = GPIO_PIN(8 + lednr2) | GPIO_PIN(8 + lednr1);
   static_assert(LED_PINS == GPIO_PINS(15,8));
   counter1 = (counter1 + 1) % 2;
   counter2 = (counter2 + 1) % 3;
   lednr1 = (lednr1 + (counter1 == 0)) % 8;
   lednr2 = (lednr2 + (counter2 == 0)) % 8;
   write_gpio(LED_PORT, GPIO_PIN(8 + lednr1) | GPIO_PIN(8 + lednr2), off);
   if (getHZ_clockcntrl() > 8000000) {
      for (volatile int i = 0; i < 100000; ++i) ;
   } else {
      for (volatile int i = 0; i < 20000; ++i) ;
   }
}

void busfault_interrupt(void)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void fault_interrupt(void)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void nmi_interrupt(void)
{
   setsysclock_clockcntrl(clock_INTERNAL);
   while (1) {
      write1_gpio(LED_PORT, LED_PINS&~(LED_MINPIN|LED_MAXPIN));
      for (volatile int i = 0; i < 80000; ++i) ;
      write0_gpio(LED_PORT, LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
   }
}

void pendsv_interrupt(void)
{
   //
}

typedef struct task_t {
   uint32_t       regs[8]; // r4..r11
   uint32_t      *sp;
   struct task_t *next;
   uint32_t       stack[256-10];
} task_t;

uint32_t s_count;
uint32_t s_task_nr[3];
task_t   task[3];
task_t  *current_task;

__attribute__ ((naked))
void systick_interrupt(void)
{
   __asm volatile(
      "movw r0, :lower16:current_task\n"  // r0 = (uint16_t) &current_task;
      "movt r0, :upper16:current_task\n"  // r0 |= &current_task << 16;
      "ldr  r1, [r0]\n"          // r1 = current_task
      "mrs  r12, psp\n"          // r12 = psp
      "stm  r1!, {r4-r12}\n"     // save r4..r11 into current_task->regs, current_task->sp = r12
      "ldr  r1, [r1]\n"          // r1 = current_task->next;
      "str  r1, [r0]\n"          // current_task = r1;
      "ldm  r1!, {r4-r12}\n"     // restore r4..r11 from current_task->regs, r12 = current_task->sp;
      "msr  psp, r12\n"          // psp = current_task->sp;
      "bx   lr\n"                // return from interrupt
   );
}

static void task_main(uint32_t id/*0..2*/)
{
   uint32_t const P = HW_KONFIG_USER_LED_MINNR;
   while (1) {
      s_task_nr[id] = (s_task_nr[id] + 1) % 8;
      write_gpio(LED_PORT, GPIO_PIN(P+s_task_nr[0])|GPIO_PIN(P+s_task_nr[1])|GPIO_PIN(P+s_task_nr[2]), LED_PINS);
      for (volatile int i = 0; i < 80000; ++i) ;
      __asm volatile(
         "1: ldrex   r0, [%0]\n"
         "add     r0, #1\n"
         "strex   r1, r0, [%0]\n"
         "tst     r1, r1\n"
         "bne     1b\n"    // repeat if increment interrupted and failed
         :: "r" (&s_count) : "memory", "r0", "r1"
      );
      if (s_count >= 30) {
         stop_systick();
         __asm volatile(
            "mrs  r0, control\n"    // r0 = control
            "bics r0, #(1<<1)\n"    // clear bit 1 in r0
            "msr  control, r0\n"    // switch cpu to using msp as stack pointer
            ::: "r0"
         );
         jump_cpustate(&cpustate);
      }
   }
}

/*
 * Dieses Programm startet 3 Tasks.
 *
 * Die Hauptfunktion eines tasks ist task_main.
 * Jeder Task bekommt seine ID als Parameter übergeben.
 *
 * Eine Task läuft 333ms bevor auf die nächste gewechselt wird.
 * Jede Task lässt ihre eigene LED wandern.
 * Der Wechsle von einer Task in die andere wird mit dem Systickinterrupt bewerkstelligt.
 *
 * Nach einer bestimmten Anzahl Schritten (siehe s_count) wird wieder nach main
 * zurückgesprungen und zwei LEDs bwegen sich blinkend im Kreis.
 *
 * Im Fehlerfall beginnen alle LEDs zu blinken.
 *
 */
int main(void)
{
   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);

   for (volatile int i = 0; i < 125000; ++i) ;

   static_assert(sizeof(task_t) == 1024);

   current_task = &task[0];
   for (uint32_t i = 0; i < lengthof(task); ++i) {
      task[i].sp   = &task[i].stack[lengthof(task[i].stack)-8];
      task[i].next = &task[(i+1)%lengthof(task)];
      task[i].sp[0/*r0*/]  = i;  // set id parameter
      task[i].sp[5/*lr*/]  = 0xffffffff;  // invalid LR value
      task[i].sp[6/*pc*/]  = (uintptr_t) &task_main; // start address of task
      task[i].sp[7/*PSR*/] = (1 << 24); // set thumb bit
   }

   int err = init_cpustate(&cpustate);
   if (err == 0) {
      setpriority_coreinterrupt(coreinterrupt_SYSTICK, interrupt_priority_LOW);
      config_systick(8000000/3/*8MHZ clock ==> 333ms*/, systickcfg_CORECLOCK|systickcfg_INTERRUPT|systickcfg_START);
      __asm volatile(
         "msr  psp, %0\n"        // psp = task[0].sp + 8
         "mrs  r0, control\n"    // r0 = control
         "orrs r0, #(1<<1)\n"    // set bit 1 in r0
         "msr  control, r0\n"    // switch cpu to using psp as stack pointer
         :: "r" (task[0].sp + 8) : "r0"
      );
      task_main(0);
   }
   assert(EINTR == err);

   while (1) switch_led();

}

