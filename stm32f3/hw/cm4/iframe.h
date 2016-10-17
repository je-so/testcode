/* title: Interrupt-Stack-Frame

   Defines the number and type of the registers
   pushed on the stack of the current running thread
   if interrupted by the CPU.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: iframe.h
    Header file <Interrupt-Stack-Frame>.
*/
#ifndef HW_CORTEXM4_IFRAME_HEADER
#define HW_CORTEXM4_IFRAME_HEADER

/* == Import == */

/* no imports */

/* == Typen == */

typedef enum iframe_e {
   // -- common registers with and without FPU

   iframe_R0 = 0,    // register is on top of stack sp[0], register R1 at sp[1], ...
   iframe_R1,
   iframe_R2,
   iframe_R3,
   iframe_R12,
   iframe_R14,
   iframe_LR = iframe_R14,
   iframe_PC,
   iframe_PSR,  // Bit[9] remembers if stack frame has additional alignment of a single uint32_t

   iframe_PADDING,   // space only used if BIT[9] of iframe_PSR was set and no FPU selected

   // -- registers only used if FPU was used

   iframe_S0 = iframe_PADDING,
   iframe_S1,
   iframe_S2,
   iframe_S3,
   iframe_S4,
   iframe_S5,
   iframe_S6,
   iframe_S7,
   iframe_S8,
   iframe_S9,
   iframe_S10,
   iframe_S11,
   iframe_S12,
   iframe_S13,
   iframe_S14,
   iframe_S15,
   iframe_FPSCR,
   iframe_ALIGNMENT,

   iframe_PADDING_FPU,  // space only used if BIT[9] of iframe_PSR was set and FPU selected

} iframe_e;

typedef enum iframe_flag_e {
   iframe_flag_PSR_THUMB   = (1 << 24),
   iframe_flag_PSR_PADDING = (1 << 9),
} iframe_flag_e;


typedef enum iframe_len_e {
   // -- 1. select one of
   iframe_len_NOFPU = 0,   // (default) stack frame contains only first 8 CPU registers
   iframe_len_FPU   = 2,   // stack frame contains additional FPU registers

   // -- 2. select one of
   iframe_len_NOPADDING = 0,  // (default) no additional padding word at end of stack frame
   iframe_len_PADDING   = 1,  // there is an additional padding word at end of stack frame
} iframe_len_e;

#define iframe_LEN(/*iframe_len_e*/ FLAGS)  (((FLAGS)&2 ? iframe_PADDING_FPU : iframe_PADDING) + ((FLAGS)&1))
            // Length (number of registers) of interrupt stack frame with(isFPU=1) or without(isFPU=0) FPU
            // and with(isALIGN=1) or without(isALIGN=0) additional alignment.

/* == Objekte == */

/* no objects */

/* == Inline == */

static inline void assert_values_iframe()
{
         static_assert(iframe_R0  == 0);
         static_assert(iframe_R14 == 5);
         static_assert(iframe_LR  == 5);
         static_assert(iframe_PC  == 6);
         static_assert(iframe_PSR == 7);
         static_assert(iframe_PADDING == 8);
         static_assert(iframe_S0  == 8);
         static_assert(iframe_FPSCR == 24);
         static_assert(iframe_ALIGNMENT == 25);
         static_assert(iframe_PADDING_FPU == 26);
         static_assert(iframe_LEN(iframe_len_NOFPU|iframe_len_NOPADDING) == 8);
         static_assert(iframe_LEN(iframe_len_NOFPU|iframe_len_PADDING) == 9);
         static_assert(iframe_LEN(iframe_len_FPU|iframe_len_NOPADDING) == 26);
         static_assert(iframe_LEN(iframe_len_FPU|iframe_len_PADDING) == 27);
}

#endif
