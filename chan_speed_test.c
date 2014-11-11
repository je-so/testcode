// Measure raw speed of message transfer of 1000000 raw pointer
#include "src/chan.h"
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>

#define MAXTHREAD 16

static chan_t* s_queue;
static struct timeval starttime[MAXTHREAD];
static struct timeval endtime[MAXTHREAD];

void* server(void* id)
{
   void* msg = 0;
   gettimeofday(&starttime[(int)id], 0);
   for (int i = 0; i < 1000000; ++i) {
       chan_recv(s_queue, &msg);
   }
   return 0;
}

void* client(void* id)
{
   for (uintptr_t i = 0; i < 1000000; ++i) {
      chan_send(s_queue, (void*)i);
   }
   gettimeofday(&endtime[(int)id], 0);
   return 0;
}

void measure(int nrthread)
{
   pthread_t cthr[MAXTHREAD], sthr[MAXTHREAD];

   s_queue = chan_init(10000);

   for (int i = 0; i < nrthread; ++i) {
      pthread_create(&sthr[i], 0, &server, (void*)i);
      pthread_create(&cthr[i], 0, &client, (void*)i);
   }
   for (int i = 0; i < nrthread; ++i) {
      pthread_join(cthr[i], 0);
      pthread_join(sthr[i], 0);
   }

   for (int i = 1; i < nrthread; ++i) {
      if (starttime[0].tv_sec > starttime[i].tv_sec || (starttime[0].tv_sec == starttime[i].tv_sec && starttime[0].tv_usec > starttime[i].tv_usec)) {
         starttime[0] = starttime[i];
      }
   }
   for (int i = 1; i < nrthread; ++i) {
      if (endtime[0].tv_sec < endtime[i].tv_sec || (endtime[0].tv_sec == endtime[i].tv_sec && endtime[0].tv_usec < endtime[i].tv_usec)) {
          endtime[0] = endtime[i];
      }
   }

   time_t sec = endtime[0].tv_sec - starttime[0].tv_sec;
   long usec = endtime[0].tv_usec - starttime[0].tv_usec;
   long msec = 1000 * (long)sec + usec / 1000;
   printf("chan_t: %d*1000000 send/recv time in ms: %ld (%ld nr_of_msg/msec)\n", nrthread, msec, nrthread*1000000/msec);

   chan_dispose(s_queue);
}

int main(void)
{
   for (int nrthread = 1; nrthread <= MAXTHREAD; nrthread *= 2) {
      measure(nrthread);
   }
   return 0;
 }
