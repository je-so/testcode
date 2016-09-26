/* title: Test-Fifo

   Implements simple data structure for communicating
   a multiple values of type uintptr_t (==uint32_t) between
   many threads.

   The number of elements storeable in the fifo can be configured
   at compile time.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: fifo.h
    Header file <Test-Fifo>.

   file: fifo.c
    Implementation file <Test-Fifo impl>.
*/
#ifndef CORTEXM4_FIFO_HEADER
#define CORTEXM4_FIFO_HEADER

#include "semaphore.h"

// == exported constants

#ifndef NOERROR
#define NOERROR   0
#endif

#ifndef ERRFULL
#define ERRFULL   1024
#endif

// == exported objects

typedef struct fifo_t {
   semaphore_t sender;
   semaphore_t receiver;
   volatile uint32_t lock;
   uintptr_t  *buffer;
   uint32_t    size;       // nr of elements which could be stored in buffer
   uint32_t    wpos;       // index into buffer to write next element to
   uint32_t    rpos;       // index into buffer to read next element from
} fifo_t;

// == lifetime
#define fifo_INIT(size, buffer) \
         { semaphore_INIT(size), semaphore_INIT(0), 0, buffer, size, 0, 0 }

// == data-exchange
void put_fifo(fifo_t *fifo, uintptr_t value);
         //
uintptr_t get_fifo(fifo_t *fifo);
         //
int tryput_fifo(fifo_t *fifo, uintptr_t value);
         // returns 0: Value written into fifo. EAGAIN: Nothing is written and fifo was not changed. Either fifo is locked or full.
int tryget_fifo(fifo_t *fifo, /*out*/uintptr_t *value);
         // returns 0: Next element from fifo read into value. EAGAIN: Nothing is read and value was not changed. Either fifo is locked or does not contain any unread elements.

#endif
