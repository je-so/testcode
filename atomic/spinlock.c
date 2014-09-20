////////////////////////////////////////////////////////
// Speedtest of Compare&Swap vs. XADD(x86) atomic ops //
////////////////////////////////////////////////////////

// On Linux – compile it with 
// > gcc -std=gnu99 spinlock.c -lpthread

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

static volatile int s_lock = 0;
static volatile int s_turn = 0;
static volatile unsigned long long s_counter = 0;
static volatile unsigned s_maxwait = 0;

#if 1
#   define LOCK()   lock_cmpswap()
#   define UNLOCK() unlock_cmpswap()
#else // 20% faster
#   define LOCK()   lock_xadd()
#   define UNLOCK() unlock_xadd()
#endif

void lock_cmpswap()
{
   unsigned count = 0;
   while (__sync_val_compare_and_swap(&s_lock, 0, 1) != 0) {
      ++count;
   }
   if (count > s_maxwait) s_maxwait = count;
}

void unlock_cmpswap()
{
   __sync_val_compare_and_swap(&s_lock, 1, 0);
}

void lock_xadd()
{
   unsigned count = 0;
   int turn = __sync_fetch_and_add(&s_lock, 1);
   while (s_turn != turn) {
      ++count;
   }
   if (count > s_maxwait) s_maxwait = count;
}

void unlock_xadd()
{
   __sync_fetch_and_add(&s_turn, 1);
}

void* thr_main(void* arg)
{
   for (int i = 0; i < 1000000; ++i) {
      LOCK();
      s_counter ++;
      UNLOCK();
   }
   return 0;
}

int main()
{
   pthread_t thread[10];
   struct timeval start, end;

   gettimeofday(&start, 0);

   for (int i = 0; i < 4; ++i) {
      assert(0 == pthread_create(&thread[i], 0, &thr_main, 0));
   }

   for (int i = 0; i < 4; ++i) {
      assert(0 == pthread_join(thread[i], 0));
   }
   
   gettimeofday(&end, 0);

   printf("counter = %lld (expected value 4000000)\n", s_counter);
   printf("maxwait = %d\n", s_maxwait);
   printf("time (ms) = %d\n", (int)(end.tv_sec - start.tv_sec) * 1000 
           + (int)end.tv_usec / 1000 - (int)start.tv_usec / 1000);

   return 0;
}
