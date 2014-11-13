// Proof of Concept Go-Routines + Go-Channels
// Compile with: gcc -O3 -std=gnu99 -ogochan gochan.c -lpthread
// Measures time of msg transfers between 3 clients and 3 servers in one system thread
// The number of system threads is doubled in every loop (up to 128 system threads).
// Every client or server is mapped to a single gofunc_t.
// Result on my (1-2GHZ) machine:
// (for 1 system thread): gochan: 1*30000 send/recv time in ms: 1 (30000 msg/msec)
// (for 32 system threads): gochan: 32*30000 send/recv time in ms: 51 (18823 msg/msec)
// (for 64 system threads): gochan: 64*30000 send/recv time in ms: 122 (15737 msg/msec)
// (for 128 system threads): gochan: 128*30000 send/recv time in ms: 156 (24615 msg/msec)

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

struct goexec_t;
struct gofunc_t;
struct gofunc_param_t;

typedef struct gofunc_param_t {
   int threadid;
   struct goexec_t* goexec;
   struct gofunc_t* gofunc;
} gofunc_param_t;

typedef struct goexec_thread_t {
   gofunc_param_t param;
   pthread_t      thr;
   struct gofunc_t* funclist_tail;
} goexec_thread_t;

typedef struct goexec_t {
   int nrthreads;
   volatile int isstop;
   volatile int nrready;
   pthread_mutex_t lock;
   pthread_cond_t run;
   pthread_cond_t done;
   goexec_thread_t threads[/*nrthreads*/];
} goexec_t;

typedef struct gofunc_t {
   void (* fct) (struct gofunc_param_t* goparam);
   void* state; // stores state of local variables of fct
   void* continue_label;
   void* gochan_msg;
   struct gofunc_t* waitlist_next;
   struct gofunc_t* funclist_next;
   struct gofunc_t* funclist_prev;
} gofunc_t;

typedef struct gochan_t {
   struct goexec_t* goexec;
   struct gofunc_t* waitlist_tail[/*nrthreads*/];
} gochan_t;

// === implement gofunc_t ===

#define gofunc_start(goparam) \
         if ((goparam)->gofunc->continue_label) { \
            goto *(goparam)->gofunc->continue_label; \
         }

#define gofunc_end(goparam) \
         goexec_delfunc(goparam)

// === implement goexecutor_t ===

void goexec_delfunc(gofunc_param_t* goparam)
{
   int tid = goparam->threadid;

   if (goparam->goexec->threads[tid].funclist_tail == goparam->gofunc) {
      if (goparam->gofunc == goparam->gofunc->funclist_prev) {
         goparam->goexec->threads[tid].funclist_tail = 0;
      } else {
         goparam->goexec->threads[tid].funclist_tail = goparam->gofunc->funclist_prev;
      }
   } 

   goparam->gofunc->funclist_next->funclist_prev = goparam->gofunc->funclist_prev;
   goparam->gofunc->funclist_prev->funclist_next = goparam->gofunc->funclist_next;
   goparam->gofunc = 0;

   free(goparam->gofunc);
}

int goexec_newfunc(goexec_t* goexec, int threadid, void (* fct) (gofunc_param_t* goparam))
{
   if (threadid < 0 || threadid >= goexec->nrthreads) {
      return EINVAL;
   }
   gofunc_t* gofunc = malloc(sizeof(gofunc_t));
   if (!gofunc) {
      return ENOMEM;
   }
   gofunc->state = 0;
   gofunc->gochan_msg = 0;
   gofunc->continue_label = 0;
   gofunc->fct = fct;
   gofunc->waitlist_next = 0;
   if (goexec->threads[threadid].funclist_tail) {
      gofunc->funclist_next = goexec->threads[threadid].funclist_tail->funclist_next;
      gofunc->funclist_next->funclist_prev = gofunc;
      gofunc->funclist_prev = goexec->threads[threadid].funclist_tail;
      gofunc->funclist_prev->funclist_next = gofunc;
   } else {
      gofunc->funclist_next = gofunc;
      gofunc->funclist_prev = gofunc;
   }
   goexec->threads[threadid].funclist_tail = gofunc;
   return 0;
}

