/* title: Fifo

   Implements managing state of a single task and
   remembers the current running task.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: fifo.h
    Header file <Fifo>.

   file: fifo.c
    Implementation file <Fifo impl>.
*/
#ifndef JRTOS_FIFO_HEADER
#define JRTOS_FIFO_HEADER

/* == Import == */
#define FIFO_GENERIC_SIZE  32
#define FIFO_GENERIC_TYPE  void*
#define FIFO_GENERIC_NAME  fifo_t
#define FIFO_GENERIC_SUFFIX fifo

/* == Generic == */
#define SIZE   (FIFO_GENERIC_SIZE)
#define type_t FIFO_GENERIC_TYPE
#define fifo_t FIFO_GENERIC_NAME
#define APPEND__(name, suffix)   name ## _ ## suffix
#define APPEND_(name, suffix)    APPEND__(name, suffix)
#define FUNC(name)               APPEND_(name, FIFO_GENERIC_SUFFIX)

/* == Typen == */
typedef struct fifo_t fifo_t;

/* == Globals == */

/* == Objekte == */

struct fifo_t {
   type_t   data[SIZE];
   volatile uint16_t rpos;
   volatile uint16_t wpos;
};

// fifo_t: test

static inline void FUNC(assert_param) (void);

#ifdef KONFIG_UNITTEST
int unittest_jrtos_fifo(void);
#endif

// fifo_t: lifetime

#define fifo_INIT { .rpos = 0, .wpos = 0 }
            // Static initializer fifo_INIT, could be used instead of init_fifo.

static inline void FUNC(init) (fifo_t *fifo);
            // init_fifo initializes read and write position to 0.

// fifo_t: update

static inline int FUNC(put) (fifo_t *fifo, type_t value);

static inline int FUNC(get) (fifo_t *fifo, /*out*/type_t *value);




/* == Inline == */

static inline void FUNC(assert_param) (void)
{
   static_assert(8 <= SIZE && "Minimum value of FIFO_GENERIC_SIZE");
   static_assert(32768 >= SIZE && "Maximum value of FIFO_GENERIC_SIZE");
   static_assert(0 == (SIZE&(SIZE-1)) && "Value of FIFO_GENERIC_SIZE should be power of 2");
}

static inline void FUNC(init) (fifo_t *fifo)
{
   fifo->rpos = 0;
   fifo->wpos = 0;
}

static inline int FUNC(put) (fifo_t *fifo, type_t value)
{
   uint16_t rpos = (uint16_t) (fifo->rpos + SIZE);
   uint16_t wpos = fifo->wpos;
   if (wpos == rpos) {
      return ENOMEM;
   }
   fifo->data[wpos & (SIZE-1)] = value;
   sw_msync();
   fifo->wpos = (uint16_t) (wpos+1);
   return 0;
}

static inline int FUNC(get) (fifo_t *fifo, /*out*/type_t *value)
{
   uint16_t rpos = fifo->rpos;
   uint16_t wpos = fifo->wpos;
   if (rpos == wpos) {
      return ENODATA;
   }
   *value = fifo->data[rpos & (SIZE-1)];
   sw_msync();
   fifo->rpos = (uint16_t) (rpos+1);
   return 0;
}

#undef APPEND__
#undef APPEND_
#undef FUNC
#undef SIZE
#undef type_t
#undef fifo_t

#endif
