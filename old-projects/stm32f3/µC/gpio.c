/* title: General-Purpose-I/O impl

   Verwendete Bezeichnungen:
   Z.B. mit PD0 ist der Pin 0 des Ports D gemeint.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: gpio.h
    Header file <General-Purpose-I/O>.

   file: gpio.c
    Implementation file <General-Purpose-I/O impl>.
*/
#include "konfig.h"
#include "µC/gpio.h"

/* Register Offset (SYSCFG_EXTICR1) External interrupt configuration register 1; Reset value: 0x00000000
 * Bits 15:12: EXTI-3, Bits 11:8 EXTI-2, Bits 7:4 EXTI-1, Bits 3:0 EXTI-0
 * These bits are written by software to select the source input for the EXTI-N external interrupt.
 * x000: PA[N] pin; valid for 0 <= N <= 15
 * x001: PB[N] pin; valid for 0 <= N <= 15
 * x010: PC[N] pin; valid for 0 <= N <= 15
 * x011: PD[N] pin; valid for 0 <= N <= 15
 * x100: PE[N] pin; valid for 0 <= N <= 15
 * x101: PF[N] pin; valid for 0 <= N <= 15
 * x110: PG[N] pin; valid for 0 <= N <= 15
 * x111: PH[N] pin; only valid for 0 <= N <= 2 */
#define HW_REGISTER_OFFSET_SYSCFG_EXTICR1 0x08
/* Register Offset (SYSCFG_EXTICR2) External interrupt configuration register 2; Reset value: 0x00000000
 * Bits 15:12: EXTI-7, Bits 11:8 EXTI-6, Bits 7:4 EXTI-5, Bits 3:0 EXTI-4 */
#define HW_REGISTER_OFFSET_SYSCFG_EXTICR2 0x0C
/* Register Offset (SYSCFG_EXTICR3) External interrupt configuration register 3; Reset value: 0x00000000
 * Bits 15:12: EXTI-11, Bits 11:8 EXTI-10, Bits 7:4 EXTI-9, Bits 3:0 EXTI-8 */
#define HW_REGISTER_OFFSET_SYSCFG_EXTICR3 0x10
/* Register Offset (SYSCFG_EXTICR4) External interrupt configuration register 4; Reset value: 0x00000000
 * Bits 15:12: EXTI-15, Bits 11:8 EXTI-14, Bits 7:4 EXTI-13, Bits 3:0 EXTI-12 */
#define HW_REGISTER_OFFSET_SYSCFG_EXTICR4 0x14

static inline uint32_t get_portnr(uint8_t port_bit)
{
   uint32_t portnr = 0;
   while ((port_bit >>= 1)) {
      ++portnr;
   }
   return portnr;
}

static inline int isvalid_portnr(uint32_t portnr)
{
   return portnr <= 5;
}

static void select_interrupt_port(uint8_t portnr, uint16_t pins)
{
   volatile uint32_t *exticr = & HW_REGISTER(SYSCFG, EXTICR1);
   static_assert( HW_REGISTER_OFFSET_SYSCFG_EXTICR1 + 4 == HW_REGISTER_OFFSET_SYSCFG_EXTICR2
                  && HW_REGISTER_OFFSET_SYSCFG_EXTICR2 + 4 == HW_REGISTER_OFFSET_SYSCFG_EXTICR3
                  && HW_REGISTER_OFFSET_SYSCFG_EXTICR3 + 4 == HW_REGISTER_OFFSET_SYSCFG_EXTICR4);
   for (uint16_t pin=0, val; (val=pins>>pin); ++pin) {
      if (!(val&1)) continue;
      uint16_t shift = 4 * (pin&0x3);
      volatile uint32_t *exticrN = exticr + (pin >> 2);
      *exticrN = (*exticrN & ~(0x0f << shift)) | (portnr << shift);
   }
}

int config_interrupts_gpio(uint8_t port_bit, uint16_t pins, interrupt_edge_e edge)
{
   if (((port_bit-1) & port_bit) != 0) {
      // port_bit == 0 || "more than one port bit set"
      return EINVAL;
   }

   uint32_t portnr = get_portnr(port_bit);
   if (! isvalid_portnr(portnr)) return EINVAL;

   // select pins for corresponding ports, only one port could be active per pin number
   select_interrupt_port(portnr, pins);

   // select event when interrupt request is generated
   if ((edge & interrupt_edge_RISING) != 0) {
      HW_REGISTER(EXTI, RTSR1) |= pins;
   } else {
      HW_SREGISTER(EXTI, RTSR1) &= ~pins;
   }
   if ((edge & interrupt_edge_FALLING) != 0) {
      HW_SREGISTER(EXTI, FTSR1) |= pins;
   } else {
      HW_SREGISTER(EXTI, FTSR1) &= ~pins;
   }

   // clear any pending interrupts
   HW_SREGISTER(EXTI, PR1) |= pins;

   return 0;
}

void update_pull_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpiocfg_e pull/*masked with gpiocfg_MASK_PULLUPDOWN*/)
{
   uint32_t mask = 0;
   uint32_t bits = 0;
   static_assert(gpiocfg_MASK_PULLUPDOWN == (3 << gpiocfg_POS_PULLUPDOWN));
   pull = (pull >> gpiocfg_POS_PULLUPDOWN) & 3;
   for (uint16_t pin=0, val; (val=pins>>pin); ++pin) {
      if (!(val&1)) continue;
      mask |= (3 << (2*pin));
      bits |= (pull << (2*pin));
   }
   port->pull = (port->pull & ~mask) | bits;
}
