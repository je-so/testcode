/* title: General-Purpose-I/O

   Verwendete Bezeichnungen:
   Z.B. mit PD[0] ist der Pin 0 des Ports D gemeint.

   Precondition:
   - include "exti.h"

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/gpio.h
    Header file <General-Purpose-I/O>.

   file: µC/gpio.c
    Implementation file <General-Purpose-I/O impl>.
*/
#ifndef STM32F303xC_MC_GPIO_HEADER
#define STM32F303xC_MC_GPIO_HEADER

// == Exported Peripherals/HW-Units
#define GPIOA ((gpio_port_t*)HW_REGISTER_BASEADDR_GPIOA)
#define GPIOB ((gpio_port_t*)HW_REGISTER_BASEADDR_GPIOB)
#define GPIOC ((gpio_port_t*)HW_REGISTER_BASEADDR_GPIOC)
#define GPIOD ((gpio_port_t*)HW_REGISTER_BASEADDR_GPIOD)
#define GPIOE ((gpio_port_t*)HW_REGISTER_BASEADDR_GPIOE)
#define GPIOF ((gpio_port_t*)HW_REGISTER_BASEADDR_GPIOF)

#define GPIOA_BIT  ((gpio_bit_t)(1u << 0))
#define GPIOB_BIT  ((gpio_bit_t)(1u << 1))
#define GPIOC_BIT  ((gpio_bit_t)(1u << 2))
#define GPIOD_BIT  ((gpio_bit_t)(1u << 3))
#define GPIOE_BIT  ((gpio_bit_t)(1u << 4))
#define GPIOF_BIT  ((gpio_bit_t)(1u << 5))

#define GPIO_PORT_NR(PORT)    (((uintptr_t)port >> 10) & 0x07)
#define GPIO_PORT_BIT(PORT)   (1u << GPIO_PORT_NR(PORT))

#define GPIO_PIN0 1
#define GPIO_PIN1 2
#define GPIO_PIN2 4
#define GPIO_PIN3 8
#define GPIO_PIN4 16
#define GPIO_PIN5 32
#define GPIO_PIN6 64
#define GPIO_PIN7 128
#define GPIO_PIN8 256
#define GPIO_PIN9 512
#define GPIO_PIN10 1024
#define GPIO_PIN11 2048
#define GPIO_PIN12 4096
#define GPIO_PIN13 8192
#define GPIO_PIN14 16384
#define GPIO_PIN15 32768
/* PINS in range maxnr..minnr && minnr <= maxnr && 0 <= minnr && maxnr <= 15 */
#define GPIO_PINS(maxnr, minnr)  ((uint16_t)((0xffff >> (15-(maxnr))) & (0xffff << (minnr))))
#define GPIO_PIN(nr)             ((uint16_t)(1u << (nr)))

// == Exported Types
typedef uint32_t gpio_bit_t;
struct gpio_port_t;

typedef enum gpio_function_e {
   GPIO_FUNCTION_0, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_1, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_2, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_3, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_4, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_5, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_6, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_7, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_8, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_9, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_14, // alternate function ==> specific to port and pin
   GPIO_FUNCTION_15  // alternate function ==> specific to port and pin
} gpio_function_e;

typedef enum gpio_pull_e {
   GPIO_PULL_OFF,
   GPIO_PULL_UP,
   GPIO_PULL_DOWN
} gpio_pull_e;

