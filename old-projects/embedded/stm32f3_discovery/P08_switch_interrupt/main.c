#include "konfig.h"

volatile uint32_t counter = 0;
volatile uint32_t msec = 0;
volatile uint32_t button2_stable_msec = 0;

// turn off 8 LEDs and turn on single LED representing state of counter
static void set_led_representing_counter(void)
{
   uint32_t leds = 1u << (8+(counter&0x7));
   setpins_gpio(GPIO_PORTE, leds/*on*/, GPIO_PINS(15,8)&~leds/*off*/);
}

void systick_interrupt(void)
{
   static uint32_t flash_time = 1000;
   // no acknowledge needed
   // Das Pending-Bit des Interrupt Controllers (NVIC)
   // wird automatisch bei Betreten dieser Funktion gelöscht
   msec += 10;
   if (msec == flash_time) {
      // flashe alle 8 LEDs on
      write1_gpio(GPIO_PORTE, GPIO_PINS(15,8));

   } else if (msec == flash_time+10) {
      flash_time += 1000;
      // schalte alle 8 LEDs aus bis auf diejenige LED, welche counter repräsentiert
      set_led_representing_counter();
   }

   // entprelle button2
   if (msec == button2_stable_msec) {
      button2_stable_msec = 0;
      if (read_gpio(GPIO_PORTD, GPIO_PIN2) == 0) {
         // button 2 an PD2 gedrückt (negative Logik) ==> Simuliere Button0
         generate_interrupts_gpio(GPIO_PIN0);
      }
      clear_interrupts_gpio(GPIO_PIN2);
      enable_interrupts_gpio(GPIO_PIN2);
   }
}

void gpiopin0_interrupt(void)
{
   // Würde der Interrupt nicht bestätigt, käme es zu einer Endlosschleife,
   // d.h. der Interrupt würde nach Beendigung dieser Funktion sofort wieder aktiviert.
   clear_interrupts_gpio(GPIO_PIN0);
   // LED weiterschalten
   ++counter;
   // turn off 8 LEDs and turn on single LED representing state of counter
   set_led_representing_counter();
}

void gpiopin2_tsc_interrupt(void)
{
   clear_interrupts_gpio(GPIO_PIN2);
   disable_interrupts_gpio(GPIO_PIN2);
   button2_stable_msec = msec + 30;
}

/*
   Button Interrupt

   Jedes Mal, wenn der Benutzer den blauen User-Button auf dem Board klickt,
   wird die Interrupt-Service-Routine (ISR) gpiopin0_interrupt aufgerufen,
   die einen counter erhöht.
   Die 3 niederwertigsten Bits counter bestimmen, welche der 8 LEDs
   eingeschaltet wird.

   Um die Sache etwas spannender zu machen, wird alle 10msec der systick Interrupt
   aufgerufen. Dieser interrupt sorgt dafür, dass jede Sekunde alle LEDs für 10msec
   aufblitzen.

   Der Systick ist ein Cortex-M Prozessor eigener Timer, der immer dann
   einen Interrupt auslöst, wenn der Zähler von 1 auf 0 herunterzählt.
   Der Systick-Counter wird im Prozessortakt heruntergezählt (8Mhz nach Reset).
   Hat der Counter den Wert 0 erreicht, wird er beim nächsten Prozessor-Takt
   wieder aug seinen Startwert zurückgesetzt und das Herunterzählen beginnnt aufs
   Neue.

   Zusätzlich wird Pin2 an PortD als Eingabepin konfiguriert mit Pull-Up Wiederstand,
   damit ein zweiter Schalter mit negative Logik angeschlossen werden kann (PD2--Button--Gnd).
   Es wird angenommen, dass dieser Schalter entprellt werden muss.
   Dies wird mit dem Timer und einer Wartezeit von 30msec realisiert.
*/
int main(void)
{
   enable_syscfg_clockcntrl(); // für gpio Interrupts an anderen Ports als PORTA benötigt
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch*/|GPIO_PORTE_BIT/*led*/|GPIO_PORTD_BIT/*additional switch*/);
   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_input_gpio(GPIO_PORTD, GPIO_PIN2, GPIO_PULL_UP);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   config_systick(80000/*10 msec assuming 8Mhz bus clock*/, systickcfg_CORECLK|systickcfg_INTERRUPT|systickcfg_ENABLE);

   set_led_representing_counter();

   config_interrupts_gpio(GPIO_PORTA_BIT, GPIO_PIN0, interrupt_edge_RISING);
   config_interrupts_gpio(GPIO_PORTD_BIT, GPIO_PIN2, interrupt_edge_FALLING); // negative Logik

   enable_interrupts_gpio(GPIO_PIN0|GPIO_PIN2);
   enable_interrupt_nvic(interrupt_GPIOPIN0);
   enable_interrupt_nvic(interrupt_GPIOPIN2_TSC);

   // warte 1 Sekunde, damit openocd genug Zeit hat zu flashen usw.
   while (msec != 1000);

// #define TEST_MASK_INTERRUPTS_WITH_BASEPRIORITY
#ifdef TEST_MASK_INTERRUPTS_WITH_BASEPRIORITY
   // Falls gesetzt, wird interrupt_GPIOPIN0 maskiert, weil seine Priorität nicht höher ist als die Basispriorität
   setbasepriority_interrupt(1);
   setpriority_interrupt_nvic(interrupt_GPIOPIN0, 1);
#endif

   while (1) {
// #define TEST_DEBUG
#ifdef TEST_DEBUG
      if (0 == (msec % 1010)) {
         generate_interrupt_nvic(interrupt_GPIOPIN0);
         while (0 == (msec % 1010)) ;
      }
#else
      wait_for_interrupt(); // entfernen, falls Debugging
#endif
   }

}
