/* title: Memory-Sync

   Implements memory barriers.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: sched.h
    Header file <Memory-Sync>.

   file: sched.c
    Implementation file <Memory-Sync impl>.
*/
#ifndef HW_CORTEXM4_MSYNC_HEADER
#define HW_CORTEXM4_MSYNC_HEADER

/* == Funktionen == */

#define  sw_msync()        __asm volatile ("":::"memory")
            // Do not allow reordering of software instructions before or after this barrier.

#define  read_msync()      __asm volatile ("":::"memory")
            // Same as <sw_msync> but also ensures that all outstanding read memory transactions
            // complete before subsequent memory transactions.

#define  rw_msync()        __asm volatile ("dmb":::"memory")
            // Same as <sw_msync> but also ensures that outstanding read/write memory transactions
            // complete before subsequent memory transactions.

#define  wait_msync()      __asm volatile ("dsb":::"memory")
            // Same as <rw_msync> but also ensures that all data transactions are
            // done before the next instruction is executed.

#endif
