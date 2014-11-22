#ifndef IPERF_HEADER
#define IPERF_HEADER

// === platform specific configurations
#define _GNU_SOURCE
// ===

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef struct iperf_param_t {
   int    tid; // threadid or processid of test instance (0,1,2,...)
   int    isthread; // 0: started as process; 1: started as thread
   int    nrops; // initially set to 1; should be overwritten by prepare
                 // reflects the number of performaned "operations"
   void*  addr;  // initially set to 0; can be overwritten by prepare/run
   size_t size;  // initially set to 0; can be overwritten by prepare/run
} iperf_param_t;

// init test run (time is not measured)
// should return 0 in case of success else error code != 0
// in case of error testrun is aborted
int iperf_prepare(iperf_param_t* param);

// run prepared test (time is measured)
// should return 0 in case of success else error code != 0
// in case of error testrun is aborted
int iperf_run(iperf_param_t* param);

#endif