typedef enum gpiocfg_e {
   // -- 1. select one of --
   gpiocfg_INPUT     = 0,        // (default) Pin values are only readable.
   gpiocfg_OUTPUT    = 1,        // Pin values in output mode are also readable.
   gpiocfg_ANALOG    = 3,        // Pin values read as 0.
   gpiocfg_AF0 = 2|(0 << 12),    // Pin values in AF-mode are also readable.
   gpiocfg_AF1 = 2|(1 << 12),    // Alternate functions are specific to port and pin number.
   gpiocfg_AF2 = 2|(2 << 12),    // The correlation must be lookup up in the datasheet.
   gpiocfg_AF3 = 2|(3 << 12),
   gpiocfg_AF4 = 2|(4 << 12),
   gpiocfg_AF5 = 2|(5 << 12),
   gpiocfg_AF6 = 2|(6 << 12),
   gpiocfg_AF7 = 2|(7 << 12),
   gpiocfg_AF8 = 2|(8 << 12),
   gpiocfg_AF9 = 2|(9 << 12),
   gpiocfg_AF10 = 2|(10 << 12),
   gpiocfg_AF11 = 2|(11 << 12),
   gpiocfg_AF12 = 2|(12 << 12),
   gpiocfg_AF13 = 2|(13 << 12),
   gpiocfg_AF14 = 2|(14 << 12),
   gpiocfg_AF15 = 2|(15 << 12),
   // -- 2. select one of
   gpiocfg_SPEED2MHZ  = 0 << 2,  // (default)
   gpiocfg_SPEED20MHZ = 1 << 2,
   gpiocfg_SPEED36MHZ = 3 << 2,
   // -- 3. select one of --
   gpiocfg_PUSHPULL  = 0 << 4,   // (default)
   gpiocfg_OPENDRAIN = 1 << 4,   // output 0 ==> 0V, output 1 ==> float, high impedance, HiZ
   // -- 4. select one of --
   gpiocfg_PULLOFF   = 0 << 5,   // (default) no pull-up/down resistor
   gpiocfg_PULLUP    = 1 << 5,   // weak pull up resistor (3.3V)
   gpiocfg_PULLDOWN  = 2 << 5,   // weak pull down resistor (0V)
   // -- 5. select one of --
   gpiocfg_INTERRUPT_OFF     = 0 << 7,   // (default)
   gpiocfg_INTERRUPT_RISING  = 1 << 7,
   gpiocfg_INTERRUPT_FALLING = 2 << 7,
   gpiocfg_INTERRUPT_BOTHEDGES = gpiocfg_INTERRUPT_FALLING|gpiocfg_INTERRUPT_RISING,
   // -- mask values for bitfields, do not use as config values --
   gpiocfg_POS_MODE        = 0,
   gpiocfg_MASK_MODE       = 3 << gpiocfg_POS_MODE,
   gpiocfg_POS_SPEED       = 2,
   gpiocfg_MASK_SPEED      = 3 << gpiocfg_POS_SPEED,
   gpiocfg_POS_OUTTYPE     = 4,
   gpiocfg_MASK_OUTTYPE    = 1 << gpiocfg_POS_OUTTYPE,
   gpiocfg_POS_PULLUPDOWN  = 5,
   gpiocfg_MASK_PULLUPDOWN = 3 << gpiocfg_POS_PULLUPDOWN,
   gpiocfg_POS_INTERRUPT   = 7,
   gpiocfg_MASK_INTERRUPT  = 3 << gpiocfg_POS_INTERRUPT,
   gpiocfg_POS_FUNCTION    = 12,
   gpiocfg_MASK_FUNCTION   = 15 << gpiocfg_POS_FUNCTION,
} gpiocfg_e;

// == Exported Functions

// To use a port call enable_gpio_clockcntrl(GPIO?_BIT) before. Also call enable_syscfg_clockcntrl() if you want use interrupts on pins.

int config_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpiocfg_e cfg);
static inline void config_input_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpio_pull_e pull);
static inline void config_output_gpio(volatile struct gpio_port_t *port, uint16_t pins);
static inline void config_output0z_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpio_pull_e pull);
static inline void config_function_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpio_function_e function);
static inline void config_analog_gpio(volatile struct gpio_port_t *port, uint16_t pins);
void update_pull_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpiocfg_e pull/*masked with gpiocfg_MASK_PULLUPDOWN*/);
int config_interrupts_gpio(uint8_t port_bit, uint16_t pins, interrupt_edge_e edge); // Per pin-nr only one port is supported
static inline void enable_interrupts_gpio(uint16_t pins);
static inline void disable_interrupts_gpio(uint16_t pins);
static inline void clear_interrupts_gpio(uint16_t pins);
static inline void generate_interrupts_gpio(uint16_t pins);
static inline uint32_t read_gpio(volatile struct gpio_port_t *port, uint16_t pins);
static inline void write1_gpio(volatile struct gpio_port_t *port, uint16_t pins);
static inline void write0_gpio(volatile struct gpio_port_t *port, uint16_t pins);
static inline void write_gpio(volatile struct gpio_port_t *port, uint16_t highpins/*1*/, uint16_t lowpins/*0*/);

// == definitions

