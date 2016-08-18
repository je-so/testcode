/* title: Universal-Asynchronous-Receiver-Transmitter

   Gibt Zugriff auf

      o Serielle Kommunikationsschnittstelle

   Precondition:
    - Include "µC/hwmap.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/hwmap.h
    Header file <Hardware-Memory-Map>.

   file: µC/uart.h
    Header file <Universal-Asynchronous-Receiver-Transmitter>.

   file: TODO: uart.c
    Implementation file <Universal-Asynchronous-Receiver-Transmitter impl>.
*/
#ifndef STM32F303xC_MC_UART_HEADER
#define STM32F303xC_MC_UART_HEADER

// == exported Peripherals/HW-Units
#define UART4 ((uart_t*)HW_REGISTER_BASEADDR_UART4)
#define UART5 ((uart_t*)HW_REGISTER_BASEADDR_UART5)

// == exported types
struct uart_t;

typedef enum uart_bit_e {
   UART4_BIT = (uint8_t) (1u << 3),
   UART5_BIT = (uint8_t) (1u << 4),
} uart_bit_e;

// == exported functions

static inline int config_uart(volatile struct uart_t *uart, uint8_t databits/*7..9*/, uint8_t parity/*0:no,1:odd,2:even*/, uint8_t stopbits/*1..2*/, uint32_t baudrate/*bits per sec*/);
static inline uint32_t read_uart(volatile struct uart_t *uart);
static inline void write_uart(volatile struct uart_t *uart, uint32_t data/*only bits (databits-1):0 are used*/);
static inline int isreceiving_uart(volatile struct uart_t *uart);
static inline int issending_uart(volatile struct uart_t *uart);
static inline int isreadpossible_uart(volatile struct uart_t *uart);
static inline int iswritepossible_uart(volatile struct uart_t *uart);
static inline uint32_t errorflags_uart(volatile struct uart_t *uart);
static inline void clearerror_uart(volatile struct uart_t *uart, uint32_t errorflags);

// == definitions

typedef volatile struct uart_t {
  /* Registeroffset (USART_CR1) Control register 1.
   * Reset value: 0x0000 */
   uint32_t cr1;
  /* Registeroffset (USART_CR2) Control register 2.
   * Reset value: 0x0000 */
   uint32_t cr2;
  /* Registeroffset (USART_CR3) Control register 3.
   * Reset value: 0x0000 */
   uint32_t cr3;
  /* Registeroffset (USART_BRR) Baud rate register.
   * This register can only be written when the USART is disabled (UE=0).
   * It may be automatically updated by hardware in auto baud rate detection mode.
   * Reset value: 0x0000 */
   uint32_t brr;
   /* Registeroffset (USART_GTPR) Guard time and prescaler register.
    * Reset value: 0x0000 */
   uint32_t gtpr;
   /* Registeroffset (USART_RTOR) Receiver timeout register.
    * Reset value: 0x0000 */
   uint32_t rtor;
   /* Registeroffset (USART_RQR) Request register.
    * Reset value: 0x0000 */
   uint32_t rqr;
   /* Registeroffset (USART_ISR) Interrupt and status register.
    * Reset value: 0x020000C0 */
   uint32_t isr;
   /* Registeroffset (USART_ISR) Interrupt and status register.
    * Reset value: 0x020000C0 */
   uint32_t icr;
   /* Registeroffset (USART_RDR) Receive data register.
    * Reset value: Undefined */
   uint32_t rdr;
   /* Registeroffset (USART_TDR) Transmit data register.
    * Reset value: Undefined */
   uint32_t tdr;
} uart_t;

/* Bit 28/Bit 12 M1/M0: Word length
 * M[1:0] = 00: 1 Start bit, 8 data bits, n stop bits
 * M[1:0] = 01: 1 Start bit, 9 data bits, n stop bits
 * M[1:0] = 10: 1 Start bit, 7 data bits, n stop bits */
#define HW_REGISTER_BIT_USART_CR1_M10_MASK ((1u << 28) | (1u << 12))
#define HW_REGISTER_BIT_USART_CR1_M10_7    ((1u << 28) | 0) // not supported
#define HW_REGISTER_BIT_USART_CR1_M10_8    (0          | 0)
#define HW_REGISTER_BIT_USART_CR1_M10_9    (0          | (1u << 12))
/* Bit 10 PCE: Parity control enable
 * 0: Parity control disabled, 1: Parity control enabled
 * Bit 9 PS: Parity selection
 * 0: Even parity, 1: Odd parity */
