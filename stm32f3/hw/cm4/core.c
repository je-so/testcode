/* title: CortexM4-Core

   Beschreibt die Register-Struktur der dem Cortex-M4 Prozessor internen Peripherie:

      • SysTick - 24-Bit clear-on-write, decrementing, wrap-on-zero Timer
      • Nested Vectored Interrupt Controller (NVIC) – Exceptions und Interrupts
      • System Control Block (SCB) - System Information und System Konfiguration
      • Memory Protection Unit (MPU) - ARMv7 Protected Memory System Architecture (PMSA)
      • Floating-Point Unit (FPU) - Single-precision HArdware support (+ fixed-point Konvertierung)

   Das STM32F303xC Spezifische ist in µC/board.h ausgegliedert.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: µC/core.h
    Header file <CortexM4-Core>.

   file: hw/cm4/core.c
    Implementation file <CortexM4-Core impl>.
*/
#include "konfig.h"
#include "µC/core.h"


#ifdef KONFIG_UNITTEST

int unittest_hw_cortexm4_core()
{
   // TODO: add other tests for core


   // TEST ishigher_corepriority
   static_assert(core_priority_NONE == 0);
   static_assert(core_priority_HIGH <  core_priority_MIN);
   static_assert(core_priority_INCR == -1);
   static_assert(core_priority_DECR == +1);
   TEST(1 == ishigher_corepriority(core_priority_HIGH, core_priority_MIN));
   TEST(1 == ishigher_corepriority(core_priority_NONE, core_priority_HIGH));
   TEST(0 == ishigher_corepriority(core_priority_MIN, core_priority_HIGH));
   TEST(0 == ishigher_corepriority(core_priority_HIGH, core_priority_NONE));
   TEST(0 == ishigher_corepriority(core_priority_MIN, core_priority_MIN));
   TEST(0 == ishigher_corepriority(core_priority_HIGH, core_priority_HIGH));

   // TEST ishighequal_corepriority
   TEST(1 == ishighequal_corepriority(core_priority_HIGH, core_priority_MIN));
   TEST(1 == ishighequal_corepriority(core_priority_NONE, core_priority_HIGH));
   TEST(0 == ishighequal_corepriority(core_priority_MIN, core_priority_HIGH));
   TEST(0 == ishighequal_corepriority(core_priority_HIGH, core_priority_NONE));
   TEST(1 == ishighequal_corepriority(core_priority_MIN, core_priority_MIN));
   TEST(1 == ishighequal_corepriority(core_priority_HIGH, core_priority_HIGH));

   // TEST getpriority_core
   TEST(core_priority_NONE == getpriority_core());

   // TEST setpriority_core
   for (uint32_t i = core_priority_HIGH; ishighequal_corepriority(i, core_priority_MIN); i += core_priority_DECR) {
      setpriority_core((core_priority_e) i);
      TEST((core_priority_e) i == getpriority_core());
   }
   setpriority_core(core_priority_NONE);
   TEST(core_priority_NONE == getpriority_core());

   // TEST setprioritymax_core: core_priority_NONE does not disable
   setpriority_core(core_priority_MIN);
   TEST(core_priority_MIN == getpriority_core());
   setprioritymax_core(core_priority_NONE);
   TEST(core_priority_MIN == getpriority_core());

   // TEST setprioritymax_core
   for (volatile uint32_t i = core_priority_NONE; ishighequal_corepriority(i, core_priority_MIN); i += core_priority_DECR) {
      for (volatile uint32_t i2 = core_priority_NONE; ishighequal_corepriority(i2, core_priority_MIN); i2 += core_priority_DECR) {
         volatile core_priority_e e = (i == 0 ? i2 : i2 == 0 ? i : i < i2 ? i : i2);
         setpriority_core((core_priority_e) i);
         TEST(i == getpriority_core());
         setprioritymax_core((core_priority_e) i2);
         TEST(e == getpriority_core());
      }
   }
   // reset
   setpriority_core(core_priority_NONE);

   return 0;
}

#endif
