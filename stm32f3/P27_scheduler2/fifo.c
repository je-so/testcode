/* title: Test-Fifo impl

   Implementation of interface <Test-Mailbox>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: fifo.h
    Header file <Test-Fifo>.

   file: fifo.c
    Implementation file <Test-Fifo impl>.
*/
#include "konfig.h"
#include "fifo.h"
#include "task.h"

static inline void doput_fifo(fifo_t *fifo, uint32_t value)
{
   fifo->buffer[fifo->wpos] = value;
   fifo->wpos = (fifo->wpos + 1 == fifo->size) ? 0 : fifo->wpos + 1;
   signal_semaphore(&fifo->receiver);
}

static inline uint32_t doget_fifo(fifo_t *fifo)
{
   uint32_t value = fifo->buffer[fifo->rpos];
   fifo->rpos = (fifo->rpos + 1 == fifo->size) ? 0 : fifo->rpos + 1;
   signal_semaphore(&fifo->sender);
   return value;
}

void put_fifo(fifo_t *fifo, uint32_t value)
{
   wait_semaphore(&fifo->sender);
   while (0 != trylock_atomic(&fifo->lock)) ;
   doput_fifo(fifo, value);
   unlock_atomic(&fifo->lock);
}

uint32_t get_fifo(fifo_t *fifo)
{
   wait_semaphore(&fifo->receiver);
   while (0 != trylock_atomic(&fifo->lock)) ;
   uint32_t value = doget_fifo(fifo);
   unlock_atomic(&fifo->lock);
   return value;
}

int tryput_fifo(fifo_t *fifo, uint32_t value)
{
   int err = EAGAIN;
   if (0 == trylock_atomic(&fifo->lock)) {
      if (0 <= trywait_semaphore(&fifo->sender)) {
         doput_fifo(fifo, value);
         err = 0;
      }
      unlock_atomic(&fifo->lock);
   }
   return err;
}

int tryget_fifo(fifo_t *fifo, /*out*/uint32_t *value)
{
   int err = EAGAIN;
   if (0 == trylock_atomic(&fifo->lock)) {
      if (0 <= trywait_semaphore(&fifo->receiver)) {
         *value = doget_fifo(fifo);
         err = 0;
      }
      unlock_atomic(&fifo->lock);
   }
   return err;
}
