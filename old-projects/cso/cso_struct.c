#define _GNU_SOURCE
#include "cso_struct.h"
#include "cso_buffer.h"
#include <endian.h>
#include <stdio.h>
#include <string.h>
#ifdef CONFIG_UNITTEST
#include "test.h"
#endif

static inline uint8_t cso_struct_get_endian(const uint8_t * data)
{
	return data[4] ;
}

int cso_struct_get_typeid(size_t size, const uint8_t data[size], /*out*/uint8_t id[3])
{
	if (size < 4) return EINVAL ;
	memcpy(id, data, 3) ;
	return 0 ;
}

// TODO: implement cso_struct_encode
int cso_struct_encode(const cso_struct_t * type, struct cso_buffer_t * dest/*msg-buffer*/,  const void * src/*struct-data*/) ;

// TODO: implement cso_struct_decode
int cso_struct_decode(const cso_struct_t * type, void * dest/*struct-data*/, size_t size, const uint8_t src[size]/*msg-buffer*/) ;


#ifdef CONFIG_UNITTEST

static int test_query(void)
{
	uint8_t data[1024] ;
	uint8_t id[3] ;

	// TEST cso_struct_get_endian
	for (uint8_t i = 0; i < 10; ++i) {
		data[4] = i ;
		TEST(i == cso_struct_get_endian(data)) ;
	}

	// TEST cso_struct_get_typeid
	memcpy(data, "\x01\x02\x03", 3) ;
	memset(id, 0, sizeof(id)) ;
	TEST(0 == cso_struct_get_typeid(4, data, id)) ;
	TEST(0 == memcmp(id, "\x01\x02\x03", sizeof(id))) ;

	// TEST cso_struct_get_typeid: EINVAL
	for (unsigned i = 0; i < 4; ++i) {
		TEST(EINVAL == cso_struct_get_typeid(i, data, id)) ;
	}

	return 0 ;
ONABORT:
	return EINVAL ;
}

static int test_encode(void)
{
	// TEST cso_struct_encode
	// TODO: implement test cso_struct_encode

	return 0 ;
}

static int test_decode(void)
{
	// TEST cso_struct_decode
	// TODO: implement test cso_struct_decode

	return 0 ;
}

int unittest_cso_struct(void)
{
	static_assert(8 == CHAR_BIT, "assume char holds exactly 8 bits") ;
	static_assert(8 == sizeof(double), "same size as uint64_t") ;

	if (test_query())  goto ONABORT ;
	if (test_encode()) goto ONABORT ;
	if (test_decode()) goto ONABORT ;

	return 0 ;
ONABORT:
	return EINVAL ;
}

#endif
