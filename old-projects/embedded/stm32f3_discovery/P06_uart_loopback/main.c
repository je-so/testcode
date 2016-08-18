#include "konfig.h"

/*
   Vor Start des Programmes müssen die Pins PC10 und PC11
   (Port C Pin 10 und Pin 11) miteinander verbunden werden.
   Der sendende UART Nummer 4 empfängt seine eigenen Datan (loopback).

   Das Programm sendet ein Byte und prüft auf korrekten Empfang.
   Die rote LED wird gesetzt, wenn ein Fehler passiert.

   Vor dem Senden wird die gelbe LED gesetzt. Mit Druck des Userbuttons
   wird der Sendevorgang gestartet. Läuft alles gut, wird die grüne
   LED gesetzt.
*/
int main(void)
{
   // Zwei simulierte Rechnerknoten: LED Pins rot,gelb,grün
   uint16_t red_led = GPIO_PIN(13);
   uint16_t yellow_led = GPIO_PIN(14);
   uint16_t green_led = GPIO_PIN(15);

   enable_gpio_clockcntrl(GPIO_PORTA_BIT/*switch*/|GPIO_PORTE_BIT/*led*/|GPIO_PORTC_BIT/*uart*/);
   enable_uart_clockcntrl(UART4_BIT);

   config_input_gpio(GPIO_PORTA, GPIO_PIN0, GPIO_PULL_OFF);
   config_output_gpio(GPIO_PORTE, GPIO_PINS(15,8));
   config_function_gpio(GPIO_PORTC, GPIO_PINS(11,10), GPIO_FUNCTION_5/*select UART4*/);

   // yellow led signals ready
   write1_gpio(GPIO_PORTE, yellow_led);
   // wait until user button pressed
   while (read_gpio(GPIO_PORTA, GPIO_PIN0) == 0) ;
   // turn off yellow
   write0_gpio(GPIO_PORTE, yellow_led);

   if (config_uart(UART4, 8, 0, 2, 115200)) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }

   if (iswritepossible_uart(UART4) == 0
      || isreceiving_uart(UART4)) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }
   write_uart(UART4, 0x0ff);
   // check data register not yet tranfered into shift register
   if (iswritepossible_uart(UART4) != 0) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }

   // wait until uart is receiving something
   while (isreceiving_uart(UART4) == 0) ;

   // check data not yet received but write data register tranfered into shift register
   if (isreadpossible_uart(UART4) != 0
      || iswritepossible_uart(UART4) == 0
      || ! issending_uart(UART4)) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }

   while (isreadpossible_uart(UART4) == 0) ; // wait until data reveived
   // check no ongoing transfer on RX line
   if (isreceiving_uart(UART4)) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }

   uint32_t data = read_uart(UART4);

   // check read buffer is empty
   if (isreadpossible_uart(UART4) != 0) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }

   // check no read error
   if (errorflags_uart(UART4) == 0) {
      // signal error
      write1_gpio(GPIO_PORTE, red_led);
   }

   while (issending_uart(UART4)) ; // check that sender stops some time after sending last byte

   if (data == 0x0ff) {
      // turn on green LED to signal correct byte received
      write1_gpio(GPIO_PORTE, green_led);
   } else {
      // turn on yellow to signal received data
      write1_gpio(GPIO_PORTE, yellow_led);
   }

   while (1) ;
}