void* goexec_threadmain(void* param)
{
   gofunc_param_t* goparam = param;
   goexec_t* goexec = goparam->goexec;
   int tid = goparam->threadid;

   //printf("Start id:%d exec:%p\n", tid, goparam->goexec);

   while (! goexec->isstop) {
      if (tid) {
         pthread_mutex_lock(&goexec->lock);
         ++ goparam->goexec->nrready;
         pthread_cond_signal(&goexec->done);
         pthread_cond_wait(&goexec->run, &goexec->lock);
         pthread_mutex_unlock(&goexec->lock);
      }
      // printf("Run id:%d %p\n", tid, goexec->threads[tid].funclist_tail);
      while (goexec->threads[tid].funclist_tail) {
         gofunc_t* next = goexec->threads[tid].funclist_tail->funclist_next;
         for (gofunc_t* gofunc; (gofunc = next); ) {
            next = (next == goexec->threads[tid].funclist_tail) ? 0 : next->funclist_next;
            goparam->gofunc = gofunc;
            gofunc->fct(goparam);
         }
      }
      if (tid == 0) break;
   }

   //printf("End id:%d exec:%p\n", tid, goparam->goexec);
}

void goexec_run(goexec_t* goexec)
{
   pthread_mutex_lock(&goexec->lock);
   goexec->nrready = 0;
   pthread_cond_broadcast(&goexec->run);
   pthread_mutex_unlock(&goexec->lock);
   goexec_threadmain(&goexec->threads[0].param);
   for (int isdone = 0; !isdone;) {
      pthread_mutex_lock(&goexec->lock);
      isdone = (goexec->nrready == goexec->nrthreads-1);
      if (!isdone) {
         pthread_cond_wait(&goexec->done, &goexec->lock);
      }
      pthread_mutex_unlock(&goexec->lock);
   }
}

goexec_t* goexec_new(int nrthreads)
{
   if (nrthreads <= 0 || nrthreads > 256) {
      return 0; /*EINVAL*/
   }

   size_t arraysize = sizeof(goexec_thread_t) * nrthreads;
   goexec_t* goexec = malloc(sizeof(goexec_t) + arraysize);
   if (!goexec) {
      return 0; /*ENOMEM*/
   }

   goexec->nrthreads = nrthreads;
   goexec->isstop = 0;
   goexec->nrready = 0;
   pthread_mutex_init(&goexec->lock, 0);
   pthread_cond_init(&goexec->run, 0);
   pthread_cond_init(&goexec->done, 0);
   memset(goexec->threads, 0, arraysize);
   for (int i = 0; i < nrthreads; ++i) {
      goexec->threads[i].param.threadid = i;
      goexec->threads[i].param.goexec = goexec;
      if (i) {
         int err = pthread_create(&goexec->threads[i].thr, 0, &goexec_threadmain, &goexec->threads[i].param);
         assert(err == 0);
      }
   }

   for (int isdone = 0; !isdone;) {
      pthread_mutex_lock(&goexec->lock);
      isdone = (goexec->nrready == goexec->nrthreads-1);
      if (!isdone) {
         pthread_cond_wait(&goexec->done, &goexec->lock);
      }
      pthread_mutex_unlock(&goexec->lock);
   }

   return goexec;
}

void goexec_delete(goexec_t** goexec)
{
   if (! *goexec) return ;
   pthread_mutex_lock(&(*goexec)->lock);
   (*goexec)->isstop = 1;
   pthread_cond_broadcast(&(*goexec)->run);
   pthread_mutex_unlock(&(*goexec)->lock);
   for (int i = 1; i < (*goexec)->nrthreads; ++i) {
      pthread_join((*goexec)->threads[i].thr, 0);
   }
   free(*goexec);
   *goexec = 0;
}

// === implement gochan_t ===

gochan_t* gochan_new(goexec_t* goexec)
{
   size_t arraysize = sizeof(gofunc_t*) * goexec->nrthreads;
   gochan_t* gochan = malloc(sizeof(gochan_t) + arraysize);
   if (!gochan) {
      return 0; /*ENOMEM*/
   }
   
   gochan->goexec = goexec;
   memset(gochan->waitlist_tail, 0, arraysize);

   return gochan;
}

void gochan_delete(gochan_t** gochan)
{
   free(*gochan);
   *gochan = 0;
}

int _gochan_send(gochan_t* gochan, gofunc_param_t* goparam)
{
   int tid = goparam->threadid;
   if (! gochan->waitlist_tail[tid]) return EAGAIN;

   gofunc_t* reader = gochan->waitlist_tail[tid]->waitlist_next;

   if (reader->waitlist_next == reader) {
      gochan->waitlist_tail[tid] = 0;
   } else {
      gochan->waitlist_tail[tid] = reader->waitlist_next;
   }
   reader->waitlist_next = 0;

   reader->gochan_msg = goparam->gofunc->gochan_msg;

   return 0;
}

