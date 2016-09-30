#include "konfig.h"
#include "mailbox.h"

#define LED_PORT        HW_KONFIG_USER_LED_PORT
#define LED_PORT_BIT    HW_KONFIG_USER_LED_PORT_BIT
#define LED_PINS        HW_KONFIG_USER_LED_PINS
#define LED_MAXPIN      GPIO_PIN(HW_KONFIG_USER_LED_MAXNR)
#define LED_MINPIN      GPIO_PIN(HW_KONFIG_USER_LED_MINNR)

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
      for (volatile int i = 0; i < 100000; ++i) ;
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
   fault_interrupt();
}

/*
 * This programs tests the simple mailbox_t data structure
 *
 * In case of an error all LEDs blink.
 *
 */
int main(void)
{
   enable_gpio_clockcntrl(LED_PORT_BIT);
   config_output_gpio(LED_PORT, LED_PINS);

   mailbox_t mb = mailbox_INIT;

   for (uint32_t v = 1; v; v <<= 1) {

      // TEST send_mailbox: Write value v to mailbox
      assert(0 == send_mailbox(&mb, v));
      assert(1 == mb.state);
      assert(v == mb.value);

      // TEST send_mailbox: No value written cause mailbox is full
      assert(ERRFULL == send_mailbox(&mb, v));
      assert(1 == mb.state);
      assert(v == mb.value);

      // TEST recv_mailbox: Returns previously written value
      assert(v == recv_mailbox(&mb));
      assert(0 == mb.state);
      assert(v == mb.value);
   }

   // OK
   while (1) switch_led();

}

