#include "konfig.h"

const uint16_t red_led = GPIO_PIN(13);
const uint16_t yellow_led = GPIO_PIN(14);
const uint16_t green_led = GPIO_PIN(15);

void write_string(const char *str)
{
   while (*str) {
      while (! iswritepossible_uart(UART4)) ;
      write_uart(UART4, (unsigned) *str);
      ++str;
   }
}

void write_number(uint32_t nr)
{
   char buffer[11];
   unsigned len = 0;
   do {
      buffer[len++] = '0' + (nr % 10);
      nr /= 10;
   } while (nr > 0);
   while ((len--)) {
      while (! iswritepossible_uart(UART4)) ;
      write_uart(UART4, (unsigned) buffer[len]);
   }
}

uint32_t read_number(void)
{
   uint32_t nr = 0;

   for (;;) {
      while (! isreadpossible_uart(UART4)) {
         if (errorflags_uart(UART4) & 8) {
            // overrun possible: in case of error need to clear flags
            // else no read possible
            clearerror_uart(UART4, 8);
         }
      }
      uint32_t data = read_uart(UART4);
      if ('0' <= data && data <= '9') {
         nr *= 10;
         nr += data - '0';
      } else {
         break;
      }
      // echo character
      write1_gpio(GPIO_PORTE, red_led);
      while (! iswritepossible_uart(UART4)) ;
      write_uart(UART4, data);
      write0_gpio(GPIO_PORTE, red_led);
   }

   return nr;
}

/*
   Verwendete Pins:
   PC10 (Port C Pin 10): UART4_TX (Sendeleitung der seriellen Schnittstelle)
   PC11 (Port C Pin 11): UART4_RX (Empfangsleitung der seriellen Schnittstelle)

   Vor Start des Programmes müssen die Pins PC10 und PC11
   mit einem seriell-zu-USB Wandler verbunden werden.

   Grüne Leitung(Senden von PC) an PC11(Empfang an µC),
   weiße Leitung(Empfang an PC) an PC10(Senden von µC).
   Die schwarze Leitung ist an einen freien Massepin des µC anzuschliessen.


*/
int main(void)
{
   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch*/|GPIO_PORTE_BIT/*led*/|GPIO_PORTC_BIT/*uart*/);
   enable_uart_clockcntrl(UART4_BIT);

   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   config_function_gpio(GPIO_PORTC, GPIO_PINS(11,10), GPIO_FUNCTION_5/*select UART4*/);


   if (config_uart(UART4, 8, 0, 1, 115200)) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }

   // yellow led signals wait for receiving Return key from serial port
   write1_gpio(GPIO_PORTE, yellow_led);

   for (;;) {
      while (! isreadpossible_uart(UART4)) ;
      uint32_t data = read_uart(UART4);
      if (data == '\r' || data == '\n') break;
   }

   write0_gpio(GPIO_PORTE, yellow_led);

   for (;;) {
      write1_gpio(GPIO_PORTE, red_led);
      write_string("\nEingabe: ");
      write0_gpio(GPIO_PORTE, red_led);
      write1_gpio(GPIO_PORTE, green_led);
      uint32_t nr = read_number();
      write0_gpio(GPIO_PORTE, green_led);
      write1_gpio(GPIO_PORTE, red_led);
      write_string(" gelesene Eingabe: ");
      write_number(nr);
      write0_gpio(GPIO_PORTE, red_led);
   }

}