typedef volatile struct gpio_port_t {
   uint32_t       mode;   /* GPIO port Mode Register, rw, Offset: 0x00, Reset: 0x00000000, 0xA8000000 (for port A), 0x00000280 (for port B)
                           * Configure the I/O mode of pins 0 <= x <= 15.
                           * Bits 2*x+1:2*x 00: Pin x input mode. 01: Output mode. 10: Alternate function mode. 11: Analog mode.
                           */
   uint32_t       otype;  /* GPIO port Output Type Register, rw, Offset: 0x04, Reset: 0x00000000
                           * Configure the output type of pins 0 <= x <= 15.
                           * Bits 31:16 Reserved.
                           * Bit x 0: Pin x output is push-pull (0V/3.3V). 1: Pin x output open-drain (0V/HiZ).
                           */
   uint32_t       speed;  /* GPIO port Output Speed Register, rw, Offset: 0x08, Reset: 0x00000000, 0x0C000000 (for port A), 0x000000C0 (for port B)
                           * Configure the I/O output speed of pins 0 <= x <= 15.
                           * Bits 2*x+1:2*x -0: Pin x low speed ~2MHz. 01: Medium speed ~20Mhz. 11: High speed ~36Mhz.
                           */
   uint32_t       pull;   /* GPIO port Pull-up/Pull-down Register, rw, Offset: 0x0C, Reset: 0x00000000, 0x64000000 (for port A), 0x00000100 (for port B)
                           * Configure the I/O pull-up or pull-down of pins 0 <= x <= 15.
                           * Bits 2*x+1:2*x 00: Pin x no pull-up,pull-down. 01: Pull-up. 10: Pull-down. 11: Reserved.
                           */
   uint32_t const indata; /* GPIO port Input Data Register, ro, Offset: 0x10, Reset: 0x0000????
                           * Contains the input value of the corresponding I/O port pins 0 <= x <= 15.
                           * Bits 31:16 Reserved.
                           * Bit x 0: Input value of pin x is 0. 1: Input value of pin x is 1.
                           */
   uint32_t       outdata;/* GPIO port Output Data Register, rw, Offset: 0x14, Reset: 0x00000000
                           * Contains the current output value of the corresponding I/O port pins 0 <= x <= 15.
                           * Bits 31:16 Reserved
                           * Bit x 0: Output value of pin x is set to 0. 1: Output value of pin x is set to 1 (or HiZ).
                           */
   uint32_t       bsrr;   /* GPIO port Bit Set/Reset Register, wo, Offset: 0x18, Reset: 0x00000000
                           * Change the current output value of pins 0 <= x <= 15 atomically.
                           * Bits x+16 (BRx) writes 0: Output value of pin x unchanged. 1: Resets pin x to 0. reads 0: Always returns 0.
                           * Bits x    (BSx) writes 0: Output value of pin x unchanged. 1: Sets pin x to 1, even if x+16 is also set to 1. reads 0: Always returns 0.
                           */
   uint32_t       lock;   /* GPIO port Configuration Lock Register, rw, Offset: 0x1C, Reset: 0x00000000
                           * Locks bits in registers mode, otype, speed, pull, afrl, afrh for pins 0 <= x <= 15.
                           * Only ports A,B and D supported.
                           * Lock sequence:
                           * 1. write bit[16] = ‘1’ and bits[15:0] = pins_to_lock
                           * 2. write bit[16] = ‘0’ and bits[15:0] = pins_to_lock
                           * 3. write bit[16] = ‘1’ and bits[15:0] = pins_to_lock
                           * Bit 16 (LCKK:Lock key) 0: Port configuration lock key not active and bits are rw. 1: Port configuration locked until the next MCU reset or peripheral reset. Bits in this register are only ro.
                           * Bit x 0: Pin x configuration not locked. 1: Pin x configuration locked.
                           */
   uint32_t       aflow;  /* GPIO port Alternate Function Low Register, rw, Offset: 0x20, Reset: 0x00000000
                           * Configure alternate function I/Os of pins 0 <= x <= 7.
                           * Bits 4*x+3:4*x 0000: Alternate function 0 selected for pin x. ... 1111: Alternate function 15 selected for pin x.
                           */
   uint32_t       afhigh; /* GPIO port Alternate Function High Register, rw, Offset: 0x24, Reset: 0x00000000
                           * Configure alternate function I/Os of pins 8 <= x <= 15.
                           * Bits 4*(x-8)+3:4*(x-8) (AFRx) 0000: Alternate function 0 selected for pin x. ... 1111: Alternate function 15 selected for pin x.
                           */
   uint32_t       brr;    /* GPIO port Bit Reset Register, wo, Offset: 0x28, Reset: 0x00000000
                           * Reset the current output value of pins 0 <= x <= 15 atomically.
                           * Bits 31:16 Reserved.
                           * Bit x (BRx) writes 0: No action is done. 1: Resets pin x to 0. reads 0: Always return 0.
                           */
} gpio_port_t;


