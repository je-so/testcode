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


#if 0

// TODO: integrate alternate implementation with buffer management !!

typedef struct fifo2_t {
   uint8_t rpos;
   uint8_t wpos;
   uint8_t szm1;
   uint32_t data[];
} fifo2_t;

#define FIFO2_DECLARE(size) \
         union { \
            fifo2_t  obj; \
            uint32_t buffer[sizeof(fifo2_t)/sizeof(uint32_t) + size]; \
         }

#define FIFO2_SIZE(fifo) ((uint8_t)((sizeof(fifo.buffer)-sizeof(fifo2_t))/sizeof(uint32_t)))

static inline int fifo2_init(fifo2_t *fifo, uint8_t size/*>=1*/)
{
	if (size == 0 || (size&(size-1)) != 0) return -1/*INVALID PARAMETER*/;
	fifo->rpos = 0;
	fifo->wpos = 0;
	fifo->szm1 = (uint8_t) (size-1);
	return 0;
}

static inline int fifo2_get(fifo2_t *fifo, uint32_t *data)
{
	if (fifo->rpos == fifo->wpos) return -1/*NO-DATA*/;
	*data = fifo->data[fifo->rpos++ & fifo->szm1];
	return 0/*OK*/;
}

static inline int fifo2_put(fifo2_t *fifo, uint32_t data)
{
	if ((uint8_t) (fifo->wpos - fifo->rpos) > fifo->szm1) return -1/*OUT-OF-BUFFER*/;
	fifo->data[fifo->wpos++ & fifo->szm1] = data;
	return 0/*OK*/;
}
#endif

/* == Typen == */
typedef struct fifo_t fifo_t;

/* == Globals == */

/* == Objekte == */

struct fifo_t {
   volatile uint16_t rpos;
   volatile uint16_t wpos;
   volatile uint16_t szm1;
   type_t   data[SIZE];
};

// fifo_t: test

#ifdef KONFIG_UNITTEST
int unittest_jrtos_fifo(void);
#endif

// fifo_t: lifetime

#define fifo_INIT { .rpos = 0, .wpos = 0, .szm1 = ((FIFO_GENERIC_SIZE)-1) }
            // Static initializer fifo_INIT, could be used instead of init_fifo.

static inline void FUNC(init) (fifo_t *fifo);
            // init_fifo initializes read and write position to 0.

// fifo_t: update

static inline int FUNC(put) (fifo_t *fifo, type_t value);

static inline int FUNC(get) (fifo_t *fifo, /*out*/type_t *value);




/* == Inline == */

static inline void FUNC(init) (fifo_t *fifo)
{
   static_assert(8 <= SIZE && "Minimum value of FIFO_GENERIC_SIZE");
   static_assert(32768 >= SIZE && "Maximum value of FIFO_GENERIC_SIZE");
   static_assert(0 == (SIZE&(SIZE-1)) && "Value of FIFO_GENERIC_SIZE should be power of 2");
   fifo->rpos = 0;
   fifo->wpos = 0;
   fifo->szm1 = (SIZE-1);
}

static inline int FUNC(put) (fifo_t *fifo, type_t value)
{
   uint16_t rpos = fifo->rpos;
   uint16_t wpos = fifo->wpos;
   if ((uint16_t) (wpos - rpos) > fifo->szm1) {
      return ENOMEM;
   }
   fifo->data[wpos & fifo->szm1] = value;
   rw_msync();
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
   *value = fifo->data[rpos & fifo->szm1];
   rw_msync();
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
