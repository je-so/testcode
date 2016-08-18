#ifndef KONFIG_HEADER
#define KONFIG_HEADER

#define static_assert(C) ((void)(sizeof(char[(C)?1:-1])))
#define lengthof(Array)  (sizeof(Array)/sizeof(Array[0]))

#ifdef KONFIG_ASSERT
void assert_failed_exception(const char *filename, int linenr); // implemented by user
#define assert(C) if (!(C)) assert_failed_exception(__FILE__, __LINE__);
#else
#define assert(C) /* ignored */
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "µC/core.h"
#include "µC/board.h"
#include "µC/hwmap.h"
#include "µC/exti.h"
#include "µC/mpu.h"
#include "µC/systick.h"
#include "µC/interrupt.h"
#include "µC/clockcntrl.h"
#include "µC/adc.h"
#include "µC/basictimer.h"
#include "µC/dac.h"
#include "µC/dma.h"
#include "µC/gpio.h"
#include "µC/uart.h"

#endif