// section: inline implementation

static inline void assert_config_gpio(void)
{
   static_assert(gpiocfg_INPUT  == 0);
   static_assert(gpiocfg_OUTPUT == 1);
   static_assert((gpiocfg_AF0 & gpiocfg_AF1 & gpiocfg_AF2
                & gpiocfg_AF3 & gpiocfg_AF4 & gpiocfg_AF5
                & gpiocfg_AF6 & gpiocfg_AF7 & gpiocfg_AF8
                & gpiocfg_AF9 & gpiocfg_AF10 & gpiocfg_AF11
                & gpiocfg_AF12 & gpiocfg_AF13 & gpiocfg_AF14
                & gpiocfg_AF15) == 2);
   static_assert(gpiocfg_ANALOG == 3);
   static_assert(gpiocfg_MASK_MODE == 3);
   static_assert(gpiocfg_POS_MODE  == 0);

   static_assert((gpiocfg_SPEED2MHZ  >> gpiocfg_POS_SPEED) == 0);
   static_assert((gpiocfg_SPEED20MHZ >> gpiocfg_POS_SPEED) == 1);
   static_assert((gpiocfg_SPEED36MHZ >> gpiocfg_POS_SPEED) == 3);
   static_assert(gpiocfg_MASK_SPEED == (3 << gpiocfg_POS_SPEED));

   static_assert((gpiocfg_PUSHPULL >> gpiocfg_POS_OUTTYPE)  == 0);
   static_assert((gpiocfg_OPENDRAIN >> gpiocfg_POS_OUTTYPE) == 1);
   static_assert(gpiocfg_MASK_OUTTYPE == (1 << gpiocfg_POS_OUTTYPE));

   static_assert((gpiocfg_PULLOFF  >> gpiocfg_POS_PULLUPDOWN) == 0);
   static_assert((gpiocfg_PULLUP   >> gpiocfg_POS_PULLUPDOWN) == 1);
   static_assert((gpiocfg_PULLDOWN >> gpiocfg_POS_PULLUPDOWN) == 2);
   static_assert(gpiocfg_MASK_PULLUPDOWN == (3 << gpiocfg_POS_PULLUPDOWN));

   static_assert((gpiocfg_INTERRUPT_OFF     >> gpiocfg_POS_INTERRUPT) == 0);
   static_assert((gpiocfg_INTERRUPT_RISING  >> gpiocfg_POS_INTERRUPT) == 1);
   static_assert((gpiocfg_INTERRUPT_FALLING >> gpiocfg_POS_INTERRUPT) == 2);
   static_assert(gpiocfg_MASK_INTERRUPT == (3 << gpiocfg_POS_INTERRUPT));

   static_assert((gpiocfg_AF0 >> gpiocfg_POS_FUNCTION) == 0);
   static_assert((gpiocfg_AF1 >> gpiocfg_POS_FUNCTION) == 1);
   static_assert((gpiocfg_AF2 >> gpiocfg_POS_FUNCTION) == 2);
   static_assert((gpiocfg_AF3 >> gpiocfg_POS_FUNCTION) == 3);
   static_assert((gpiocfg_AF4 >> gpiocfg_POS_FUNCTION) == 4);
   static_assert((gpiocfg_AF5 >> gpiocfg_POS_FUNCTION) == 5);
   static_assert((gpiocfg_AF6 >> gpiocfg_POS_FUNCTION) == 6);
   static_assert((gpiocfg_AF7 >> gpiocfg_POS_FUNCTION) == 7);
   static_assert((gpiocfg_AF8 >> gpiocfg_POS_FUNCTION) == 8);
   static_assert((gpiocfg_AF9 >> gpiocfg_POS_FUNCTION) == 9);
   static_assert((gpiocfg_AF10 >> gpiocfg_POS_FUNCTION) == 10);
   static_assert((gpiocfg_AF11 >> gpiocfg_POS_FUNCTION) == 11);
   static_assert((gpiocfg_AF12 >> gpiocfg_POS_FUNCTION) == 12);
   static_assert((gpiocfg_AF13 >> gpiocfg_POS_FUNCTION) == 13);
   static_assert((gpiocfg_AF14 >> gpiocfg_POS_FUNCTION) == 14);
   static_assert((gpiocfg_AF15 >> gpiocfg_POS_FUNCTION) == 15);
   static_assert(gpiocfg_MASK_FUNCTION == (15 << gpiocfg_POS_FUNCTION));

   static_assert((gpiocfg_MASK_MODE&gpiocfg_MASK_SPEED)   == 0);
   static_assert((gpiocfg_MASK_MODE&gpiocfg_MASK_OUTTYPE) == 0);
   static_assert((gpiocfg_MASK_MODE&gpiocfg_MASK_PULLUPDOWN) == 0);
   static_assert((gpiocfg_MASK_MODE&gpiocfg_MASK_INTERRUPT) == 0);
   static_assert((gpiocfg_MASK_MODE&gpiocfg_MASK_FUNCTION) == 0);
   static_assert((gpiocfg_MASK_SPEED&gpiocfg_MASK_OUTTYPE) == 0);
   static_assert((gpiocfg_MASK_SPEED&gpiocfg_MASK_PULLUPDOWN) == 0);
   static_assert((gpiocfg_MASK_SPEED&gpiocfg_MASK_INTERRUPT) == 0);
   static_assert((gpiocfg_MASK_SPEED&gpiocfg_MASK_FUNCTION) == 0);
   static_assert((gpiocfg_MASK_OUTTYPE&gpiocfg_MASK_PULLUPDOWN) == 0);
   static_assert((gpiocfg_MASK_OUTTYPE&gpiocfg_MASK_INTERRUPT) == 0);
   static_assert((gpiocfg_MASK_OUTTYPE&gpiocfg_MASK_FUNCTION) == 0);
   static_assert((gpiocfg_MASK_PULLUPDOWN&gpiocfg_MASK_INTERRUPT) == 0);
   static_assert((gpiocfg_MASK_PULLUPDOWN&gpiocfg_MASK_FUNCTION) == 0);
   static_assert((gpiocfg_MASK_INTERRUPT&gpiocfg_MASK_FUNCTION) == 0);
}

