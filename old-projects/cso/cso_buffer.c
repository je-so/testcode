#define _GNU_SOURCE
#include "cso_buffer.h"
#include <stdio.h>
#ifdef CONFIG_UNITTEST
#include "test.h"
#endif


#ifdef CONFIG_UNITTEST

static int test_lifetime(void)
{
	cso_buffer_t buf ;

	// TEST cso_buffer_init
	cso_buffer_init(&buf) ;
	TEST(0 == buf.next) ;
	TEST(0 == buf.end) ;
	TEST(0 == buf.start) ;

	// TEST cso_buffer_free
	buf.start = malloc(1000) ;
	TEST(0 != buf.start) ;
	buf.next = buf.start ;
	buf.end  = buf.start + 1000 ;
	cso_buffer_free(&buf) ;
	TEST(0 == buf.next) ;
	TEST(0 == buf.end) ;
	TEST(0 == buf.start) ;
	cso_buffer_free(&buf) ;
	TEST(0 == buf.next) ;
	TEST(0 == buf.end) ;
	TEST(0 == buf.start) ;

	return 0 ;
ONABORT:
	return EINVAL ;
}

static int test_query(void)
{
	cso_buffer_t buf ;

	// prepare
	buf.start = malloc(1000) ;
	TEST(0 != buf.start) ;
	buf.next = buf.start ;
	buf.end  = buf.start + 1000 ;

	// TEST cso_buffer_sizeallocated
	for (unsigned i = 0; i <= 1000; ++i) {
		buf.end = buf.start + i ;
		TEST(i == cso_buffer_sizeallocated(&buf)) ;
	}

	// TEST cso_buffer_sizeused
	for (unsigned i = 0; i <= 1000; ++i) {
		buf.next = buf.start + i ;
		TEST(i == cso_buffer_sizeused(&buf)) ;
	}

	// TEST cso_buffer_sizefree
	for (unsigned i = 0; i <= 1000; ++i) {
		buf.next = buf.start + 1000 - i ;
		TEST(i == cso_buffer_sizefree(&buf)) ;
	}

	return 0 ;
ONABORT:
	return EINVAL ;
}

static int test_allocate(void)
{
	cso_buffer_t buf ;

	// prepare
	cso_buffer_init(&buf) ;

	// TEST cso_buffer_expand: empty buffer
	TEST(0 == cso_buffer_expand(&buf, 10)) ;
	TEST(0 != buf.start) ;
	TEST(10 == cso_buffer_sizeallocated(&buf)) ;
	TEST(10 == cso_buffer_sizefree(&buf)) ;
	TEST(0  == cso_buffer_sizeused(&buf)) ;

	// TEST cso_buffer_expand: double size
	TEST(0 == cso_buffer_expand(&buf, 5)) ;
	TEST(0 != buf.start) ;
	TEST(20 == cso_buffer_sizeallocated(&buf)) ;
	TEST(20 == cso_buffer_sizefree(&buf)) ;
	TEST(0  == cso_buffer_sizeused(&buf)) ;

	// TEST cso_buffer_expand: keep sizeused
	buf.next = buf.start + 11 ;
	TEST(11 == cso_buffer_sizeused(&buf)) ;
	TEST(0 == cso_buffer_expand(&buf, 1)) ;
	TEST(40 == cso_buffer_sizeallocated(&buf)) ;
	TEST(29 == cso_buffer_sizefree(&buf)) ;
	TEST(11 == cso_buffer_sizeused(&buf)) ;

	// TEST cso_buffer_expand: keep content
	void * dummy = malloc(40) ;
	buf.next = buf.start + 40 ;
	for (unsigned i = 0; i < 40; ++i) {
		buf.start[i] = (uint8_t)i ;
	}
	TEST(0 == cso_buffer_expand(&buf, 60)) ;
	TEST(100 == cso_buffer_sizeallocated(&buf)) ;
	TEST(60 == cso_buffer_sizefree(&buf)) ;
	TEST(40 == cso_buffer_sizeused(&buf)) ;
	for (unsigned i = 0; i < 40; ++i) {
		TEST(i == buf.start[i]) ;
	}
	free(dummy) ;

	// TEST cso_buffer_expand: E2BIG
	void * oldstart = buf.start ;
	TEST(E2BIG == cso_buffer_expand(&buf, UINT32_MAX-cso_buffer_sizeallocated(&buf))) ;
	TEST(oldstart == buf.start) ;
	TEST(100 == cso_buffer_sizeallocated(&buf)) ;
	TEST(60 == cso_buffer_sizefree(&buf)) ;
	TEST(40 == cso_buffer_sizeused(&buf)) ;
	for (unsigned i = 0; i < 40; ++i) {
		TEST(i == buf.start[i]) ;
	}

	return 0 ;
ONABORT:
	return EINVAL ;
}

int unittest_cso_buffer(void)
{
	if (test_lifetime()) goto ONABORT ;
	if (test_query())    goto ONABORT ;
	if (test_allocate()) goto ONABORT ;

	return 0 ;
ONABORT:
	return EINVAL ;
}

#endif
