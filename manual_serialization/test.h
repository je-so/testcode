#ifndef CSO_TEST_HEADER
#define CSO_TEST_HEADER

#define CONCAT(A,B)	CONCAT_(A,B)
#define CONCAT_(A,B)	A ## B

#define TEST(CONDITION) \
	if ( !(CONDITION) ) { \
		printf("%s:%d: TEST FAILED\n", __FILE__, __LINE__) ; \
		goto ONABORT ; \
	}

#define static_assert(C,S) \
	{ \
		int CONCAT(_extern_static_assert,__LINE__)[(C)?1:-1] ; \
		(void)CONCAT(_extern_static_assert,__LINE__) ; \
	}

#endif