static inline int isvalidnr_gpio(uint32_t portnr)
{
   return portnr <= 5;
}

static inline uint32_t portnr_gpio(const gpio_port_t* port)
{
   static_assert((((uintptr_t)HW_REGISTER_BASEADDR_GPIOA >> 10)&0x7) == 0);
   static_assert((((uintptr_t)HW_REGISTER_BASEADDR_GPIOB >> 10)&0x7) == 1);
   static_assert((((uintptr_t)HW_REGISTER_BASEADDR_GPIOC >> 10)&0x7) == 2);
   static_assert((((uintptr_t)HW_REGISTER_BASEADDR_GPIOD >> 10)&0x7) == 3);
   static_assert((((uintptr_t)HW_REGISTER_BASEADDR_GPIOE >> 10)&0x7) == 4);
   static_assert((((uintptr_t)HW_REGISTER_BASEADDR_GPIOF >> 10)&0x7) == 5);
   static_assert(GPIO_PORT_NR(GPIOA) == 0);
   static_assert(GPIO_PORT_NR(GPIOB) == 0);
   static_assert(GPIO_PORT_NR(GPIOC) == 0);
   static_assert(GPIO_PORT_NR(GPIOD) == 0);
   static_assert(GPIO_PORT_NR(GPIOE) == 0);
   static_assert(GPIO_PORT_NR(GPIOF) == 0);

   return GPIO_PORT_NR(GPIOA);
}

static inline uint32_t portbit_gpio(const gpio_port_t* port)
{
   static_assert(GPIO_PORT_BIT(GPIOA) == 1);
   static_assert(GPIO_PORT_BIT(GPIOB) == 2);
   static_assert(GPIO_PORT_BIT(GPIOC) == 4);
   static_assert(GPIO_PORT_BIT(GPIOD) == 8);
   static_assert(GPIO_PORT_BIT(GPIOE) == 16);
   static_assert(GPIO_PORT_BIT(GPIOF) == 32);
   return GPIO_PORT_BIT(port);
}

static inline void enable_interrupts_gpio(uint16_t pins)
{
   HW_SREGISTER(EXTI, IMR1) |= pins;
}

static inline void disable_interrupts_gpio(uint16_t pins)
{
   HW_SREGISTER(EXTI, IMR1) &= ~pins;
}

/* function: clear_interrupts_gpio
 * Löscht Interrupt-Pending Bit für entsprechenden Pin. D.h. der Interrupt wird nach Beendigung
 * der Interrupt-Service-Routine nicht wieder erneut aktiviert. */
