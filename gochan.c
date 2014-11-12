// Compile with: gcc -std=gnu99 -ogochan gochan.c
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

typedef struct goroutine_t {
   int   id;
   void* state;
   void* chan_msg;
   void* continue_label;
   void (* mainfct) (struct goroutine_t* gofunc);
} goroutine_t;

#define start_goroutine(gofunc) \
         if ((gofunc)->continue_label) { \
            goto *(gofunc)->continue_label; \
         }

#define end_goroutine(gofunc) \
         remove_goroutine(gofunc)

static int         s_goroutines_size = 0;
static goroutine_t s_goroutines[100];

void goexecutor()
{
   while (s_goroutines_size) {
      // printf("--goexecutor--\n");
      for (int i = s_goroutines_size; i > 0; ) {
         --i;
         // printf("exec:%p id:%d i:%d size:%d\n", s_goroutines[i].mainfct, s_goroutines[i].id, i, s_goroutines_size);
         s_goroutines[i].mainfct(&s_goroutines[i]);
      }
   }
}

void remove_goroutine(goroutine_t* gofunc)
{
   assert(gofunc->id > 0);
   assert(s_goroutines_size >= gofunc->id);
   // printf("remove %p id=%d size=%d\n", gofunc->mainfct, gofunc->id, s_goroutines_size);
   if (gofunc->id != s_goroutines_size) {
      s_goroutines[s_goroutines_size-1].id = gofunc->id;
      s_goroutines[gofunc->id-1] = s_goroutines[s_goroutines_size-1];
   }
   --s_goroutines_size;
}

void add_goroutine(goroutine_t* gofunc)
{
   assert(s_goroutines_size < 100);
   gofunc->id = ++s_goroutines_size;
   s_goroutines[s_goroutines_size-1] = *gofunc;
   // printf("add %p id=%d size=%d\n", gofunc->mainfct, gofunc->id, s_goroutines_size);
}

void new_goroutine(void (* mainfct) (goroutine_t* gofunc))
{
   goroutine_t gofunc;
   gofunc.id = 0;
   gofunc.state = 0;
   gofunc.chan_msg = 0;
   gofunc.continue_label = 0;
   gofunc.mainfct = mainfct;
   add_goroutine(&gofunc);
}


typedef struct gochannel_t {
   goroutine_t reader;
   goroutine_t writer;
} gochannel_t;

int _chan_recv(gochannel_t* gochan, goroutine_t* gofunc)
{
   if (gochan->writer.id > 0) {
      // printf("recv msg = %d\n", (int)gochan->writer.chan_msg);
      gofunc->chan_msg = gochan->writer.chan_msg;
      add_goroutine(&gochan->writer);
      gochan->writer.id = 0;
      return 0;
   } else {
      assert(gofunc->id > 0);
      assert(gochan->reader.id == 0);
      // printf("recv wait\n");
      gochan->reader = *gofunc;
      remove_goroutine(gofunc);
      return EAGAIN;
   }
}

int _chan_send(gochannel_t* gochan, goroutine_t* gofunc, void* msg)
{
   if (gochan->reader.id > 0) {
      // printf("send msg = %d\n", (int)msg);
      gochan->reader.chan_msg = msg;
      add_goroutine(&gochan->reader);
      gochan->reader.id = 0;
      return 0;
   } else {
      assert(gofunc->id > 0);
      assert(gochan->writer.id == 0);
      // printf("send wait msg = %d\n", (int)msg);
      gofunc->chan_msg = msg;
      gochan->writer = *gofunc;
      remove_goroutine(gofunc);
      return EAGAIN;
   }
}

#define chan_send(gochan, gofunc, msg) \
         (gofunc)->continue_label = &&SEND_CONTINUE; \
         if (_chan_send(gochan, gofunc, msg)) { \
            return; \
         } \
         SEND_CONTINUE: ; 


#define chan_recv(gochan, gofunc, msg) \
         (gofunc)->continue_label = &&RECV_CONTINUE; \
         if (_chan_recv(gochan, gofunc)) { \
            return; \
         } \
         RECV_CONTINUE: \
         *(msg) = (gofunc)->chan_msg;

void init_gochannel(gochannel_t* chan)
{
   memset(chan, 0, sizeof(*chan));
}

// ===== client/server test =====

gochannel_t chan;

void server(goroutine_t* gofunc)
{
   start_goroutine(gofunc);

   for (gofunc->state = 0; (int)gofunc->state < 1000000; gofunc->state = (void*) (1 + (int)gofunc->state)) {
      // printf("try recv %d\n", (int)gofunc->state);
      void* msg;
      chan_recv(&chan, gofunc, &msg);
      // printf("received = %d\n", (int)msg);
      assert(msg == gofunc->state);
   }

   end_goroutine(gofunc);
}

void client(goroutine_t* gofunc)
{
   start_goroutine(gofunc);

   for (gofunc->state = 0; (int)gofunc->state < 1000000; gofunc->state = (void*) (1 + (int)gofunc->state)) {
      // printf("try send %d\n", (int)gofunc->state);
      chan_send(&chan, gofunc, gofunc->state);
      // printf("sended %d\n", (int)gofunc->state);
   }

   end_goroutine(gofunc);
}

int main()
{
   struct timeval starttime;
   struct timeval endtime;
   gettimeofday(&starttime, 0);
   init_gochannel(&chan);
   new_goroutine(&server);
   new_goroutine(&client);
   goexecutor();
   gettimeofday(&endtime, 0);

   time_t sec = endtime.tv_sec - starttime.tv_sec;
   long usec = endtime.tv_usec - starttime.tv_usec;
   long msec = 1000 * (long)sec + usec / 1000;
   printf("gochan: 1000000 send/recv time in ms: %ld (%ld msg/msec)\n", msec, 1000000/msec);

   return 0;
}