void gochan_addwaitlist(gochan_t* gochan, gofunc_param_t* goparam)
{
   int tid = goparam->threadid;
   if (! goparam->gofunc->waitlist_next) {
      if (gochan->waitlist_tail[tid]) {
         goparam->gofunc->waitlist_next = gochan->waitlist_tail[tid]->waitlist_next;
         gochan->waitlist_tail[tid]->waitlist_next = goparam->gofunc;
      } else {
         goparam->gofunc->waitlist_next = goparam->gofunc;
      }
      gochan->waitlist_tail[tid] = goparam->gofunc;
   }
}

int _gochan_recv(gochan_t* gochan, gofunc_param_t* goparam, void** msg)
{
   if (goparam->gofunc->waitlist_next) return EAGAIN;

   *msg = goparam->gofunc->gochan_msg;

   return 0;
}

#define gochan_send(gochan, goparam, msg) \
         do { \
            __label__ SEND_CONTINUE; \
            (goparam)->gofunc->continue_label = &&SEND_CONTINUE; \
            (goparam)->gofunc->gochan_msg = (msg); \
            SEND_CONTINUE: \
            if (_gochan_send(gochan, goparam)) return; \
         } while (0)

#define gochan_recv(gochan, goparam, msg) \
         do { \
            __label__ RECV_CONTINUE; \
            (goparam)->gofunc->continue_label = &&RECV_CONTINUE; \
            gochan_addwaitlist(gochan, goparam); \
            return; \
            RECV_CONTINUE: \
            if (_gochan_recv(gochan, goparam, msg)) return; \
         } while (0)

// ===== client/server test =====

gochan_t* gochan;
int msgcount[16];

void server(gofunc_param_t* goparam)
{
   gofunc_start(goparam);

   for (goparam->gofunc->state = 0; (int)goparam->gofunc->state < 50000; goparam->gofunc->state = (void*) (1 + (int)goparam->gofunc->state)) {
      //printf("try recv %d\n", (int)goparam->gofunc->state);
      void* msg;
      gochan_recv(gochan, goparam, &msg);
      //printf("received = %d\n", (int)msg);
      assert(msg == goparam->gofunc->state);
      //++msgcount[goparam->threadid];
   }

   gofunc_end(goparam);
}

void client(gofunc_param_t* goparam)
{
   gofunc_start(goparam);

   for (goparam->gofunc->state = 0; (int)goparam->gofunc->state < 50000; goparam->gofunc->state = (void*) (1 + (int)goparam->gofunc->state)) {
      //printf("try send %d\n", (int)goparam->gofunc->state);
      gochan_send(gochan, goparam, goparam->gofunc->state);
      //printf("sended %d\n", (int)goparam->gofunc->state);
      //++msgcount[goparam->threadid];
   }

   gofunc_end(goparam);
}

int main()
{
   goexec_t* goexec;
   struct timeval starttime;
   struct timeval endtime;

   for (int nrthreads = 1; nrthreads <= 128; nrthreads *= 2) {
      if (nrthreads == 2) nrthreads = 32;
      memset(msgcount, 0, sizeof(msgcount));
      goexec = goexec_new(nrthreads);
      gochan = gochan_new(goexec); 

      for (int i = 0; i < nrthreads; ++i) {
         goexec_newfunc(goexec, i, &server);
         goexec_newfunc(goexec, i, &client);
         goexec_newfunc(goexec, i, &server);
         goexec_newfunc(goexec, i, &client);
         goexec_newfunc(goexec, i, &server);
         goexec_newfunc(goexec, i, &client);
      }
      gettimeofday(&starttime, 0);
      goexec_run(goexec);
      gettimeofday(&endtime, 0);

      time_t sec = endtime.tv_sec - starttime.tv_sec;
      long usec = endtime.tv_usec - starttime.tv_usec;
      long msec = 1000 * (long)sec + usec / 1000;
      if (msec == 0) msec = 1;
      printf("gochan: %d*150000 send/recv time in ms: %ld (%ld msg/msec)\n", nrthreads, msec, nrthreads*3*50000/msec);

      gochan_delete(&gochan);
      goexec_delete(&goexec);
   }

   return 0;
}
