#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EINVARIANT 1024

typedef void* IDNAME;
typedef void* TYPENAME;
typedef uint32_t char32_t;

#define TEST(CONDITION) \
         if ( !(CONDITION) ) {      \
            dprintf(STDERR_FILENO,  \
            "%s:%d: TEST FAILED\n", \
            __FILE__, __LINE__); \
            goto ONERR;          \
         }

#define TESTP(CONDITION, FORMAT, ...) \
         if ( !(CONDITION) ) {      \
            dprintf(STDERR_FILENO,  \
            "%s:%d: TEST FAILED\n" FORMAT "\n", \
            __FILE__, __LINE__, __VA_ARGS__);   \
            goto ONERR;             \
         }                          \

#define static_assert(C,S) \
         ((void)(sizeof(char[(C)?1:-1])))

#define TRACEEXIT_ERRLOG(err) \
         dprintf(STDERR_FILENO,   \
            "%s:%d: %s(): Exit function with error (err=%d)\n", \
            __FILE__, __LINE__, __FUNCTION__, err);

#define TRACEEXITFREE_ERRLOG(err) \
         dprintf(STDERR_FILENO,   \
            "%s:%d: %s(): Exit function with error (err=%d)." \
            " Some resources could not be freed.\n", \
            __FILE__, __LINE__, __FUNCTION__, err);

#define VALIDATE_INPARAM_TEST(test, label, log) \
         if (!(test)) { \
            dprintf(STDERR_FILENO, \
               "%s:%d: %s(): Wrong input arguments\n", \
               __FILE__, __LINE__, __FUNCTION__); \
            err = EINVAL; \
            goto label; \
         }

#define ispowerof2_int(i) (0 == ((i)&((i)-1)))

#define lengthof(a) (sizeof(a) / sizeof((a)[0]))
#define bitsof(a) (sizeof(a) * 8/*assume CHAR_BIT == 8*/)