static inline void clear_interrupts_gpio(uint16_t pins)
{
   HW_SREGISTER(EXTI, PR1) |= pins;
}

static inline void generate_interrupts_gpio(uint16_t pins)
{
   HW_SREGISTER(EXTI, SWIER1) |= pins;
}

static inline void config_input_gpio(gpio_port_t *port, uint16_t pins, gpio_pull_e pull)
{
   uint32_t mask = 0;
   uint32_t pval = 0;
   for (uint16_t pin=0, val; (val=pins>>pin); ++pin) {
      if (!(val&1)) continue;
      mask |= (3 << (2*pin));
      pval |= (pull << (2*pin));
   }
   port->mode  &= ~mask;   // input mode 00
   port->speed |= mask;    // high speed 11
   port->pull   = (port->pull & ~mask) | pval;
}

static inline void config_output_gpio(gpio_port_t *port, uint16_t pins)
{
   uint32_t mask = 0;
   uint32_t mval = 0;
   for (uint16_t pin=0, val; (val=pins>>pin); ++pin) {
      if (!(val&1)) continue;
      mask |= (3 << (2*pin));
      mval |= (1 << (2*pin));
   }
   port->mode   = (port->mode & ~mask) | mval;
   port->otype &= ~(uint32_t)pins;  // push-pull 0
   port->speed |= mask;             // high speed 11
   port->pull   = (port->pull & ~mask); // no pull-up/down
}

static inline void config_output0z_gpio(gpio_port_t *port, uint16_t pins, gpio_pull_e pull)
{
   uint32_t mask = 0;
   uint32_t mval = 0;
   uint32_t pval = 0;
   for (uint16_t pin=0, val; (val=pins>>pin); ++pin) {
      if (!(val&1)) continue;
      mask |= (3 << (2*pin));
      mval |= (1 << (2*pin));
      pval |= (pull << (2*pin));
   }
   port->mode   = (port->mode & ~mask) | mval;
   port->otype |= (uint32_t)pins;   // open-drain 1 (0V or floating/high impedance)
   port->speed |= mask;             // high speed 11
   port->pull   = (port->pull & ~mask) | pval;
}

static inline void config_function_gpio(gpio_port_t *port, uint16_t pins, gpio_function_e fct)
{
   uint32_t mask = 0;
   uint32_t mval = 0;
   uint64_t amask = 0;
   uint64_t aval = 0;
   for (uint16_t pin=0, val; (val=pins>>pin); ++pin) {
      if (!(val&1)) continue;
      mask |= (3 << (2*pin));
      mval |= (2 << (2*pin));
      amask |= ((uint64_t)15 << (4*pin));
      aval  |= ((uint64_t)fct << (4*pin));
   }
   port->mode  &= ~mask;                  // set pins to input mode
   port->otype &= ~(uint32_t)pins;        // push-pull 0
   port->speed |= mask;                   // high speed
   port->pull   = (port->pull & ~mask);   // no pull-up/down
   port->aflow  = (port->aflow  & ~ (uint32_t)amask) | (uint32_t)aval;              // select alternate function for pins
   port->afhigh = (port->afhigh & ~ (uint32_t)(amask>>32)) | (uint32_t)(aval>>32);  // select alternate function for pins
   port->mode  |= mval;                   // switch to alternate function mode
}

static inline void config_analog_gpio(gpio_port_t *port, uint16_t pins)
{
   uint32_t mask = 0;
   for (uint16_t pin=0, val; (val=pins>>pin); ++pin) {
      if (!(val&1)) continue;
      mask |= (3 << (2*pin));
   }
   port->mode = (port->mode | mask/*Bits 11: analog mode*/);
   port->pull = (port->pull & ~mask);   // no pull-up/down
}

static inline uint32_t read_gpio(gpio_port_t *port, uint16_t pins)
{
   return (port->indata & pins);
}

static inline void write1_gpio(gpio_port_t *port, uint16_t pins)
{
   port->bsrr = pins;
}

static inline void write0_gpio(gpio_port_t *port, uint16_t pins)
{
   port->brr = pins;
}

/* Setzt die Ausgangspins in highpins auf 1 und in lowpins auf 0.
 * In beiden Parameterwerten gelistete Pinausgänge ((highpins&lowpins) != 0) werden auf 1 gesetzt. */
static inline void write_gpio(volatile struct gpio_port_t *port, uint16_t highpins/*1*/, uint16_t lowpins/*0*/)
{
   port->bsrr = highpins | ((uint32_t)lowpins << 16);
}


#endif