#define HW_REGISTER_BIT_USART_CR1_PARITY_MASK  (3u << 9)
#define HW_REGISTER_BIT_USART_CR1_PARITY_OFF   (0)
#define HW_REGISTER_BIT_USART_CR1_PARITY_EVEN  (2u << 9)
#define HW_REGISTER_BIT_USART_CR1_PARITY_ODD   (3u << 9)

/* Bits 13:12 STOP[1:0]: STOP bits
 * These bits are used for programming the stop bits.
 * 00: 1 stop bit, 01: 0.5 stop bit, 10: 2 stop bits, 11: 1.5 stop bits */
#define HW_REGISTER_BIT_USART_CR2_STOP_MASK (3u << 12)
#define HW_REGISTER_BIT_USART_CR2_STOP_1    (0)
#define HW_REGISTER_BIT_USART_CR2_STOP_2    (2u << 12)


// section: inline implementation

static inline int isreadpossible_uart(uart_t *uart)
{
   // Bit 5 RXNE: Read data register not empty
   return (int) (uart->isr & (1u<<5));
}

static inline int iswritepossible_uart(uart_t *uart)
{
   // Bit 7 TXE: Transmit data register empty
   return (int) (uart->isr & (1u<<7));
}

static inline int isreceiving_uart(uart_t *uart)
{
   // Bit 16 BUSY: Busy flag  1: UART is receiving data
   return (int) (uart->isr & (1u<<16));
}

static inline int issending_uart(uart_t *uart)
{
   // Bit 6 TC: Transmission complete
   return (uart->isr & (1u<<6)) == 0;
}

static inline uint32_t errorflags_uart(uart_t *uart)
{
   // Bit 4 IDLE: Idle line detected
   // Bit 3 ORE: Overrun error (new data received while old data nor read with read_uart)
   // Bit 2 NF: START bit Noise detection flag
   // Bit 1 FE: Framing error (excessive noise or a break character received)
   // Bit 0 PE: Parity error
   return (uart->isr & 0x1f);
}

static inline void clearerror_uart(uart_t *uart, uint32_t errorflags)
{
   uart->icr = errorflags;
}

static inline uint32_t read_uart(uart_t *uart)
{
   // if parity turned on: msb of data register contains parity (either 0x80 (7 bit + parity) or 0x100 (8 bit + parity)
   return (uart->rdr & 0x1ff/*9 bits are valid at max*/);
}

static inline void write_uart(uart_t *uart, uint32_t data/*only bits (databits-1):0 are used*/)
{
   uart->tdr = data;
}

/* function config_uart:
 * Stellt Baudrate des UARTs ein (Bits/Sekunde).
 * Pro Byte werden 1 Start-Bit, databits Datenbits, 0 oder 1 Parity und stopbits Stopbits übertragen.
 * Ist parity == 2 wird ein "Even parity" übertragen, parity == 1 übertägt ein "Odd parity" und parity == 0
 * überträgt kein Parity. */
static inline int config_uart(uart_t *uart, uint8_t databits/*7..9*/, uint8_t parity/*0:no,1:odd,2:even*/, uint8_t stopbits/*1..2*/, uint32_t baudrate/*bits per sec*/)
{
   uint32_t nrbits = databits + (parity != 0)/*0..1*/;
   // assume clock source is HSI (preconfigured in enable_uart_clockcntrl)
   uint32_t usartdiv = ((HW_KONFIG_CLOCK_INTERNAL_HZ + baudrate/2) / baudrate);
   --stopbits;
   if (parity > 2 || nrbits < 8 || nrbits > 9 || stopbits > 1 || usartdiv < 16 || usartdiv > 65535) return EINVAL;
   uart->cr1 = 0; // disable
   uint32_t cr1 = HW_REGISTER_BIT_USART_CR1_M10_8;
   if (nrbits == 9) {
      cr1 = HW_REGISTER_BIT_USART_CR1_M10_9;
   }
   static_assert(HW_REGISTER_BIT_USART_CR1_PARITY_OFF == 0);
   if (parity) {
      cr1 |= parity&1 ? HW_REGISTER_BIT_USART_CR1_PARITY_ODD : HW_REGISTER_BIT_USART_CR1_PARITY_EVEN;
   }
   uart->cr1 = cr1;
   uart->cr2 = stopbits ? HW_REGISTER_BIT_USART_CR2_STOP_2 : HW_REGISTER_BIT_USART_CR2_STOP_1;
   uart->cr3 = 0;
   uart->brr = usartdiv;
   cr1 |= 1  // enable UART
        | 4  // enable receiver
        | 8; // enable transmitter
   uart->cr1 = cr1;
   return 0;
}


#endif
