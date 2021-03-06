// TODO:
#ifndef KONFIG_HEADER
#define KONFIG_HEADER

#define static_assert(C)   ((void)(sizeof(char[(C)?1:-1])))
#define lengthof(Array)    (sizeof(Array)/sizeof(Array[0]))
#define TOSTRING_(str)     #str
#define TOSTRING(str)      TOSTRING_(str)


void assert_failed_exception(const char *filename, int linenr);
            // Function is called in case of a failed assert or TEST macro.
            // It must be implemented by the user.
#define assert(C) if (!(C)) assert_failed_exception(__FILE__, __LINE__)
#define TEST(C)   if (!(C)) assert_failed_exception(__FILE__, __LINE__)

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "hw/cm4/atomic.h"   // helper
#include "hw/cm4/cpustate.h" // helper
#include "hw/cm4/iframe.h"   // helper
#include "µC/board.h"
#include "µC/core.h"
#include "µC/hwmap.h"
#include "µC/debug.h"
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
