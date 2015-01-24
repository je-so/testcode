#ifndef CSO_BUFFER_HEADER
#define CSO_BUFFER_HEADER

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct cso_buffer_t cso_buffer_t ;

#ifdef CONFIG_UNITTEST
int unittest_cso_buffer(void) ;
#endif

struct cso_buffer_t {
	uint8_t   * next ;       /* next free memory address after last written bytes */
	uint8_t   * end ;        /* end of allocated memory block (address of last byte +1) */
	uint8_t   * start ;      /* start (lowest address) of allocated memory block */
} ;

// group: lifetime

static void cso_buffer_init(/*out*/cso_buffer_t * buf) ;

static void cso_buffer_free(cso_buffer_t * buf) ;

// group: query

static size_t cso_buffer_sizeallocated(const cso_buffer_t * buf) ;

static size_t cso_buffer_sizeused(const cso_buffer_t * buf) ;

static size_t cso_buffer_sizefree(const cso_buffer_t * buf) ;

// group: allocate

static int cso_buffer_expand(cso_buffer_t * buf, size_t size_increment) ;


// section: inline implementation

static inline void cso_buffer_init(/*out*/cso_buffer_t * buf)
{
	*buf = (cso_buffer_t) { 0, 0, 0 } ;
}

static inline void cso_buffer_free(cso_buffer_t * buf)
{
	free(buf->start) ;
	*buf = (cso_buffer_t) { 0, 0, 0 } ;
}

static inline size_t cso_buffer_sizeallocated(const cso_buffer_t * buf)
{
	return (size_t) (buf->end - buf->start) ;
}

static inline size_t cso_buffer_sizeused(const cso_buffer_t * buf)
{
	return (size_t) (buf->next - buf->start) ;
}

static inline size_t cso_buffer_sizefree(const cso_buffer_t * buf)
{
	return (size_t) (buf->end - buf->next) ;
}

static inline int cso_buffer_expand(cso_buffer_t * buf, size_t size_increment)
{
	size_t  size    = cso_buffer_sizeallocated(buf) ;
	size_t  newsize = size + size_increment ;

	if (UINT32_MAX-size <= size_increment) return E2BIG ;

	// at least double the size
	if (newsize < 2*size) newsize = 2*size ;

	size_t  sizeused   = cso_buffer_sizeused(buf) ;
	uint8_t * newstart = realloc(buf->start, newsize) ;

	if (!newstart) return ENOMEM ;

	buf->next  = newstart + sizeused ;
	buf->end   = newstart + newsize ;
	buf->start = newstart ;

	return 0 ;
}

#endif

