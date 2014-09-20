////////////////////////////////////////////////
// Test of Store after Load Reordering on X86 //
////////////////////////////////////////////////

// Compile on Linux with gcc -std=gnu99 reorder.c -lpthread

#include <pthread.h>
#include <stdio.h>

static volatile int x = 0;
static volatile int y = 0;
static volatile int x2= 0;
static volatile int y2= 0;

///////////////////////////////////////////
// remove comment from one of the fences //

// #define MEMORY_FENCE() asm volatile("mfence" ::: "memory") /* full memory barrier */
// #define MEMORY_FENCE() asm volatile("sfence" ::: "memory") /* store fence */
 #define MEMORY_FENCE() asm volatile("" ::: "memory") /* compiler does not reorder */

void* thr_x(void* arg)
{
   x = 1; 
   MEMORY_FENCE();
   x2 = y;   // could be reordered x = 1
   return 0;
}

void* thr_y(void* arg)
{
   y = 1; 
   MEMORY_FENCE();
   y2 = x;  // could be reordered y = 1
   return 0;
}

int main()
{
   pthread_t thr1, thr2;


   for (int i = 0; i < 1000000; ++i) {
      x = y = 0;
      pthread_create(&thr1, 0, &thr_x, 0);
      pthread_create(&thr2, 0, &thr_y, 0);
      pthread_join(thr1, 0);
      pthread_join(thr2, 0);
      asm volatile("mfence" ::: "memory");
      if (x2 == 0 && y2 == 0) {
         printf("TADAA (catched reordering) (x2==y2==0) i = %d\n", i);
      }
   }

   return 0;
}
