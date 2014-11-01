#define _GNU_SOURCE
#include "cso.h"
#include "cso_struct.h"
#include <errno.h>
#include "cso_buffer.h"
#include <stdio.h>
#ifdef __linux
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#endif

#ifdef __linux
/* function: size_allocated_malloc
 * Uses GNU malloc_stats extension.
 * This functions returns internal collected statistics
 * about memory usage so implementing a thin wrapper
 * for malloc is not necessary.
 * This function may be missing on some platforms.
 * Currently it is only tested on Linux platforms.
 *
 * What malloc_stats does:
 * The GNU C lib function malloc_stats writes textual information
 * to standard err in the following form:
 * > Arena 0:
 * > system bytes     =     135168
 * > in use bytes     =      15000
 * > Total (incl. mmap):
 * > system bytes     =     135168
 * > in use bytes     =      15000
 * > max mmap regions =          0
 * > max mmap bytes   =          0
 *
 * How it is implemented:
 * This function redirects standard error file descriptor
 * to a pipe and reads the content of the pipe into a buffer.
 * It scans backwards until the third last line is
 * reached ("in use bytes") and then returns the converted
 * number at the end of the line as result.
 * */
int size_allocated_malloc(/*out*/size_t * number_of_allocated_bytes)
{
	int err ;
	int fd     = -1 ;
	int pfd[2] = { -1, -1 } ;

	if (pipe2(pfd, O_CLOEXEC|O_NONBLOCK)) {
		err = errno ;
		perror("pipe2") ;
		goto ONABORT ;
	}

	fd = dup(STDERR_FILENO) ;
	if (fd == -1) {
		err = errno ;
		perror("dup") ;
		goto ONABORT ;
	}

	if (-1 == dup2(pfd[1], STDERR_FILENO)) {
		err = errno ;
		perror("dup2") ;
		goto ONABORT ;
	}

	malloc_stats() ;

	char     buffer[256/*must be of even size*/] ;
	ssize_t  len    = 0 ;

	len = read(pfd[0], buffer, sizeof(buffer)) ;
	if (len < 0) {
		err = errno ;
		perror("read") ;
		goto ONABORT ;
	}

	while (sizeof(buffer) == len) {
		memcpy(buffer, &buffer[sizeof(buffer)/2], sizeof(buffer)/2) ;
		len = read(pfd[0], &buffer[sizeof(buffer)/2], sizeof(buffer)/2) ;
		if (len < 0) {
			err = errno ;
			if (err == EWOULDBLOCK || err == EAGAIN) {
				len = sizeof(buffer)/2 ;
				break ;
			}
			perror("read") ;
			goto ONABORT ;
		}
		len += (int)sizeof(buffer)/2 ;
	}

	/* remove last two lines */
	for (unsigned i = 3; len > 0; --len) {
		i -= (buffer[len] == '\n') ;
		if (!i) break ;
	}

	while (len > 0 && buffer[len-1] >= '0' && buffer[len-1] <= '9') {
		-- len ;
	}

	size_t used_bytes = 0 ;
	if (len > 0 && buffer[len] >= '0' && buffer[len] <= '9') {
		sscanf(&buffer[len], "%zu", &used_bytes) ;
	}

	if (-1 == dup2(fd, STDERR_FILENO)) {
		err = errno ;
		perror("dup2") ;
		goto ONABORT ;
	}

	(void) close(fd) ;
	(void) close(pfd[0]) ;
	(void) close(pfd[1]) ;

	*number_of_allocated_bytes = used_bytes ;

	return 0;
ONABORT:
	if (pfd[0] != -1) close(pfd[0]) ;
	if (pfd[1] != -1) close(pfd[1]) ;
	if (fd != -1) {
		dup2(fd, STDERR_FILENO) ;
		close(fd) ;
	}
	return err ;
}
#endif

#define RUN(UNITTEST) (run_single_unittest(#UNITTEST, &UNITTEST))

static int run_single_unittest(const char * name, int (*unittest_f) (void))
{
	size_t allocated_bytes = 0 ;

	int err = unittest_f() ;

#ifdef __linux
	if (size_allocated_malloc(&allocated_bytes)) {
		printf("ERROR in size_allocated_malloc\n") ;
	}
#endif

	printf("%s: ", name) ;

	if (err) {
		printf("FAILED\n") ;
	} else {
		printf("OK\n") ;
	}

#ifdef __linux
	// TEST all memory is freed
	{
		size_t allocated_bytes2 = 0 ;
		if (size_allocated_malloc(&allocated_bytes2)) {
			printf("ERROR in size_allocated_malloc\n") ;
		}
		allocated_bytes = allocated_bytes2 - allocated_bytes ;
	}
#endif

	if (!err && 0 != allocated_bytes) {
		printf("%s: MEMORY LEAK\n", name) ;
		err = EINVAL ;
	}

	return err ;
}

int main(void)
{
	RUN(unittest_cso_buffer) ;
	RUN(unittest_cso) ;
	RUN(unittest_cso_struct) ;
	return 0 ;
}
