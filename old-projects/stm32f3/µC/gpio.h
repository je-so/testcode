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

// == exported Peripherals/HW-Units
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

// == exported types
typedef uint8_t gpio_bit_t;
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

typedef enum gpiocfg_e {
   // -- 1. select one of --
   gpiocfg_INPUT     = 0,        // (default)
   gpiocfg_OUTPUT    = 1,
   gpiocfg_ANALOG    = 3,
   gpiocfg_FUNCTION0 = 2|(0 << 12),   // alternate functions are specific to port and pin number
   gpiocfg_FUNCTION1 = 2|(1 << 12),   // the correlation must be read off from a table
   gpiocfg_FUNCTION2 = 2|(2 << 12),
   gpiocfg_FUNCTION3 = 2|(3 << 12),
   gpiocfg_FUNCTION4 = 2|(4 << 12),
   gpiocfg_FUNCTION5 = 2|(5 << 12),
   gpiocfg_FUNCTION6 = 2|(6 << 12),
   gpiocfg_FUNCTION7 = 2|(7 << 12),
   gpiocfg_FUNCTION8 = 2|(8 << 12),
   gpiocfg_FUNCTION9 = 2|(9 << 12),
   gpiocfg_FUNCTION10 = 2|(10 << 12),
   gpiocfg_FUNCTION11 = 2|(11 << 12),
   gpiocfg_FUNCTION12 = 2|(12 << 12),
   gpiocfg_FUNCTION13 = 2|(13 << 12),
   gpiocfg_FUNCTION14 = 2|(14 << 12),
   gpiocfg_FUNCTION15 = 2|(15 << 12),
   // -- 2. speed
   gpiocfg_SPEED2MHZ  = 0 << 2,   // (default)
   gpiocfg_SPEED20MHZ = 1 << 2,
   gpiocfg_SPEED36MHZ = 3 << 2,
   // -- 3. select one of --
   gpiocfg_PUSHPULL  = 0 << 4,   // (default)
   gpiocfg_OPENDRAIN = 1 << 4,   // output 0 ==> 0V, output 1 ==> float, high impedance, HiZ
   // -- 4. select one of --
   gpiocfg_PULLOFF   = 0 << 5,   // (default)
   gpiocfg_PULLUP    = 1 << 5,
   gpiocfg_PULLDOWN  = 2 << 5,
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

// == exported functions

static inline void config_input_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpio_pull_e pull);
static inline void config_output_gpio(volatile struct gpio_port_t *port, uint16_t pins);
static inline void config_output0z_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpio_pull_e pull);
static inline void config_function_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpio_function_e function);
static inline void config_analog_gpio(volatile struct gpio_port_t *port, uint16_t pins);
void update_pull_gpio(volatile struct gpio_port_t *port, uint16_t pins, gpiocfg_e pull/*masked with gpiocfg_MASK_PULLUPDOWN*/);
/* function: config_interrupts_gpio
 * Für jede Pin-Nummer ist ein Interrupt möglich, aber nur ein einziger Port pro Pin-Nummer.
 * D.h. Interrupts sind nicht gleichzeitig für Pins PX[n] und PY[n] möglich, sondern nur für PX[n]
 * oder PY[n]. PX[n] und PY[m] für m != n sind möglich.
 * Die syscfg HWUnit muss vorher mit Funktion enable_syscfg_clockcntrl() eingeschaltet werden. */
int config_interrupts_gpio(uint8_t port_bit, uint16_t pins, interrupt_edge_e edge);
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
   uint32_t mode;  /* Pin-x: bits[2*x+1..2*x]  0 <= x <= 15
                      00: Input mode (reset state)
                      01: General purpose output mode
                      10: Alternate function mode
                      11: Analog mode */
   uint32_t otype; /* Bits 31..16: Reserved
                      Pin-x: bits[x]
                      0: Output push-pull (reset state)
                      1: Output open-drain */
   uint32_t speed; /* Pin-x: bits[2*x+1..2*x]  0 <= x <= 15
                      x0: Low speed     ~2MHz
                      01: Medium speed  ~20Mhz
                      11: High speed    ~36Mhz */
   uint32_t pull;  /* Pin-x: bits[2*x+1..2*x]  0 <= x <= 15
                      00: No pull-up, pull-down
                      01: Pull-up
                      10: Pull-down
                      11: Reserved */
   uint32_t indata;/* Bits 31..16: Reserved
                      Pin-x: bits[x]
                      0: Input value of pin is 0
                      1: Input value of pin is 1 */
   uint32_t outdata; /* Bits 31..16: Reserved
                      Pin-x: bits[x]
                      0: value of pin is set to 0
                      1: value of pin is set to 1 */
   uint32_t outbsrr; /* bit set reset register
                      Bits 31..16: Pin-x: bits[x+16]
                      0: No action is done
                      1: Resets the corresponding outdata x bit
                      Bits 15..0: Pin-x: bits[x]
                      0: No action is done
                      1: Sets the corresponding outdata x bit */
   uint32_t lock;    /* Locks Pins of registers MODE, otype, speed, pull, afrl, afrh
                      lock sequence:
                      write bits[16..0] = ‘1’ + bits[15..0]
                      write bits[16..0] = ‘0’ + bits[15..0]
                      write bits[16..0] = ‘1’ + bits[15..0]
                      Bit 16 LCKK: Lock key
                      This bit can be read any time. It can only be modified using the lock key write sequence.
                      0: Port configuration lock key not active
                      1: Port configuration lock key active.
                         It is locked until the next MCU reset or peripheral reset.
                      Bits 15..0: Pin-x: bits[x]
                      These bits are read/write but can only be written when the LCKK bit is ‘0.
                      0: Pin configuration not locked
                      1: Pin configuration locked */
   uint32_t aflow;  /* alternate function low register
                      Pin-x: bits[4*x+3..4*x]  0 <= x <= 7
                      0000: Alternate function 0 AF0
                      ...
                      1111: Alternate function 15 AF15
                      */
   uint32_t afhigh; /* alternate function high register
                      Pin-x: bits[4*(x-8)+3..4*(x-8)]  8 <= x <= 15
                      0000: Alternate function 0 AF0
                      ...
                      1111: Alternate function 15 AF15 */
   uint32_t outbrr; /* bit reset register
                      Bits 31..16: Reserved
                      Bits 15..0: Pin-x: bits[x]
                      0: No action is done
                      1: Resets the corresponding outdata x bit */
} gpio_port_t;


