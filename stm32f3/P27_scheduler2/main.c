#include "konfig.h"
#include "task.h"
#include "sched.h"
#include "semaphore.h"
#include "fifo.h"

#define SWITCH_PORT     HW_KONFIG_USER_SWITCH_PORT
#define SWITCH_PORT_BIT HW_KONFIG_USER_SWITCH_PORT_BIT
#define SWITCH_PIN      HW_KONFIG_USER_SWITCH_PIN
#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS
#define LED_MAXPIN      GPIO_PIN(HW_KONFIG_USER_LED_MAXNR)
#define LED_MINPIN      GPIO_PIN(HW_KONFIG_USER_LED_MINNR)

/* used by scheduler */
task_t         task[3];

/* used in tasks */
semaphore_t    sem1;
fifo_t         fifo1;
uint32_t       fifo1_buffer[5];

/* set by assert_failed_exception */
volatile const char *filename;
volatile int         linenr;

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
      for (volatile int i = 0; i < 140000; ++i) ;
   } else {
      for (volatile int i = 0; i < 20000; ++i) ;
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

void systick_interrupt(void)
{
   static uint32_t count;
   periodic_sched(1);
   if (++count == 10) {
      count = 0;
      trigger_sched();
   }
}

static void task_main(uintptr_t id/*0..2*/)
{
   static uint32_t s_count;
   static uint32_t s_task_nr[3];

   if (id == 0) {
      assert(s_count == 0);
      get_fifo(&fifo1);
      assert(s_count > 0);
   }
   if (id == 1) {
      assert(s_count == 0);
      wait_semaphore(&sem1);
      assert(s_count > 0);
   }
   if (id == 2) {
      assert(s_count == 0);
      s_count = 1;
      put_fifo(&fifo1, 0);
      signal_semaphore(&sem1);
   }

   sleepms_task(id*330);

   while (1) {
      const uint32_t P = HW_KONFIG_USER_LED_MINNR;
      for (int i = 0; i < 3; ++i) {
         s_task_nr[id] = (s_task_nr[id] + 1) % 8;
         write_gpio(LED_PORT, GPIO_PIN(P+s_task_nr[0])|GPIO_PIN(P+s_task_nr[1])|GPIO_PIN(P+s_task_nr[2]), LED_PINS);
         increment_atomic(&s_count);
         sleepms_task(110);
      }
      if (s_count >= 30 && id == 0/*main thread*/) {
         stop_systick();
         return;
      }
      sleepms_task(2*330);
   }

}

// task[0] is main thread
void* getmainpsp_startup(void)
{
   return &task[0].stack[lengthof(task[0].stack)];
}

/*
 * Dieses Programm startet 3 Tasks.
 *
 * Die Hauptfunktion eines tasks ist task_main.
 * Jeder Task bekommt seine ID als Parameter übergeben.
 *
 * Eine Task läuft maximal 333ms bevor auf die nächste gewechselt wird.
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
   int err;
   enable_gpio_clockcntrl(SWITCH_PORT_BIT|LED_PORT_BIT);
   config_input_gpio(SWITCH_PORT, SWITCH_PIN, GPIO_PULL_OFF);
   config_output_gpio(LED_PORT, LED_PINS);

   // TEST main is running on psp and not msp
   __asm volatile(
      "mrs  %0, psp\n"     // err = psp
      "mov  r1, sp\n"      // r1 = sp
      "subs %0, r1\n"      // err = psp - sp ;  (err == 0) in case psp == sp
      : "=r" (err) :: "r1"
   );
   assert(err == 0);

   for (volatile int i = 0; i < 125000; ++i) ;

   setsysclock_clockcntrl(clock_PLL/*72MHZ*/);
   assert(getHZ_clockcntrl() == 72000000);

   setprioritymask_interrupt(interrupt_priority_MIN);   // prevent scheduler interrupt to be activated
   init_task(&task[0], 0, 0); // init main task == caller
   init_sched(0, 1, task);    // set current task to main task
   assert(&task[0] == current_task());

   // TEST semaphore_INIT
   for (uint32_t i = 1; i <= 100; ++i) {
      semaphore_t sem = semaphore_INIT(i);
      assert(i == value_semaphore(&sem));
      assert(0 == sem.taskwait.nrevent);
      assert(0 == sem.taskwait.last);
   }

   // TEST signal_semaphore: value < 0
   for (int32_t i = -100; i <= 0; ++i) {
      semaphore_t sem = semaphore_INIT(i-1);
      signal_semaphore(&sem);
      assert(i == value_semaphore(&sem));
      assert(1 == sem.taskwait.nrevent);
      assert(0 == sem.taskwait.last);
      for (int32_t i2 = i+1; i2 <= 0; ++i2) {
         signal_semaphore(&sem);
         assert(i2     == value_semaphore(&sem));
         assert(i2+1-i == sem.taskwait.nrevent);
         assert(0 == sem.taskwait.last);
      }
   }

   // TEST signal_semaphore: value >= 0
   for (int32_t i = 1; i <= 100; ++i) {
      semaphore_t sem = semaphore_INIT(i-1);
      signal_semaphore(&sem);
      assert(i == value_semaphore(&sem));
      assert(0 == sem.taskwait.nrevent);
      assert(0 == sem.taskwait.last);
   }

   // TEST wait_semaphore: value > 0
   for (int32_t i = 99; i >= 0; --i) {
      semaphore_t sem = semaphore_INIT(i+1);
      wait_semaphore(&sem);
      assert(i == value_semaphore(&sem));
      assert(0 == sem.taskwait.nrevent);
      assert(0 == sem.taskwait.last);
   }

   // TEST wait_semaphore: value <= 0
   for (int32_t i = -1; i > -100; --i) {
      semaphore_t sem = semaphore_INIT(i+1);
      assert(0 == is_coreinterrupt(coreinterrupt_PENDSV));
      assert(0 == task[0].state);
      wait_semaphore(&sem);
      assert(i == value_semaphore(&sem));
      assert(0 == sem.taskwait.nrevent);
      assert(0 == sem.taskwait.last);
      assert(0 != is_coreinterrupt(coreinterrupt_PENDSV));  // scheduler triggered
      assert(1 == task[0].state);                           // task blocked
      // reset
      task[0].state = 0;
      clear_coreinterrupt(coreinterrupt_PENDSV);
   }

   // TEST fifo_INIT
   for (uint32_t i = 0; i < 100; ++i) {
      uint32_t   buffer[1];
      fifo_t     fifo = fifo_INIT(i, buffer);
      assert(fifo.sender.value             == i);  // i sender could send
      assert(fifo.sender.taskwait.nrevent  == 0);
      assert(fifo.sender.taskwait.last     == 0);
      assert(fifo.receiver.value           == 0);  // no data in queue nothing could be read
      assert(fifo.receiver.taskwait.nrevent== 0);
      assert(fifo.receiver.taskwait.last   == 0);
      assert(fifo.lock   == 0);
      assert(fifo.buffer == buffer);
      assert(fifo.size   == i);
      assert(fifo.wpos   == 0);
      assert(fifo.rpos   == 0);
   }

   // TEST put_fifo
   for (uint32_t size = 1; size <= 10; ++size) {
      uint32_t   buffer[10];
      fifo_t      fifo = fifo_INIT(size, buffer);
      for (uint32_t i = 0; i < size; ++i) {
         put_fifo(&fifo, 256*size+i);
         assert(fifo.sender.value             == size-1-i);
         assert(fifo.sender.taskwait.nrevent  == 0);
         assert(fifo.sender.taskwait.last     == 0);
         assert(fifo.receiver.value           == i+1);   // queue contains i+1 elements which could be read
         assert(fifo.receiver.taskwait.nrevent== 0);
         assert(fifo.receiver.taskwait.last   == 0);
         assert(fifo.lock   == 0);
         assert(fifo.buffer == buffer);
         assert(fifo.size   == size);
         assert(fifo.wpos   == (i+1)%size);
         assert(fifo.rpos   == 0);
      }
      for (uint32_t i = 0; i < size; ++i) {
         assert(buffer[i] == 256*size+i);
      }
   }

   // TEST get_fifo
   for (uint32_t size = 1; size <= 10; ++size) {
      uint32_t   buffer[10];
      fifo_t     fifo = fifo_INIT(size, buffer);
      for (uint32_t i = 0; i < size; ++i) {
         put_fifo(&fifo, 512*size+i);
      }
      for (uint32_t i = 0; i < size; ++i) {
         assert(512*size+i == get_fifo(&fifo));
         assert(fifo.sender.value             == i+1);   // i+1 unsued entries which could be send
         assert(fifo.sender.taskwait.nrevent  == 0);
         assert(fifo.sender.taskwait.last     == 0);
         assert(fifo.receiver.value           == size-1-i);
         assert(fifo.receiver.taskwait.nrevent== 0);
         assert(fifo.receiver.taskwait.last   == 0);
         assert(fifo.lock   == 0);
         assert(fifo.buffer == buffer);
         assert(fifo.size   == size);
         assert(fifo.wpos   == 0);
         assert(fifo.rpos   == (i+1)%size);
      }
   }

   // TEST tryput_fifo
   for (uint32_t size = 1; size <= 10; ++size) {
      uint32_t   buffer[10];
      fifo_t     fifo = fifo_INIT(size, buffer);
      for (uint32_t i = 0; i < size; ++i) {
         fifo.lock = 1;
         assert(EAGAIN == tryput_fifo(&fifo, 1));
         fifo.lock = 0;
         assert(0 == tryput_fifo(&fifo, 256*size+i));
         assert(fifo.sender.value             == size-1-i);
         assert(fifo.sender.taskwait.nrevent  == 0);
         assert(fifo.sender.taskwait.last     == 0);
         assert(fifo.receiver.value           == i+1);   // queue contains i+1 elements which could be read
         assert(fifo.receiver.taskwait.nrevent== 0);
         assert(fifo.receiver.taskwait.last   == 0);
         assert(fifo.lock   == 0);
         assert(fifo.buffer == buffer);
         assert(fifo.size   == size);
         assert(fifo.wpos   == (i+1)%size);
         assert(fifo.rpos   == 0);
      }
      assert(EAGAIN == tryput_fifo(&fifo, 1));
      assert(fifo.sender.value             == 0);
      assert(fifo.sender.taskwait.nrevent  == 0);
      assert(fifo.sender.taskwait.last     == 0);
      assert(fifo.receiver.value           == size);
      assert(fifo.receiver.taskwait.nrevent== 0);
      assert(fifo.receiver.taskwait.last   == 0);
      assert(fifo.lock   == 0);
      assert(fifo.buffer == buffer);
      assert(fifo.size   == size);
      assert(fifo.wpos   == 0);
      assert(fifo.rpos   == 0);
      for (uint32_t i = 0; i < size; ++i) {
         assert(buffer[i] == 256*size+i);
      }
   }

   // TEST tryget_fifo
   for (uint32_t size = 1; size <= 10; ++size) {
      uint32_t   buffer[10];
      fifo_t     fifo = fifo_INIT(size, buffer);
      for (uint32_t i = 0; i < size; ++i) {
         put_fifo(&fifo, 512*size+i);
      }
      for (uint32_t i = 0; i < size; ++i) {
         uint32_t value = 0;
         fifo.lock = 1;
         assert(EAGAIN == tryget_fifo(&fifo, &value));
         assert(0 == value);
         fifo.lock = 0;
         assert(0 == tryget_fifo(&fifo, &value));
         assert(value == 512*size+i);
         assert(fifo.sender.value             == i+1);   // i+1 unsued entries which could be send
         assert(fifo.sender.taskwait.nrevent  == 0);
         assert(fifo.sender.taskwait.last     == 0);
         assert(fifo.receiver.value           == size-1-i);
         assert(fifo.receiver.taskwait.nrevent== 0);
         assert(fifo.receiver.taskwait.last   == 0);
         assert(fifo.lock   == 0);
         assert(fifo.buffer == buffer);
         assert(fifo.size   == size);
         assert(fifo.wpos   == 0);
         assert(fifo.rpos   == (i+1)%size);
      }
      assert(EAGAIN == tryget_fifo(&fifo, 0));
      assert(fifo.sender.value             == size);
      assert(fifo.sender.taskwait.nrevent  == 0);
      assert(fifo.sender.taskwait.last     == 0);
      assert(fifo.receiver.value           == 0);
      assert(fifo.receiver.taskwait.nrevent== 0);
      assert(fifo.receiver.taskwait.last   == 0);
      assert(fifo.lock   == 0);
      assert(fifo.buffer == buffer);
      assert(fifo.size   == size);
      assert(fifo.wpos   == 0);
      assert(fifo.rpos   == 0);
   }

   for (uint32_t i = 0; i < lengthof(task); ++i) {
      init_task(&task[i], &task_main, i);
   }
   init_sched(0, lengthof(task), task);
   clearprioritymask_interrupt(); // allow scheduler interrupt to be activated

   sem1  = (semaphore_t) semaphore_INIT(0);
   fifo1 = (fifo_t) fifo_INIT(lengthof(fifo1_buffer), fifo1_buffer);

   setpriority_coreinterrupt(coreinterrupt_SYSTICK, interrupt_priority_MIN-1);   // could preempt scheduler
   config_systick(getHZ_clockcntrl()/1000/*1ms*/, systickcfg_CORECLOCK|systickcfg_INTERRUPT|systickcfg_START);
   task_main(0/*call main thread manually so returning is possible*/);

   while (1) switch_led();

}