// section: inline implementation

static inline void assert_config_gpio(void)
{
   static_assert(gpiocfg_INPUT  == 0);
   static_assert(gpiocfg_OUTPUT == 1);
   static_assert((gpiocfg_FUNCTION0 & gpiocfg_FUNCTION1 & gpiocfg_FUNCTION2
                & gpiocfg_FUNCTION3 & gpiocfg_FUNCTION4 & gpiocfg_FUNCTION5
                & gpiocfg_FUNCTION6 & gpiocfg_FUNCTION7 & gpiocfg_FUNCTION8
                & gpiocfg_FUNCTION9 & gpiocfg_FUNCTION10 & gpiocfg_FUNCTION11
                & gpiocfg_FUNCTION12 & gpiocfg_FUNCTION13 & gpiocfg_FUNCTION14
                & gpiocfg_FUNCTION15) == 2);
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

   static_assert((gpiocfg_FUNCTION0 >> gpiocfg_POS_FUNCTION) == 0);
   static_assert((gpiocfg_FUNCTION1 >> gpiocfg_POS_FUNCTION) == 1);
   static_assert((gpiocfg_FUNCTION2 >> gpiocfg_POS_FUNCTION) == 2);
   static_assert((gpiocfg_FUNCTION3 >> gpiocfg_POS_FUNCTION) == 3);
   static_assert((gpiocfg_FUNCTION4 >> gpiocfg_POS_FUNCTION) == 4);
   static_assert((gpiocfg_FUNCTION5 >> gpiocfg_POS_FUNCTION) == 5);
   static_assert((gpiocfg_FUNCTION6 >> gpiocfg_POS_FUNCTION) == 6);
   static_assert((gpiocfg_FUNCTION7 >> gpiocfg_POS_FUNCTION) == 7);
   static_assert((gpiocfg_FUNCTION8 >> gpiocfg_POS_FUNCTION) == 8);
   static_assert((gpiocfg_FUNCTION9 >> gpiocfg_POS_FUNCTION) == 9);
   static_assert((gpiocfg_FUNCTION10 >> gpiocfg_POS_FUNCTION) == 10);
   static_assert((gpiocfg_FUNCTION11 >> gpiocfg_POS_FUNCTION) == 11);
   static_assert((gpiocfg_FUNCTION12 >> gpiocfg_POS_FUNCTION) == 12);
   static_assert((gpiocfg_FUNCTION13 >> gpiocfg_POS_FUNCTION) == 13);
   static_assert((gpiocfg_FUNCTION14 >> gpiocfg_POS_FUNCTION) == 14);
   static_assert((gpiocfg_FUNCTION15 >> gpiocfg_POS_FUNCTION) == 15);
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
   port->mode   = (port->mode & ~mask) | mval;
   port->otype &= ~(uint32_t)pins;        // push-pull 0
   port->speed |= mask;                   // high speed
   port->pull   = (port->pull & ~mask);   // no pull-up/down
   port->aflow  = (port->aflow  & ~ (uint32_t)amask) | (uint32_t)aval;
   port->afhigh = (port->afhigh & ~ (uint32_t)(amask>>32)) | (uint32_t)(aval>>32);
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
   port->outbsrr = pins;
}

static inline void write0_gpio(gpio_port_t *port, uint16_t pins)
{
   port->outbrr = pins;
}

/* Setzt die Ausgangspins in highpins auf 1 und in lowpins auf 0.
 * In beiden Parameterwerten gelistete Pinausgänge ((highpins&lowpins) != 0) werden auf 1 gesetzt. */
static inline void write_gpio(volatile struct gpio_port_t *port, uint16_t highpins/*1*/, uint16_t lowpins/*0*/)
{
   port->outbsrr = highpins | ((uint32_t)lowpins << 16);
}


#endif
