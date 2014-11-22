#include "iperf.h"

// =====================

// dummy test implementation (replace with your own implementation)

int iperf_prepare(iperf_param_t* param)
{
   param->nrops = 10 * 1000 * 1000;
   return 0;
}

int iperf_run(iperf_param_t* param)
{
   // performs nrops add operations
   int sum = 0;
   for (int x = 0; x < param->nrops; ++x) {
      sum += x;
   }
   param->size = (size_t) sum;
   return 0;
}

// =====================


typedef struct instance_t {
   pthread_t     thr;
   pid_t         child;
   iperf_param_t param;
   int preparedfd;
   int startfd;
   int resultfd;
} instance_t;

static int isthread = 1;
static int nrinstance = 4;
static int preparedfd[2];
static int startfd[2];
static int resultfd[2];
static instance_t* instance = 0;
static struct timeval starttime;
static struct timeval endtime;

static void print_error(int tid, int err)
{
   fprintf(stderr, "\nERROR %d (tid: %d): %s\n", err, tid, strerror(err));
}

static void abort_test(int tid, int err)
{
   print_error(tid, err);
   kill(-getpgid(0), SIGINT);
}

static void* instance_main(void* _param)
{
   int err;
   ssize_t bytes;
   instance_t* param = _param;

   err = iperf_prepare(&param->param);
   if (err) abort_test(param->param.tid, err);

   // signal waiting starter that instance is prepared
   bytes = write(param->preparedfd, "", 1);
   if (bytes != 1) {
      abort_test(param->param.tid, EIO);
   }

   // wait for start signal
   char dummy;
   bytes = read(param->startfd, &dummy, 1);
   if (bytes != 1) {
      abort_test(param->param.tid, EIO);
   }

   err = iperf_run(&param->param);
   if (err) abort_test(param->param.tid, err);

   // signal result (time) to waiting starter
   struct timeval now;
   gettimeofday(&now, 0);
   char result[64] = {0};
   sprintf(result, "%d %lld %lld", param->param.nrops, (long long)now.tv_sec, (long long)now.tv_usec);
   bytes = write(param->resultfd, result, sizeof(result));
   if (bytes != sizeof(result)) {
      abort_test(param->param.tid, EIO);
   }

   // printf("tid: %d time: %s\n", param->param.tid, result);

   return 0;
}

static void prepare_instances(void)
{
   int err;

   err = pipe(preparedfd);
   if (err) abort_test(-1, errno);
   err = pipe(startfd);
   if (err) abort_test(-1, errno);
   err = pipe(resultfd);
   if (err) abort_test(-1, errno);

   // start all instances
   for (int tid = 0; tid < nrinstance; ++tid) {
      instance[tid].param.tid = tid;
      instance[tid].param.isthread = isthread;
      instance[tid].param.nrops = 1;
      instance[tid].param.addr = 0;
      instance[tid].param.size = 0;
      instance[tid].preparedfd = preparedfd[1];
      instance[tid].startfd = startfd[0];
      instance[tid].resultfd = resultfd[1];

      if (isthread) {
         err = pthread_create(&instance[tid].thr, 0, &instance_main, &instance[tid]);
         if (err) abort_test(-1, err);

      } else {
         instance[tid].child = fork();
         if (instance[tid].child == -1) {
            abort_test(-1, errno);
         }

         if (instance[tid].child == 0) {
            instance_main(&instance[tid]);
            exit(0);
         }
      }
   }

   // wait until all instances prepared themselves
   for (int tid = 0; tid < nrinstance; ++tid) {
      char byte;
      ssize_t bytes = read(preparedfd[0], &byte, 1);
      if (bytes != 1) {
         abort_test(-1, EIO);
      }
   }
}

static void run_instances(/*out*/long long* nrops_total)
{
   gettimeofday(&starttime, 0);

   // send start signal
   {
      char buffer[256];
      ssize_t bytes = write(startfd[1], buffer, (size_t)nrinstance);
      if (bytes != nrinstance) {
         abort_test(-1, EIO);
      }
   }

   // wait for result
   *nrops_total = 0;
   for (int tid = 0; tid < nrinstance; ++tid) {
      char result[64];
      ssize_t bytes = read(resultfd[0], result, sizeof(result));
      if (bytes != sizeof(result)) {
         abort_test(-1, EIO);
      }
      int nrops = 0;
      long long sec = 0;
      long long usec = -1;
      sscanf(result, "%d %lld %lld", &nrops, &sec, &usec);
      if (usec == -1) {
         abort_test(-1, EINVAL);
      }
      if ( tid == 0
           || ( endtime.tv_sec < sec
                || (endtime.tv_sec == sec && endtime.tv_usec < usec))) {
         endtime.tv_sec = (time_t) sec;
         endtime.tv_usec = (suseconds_t) usec;
      }
      *nrops_total += nrops;
   }

   // wait for end of instance
   for (int tid = 0; tid < nrinstance; ++tid) {
      if (isthread) {
         pthread_join(instance[tid].thr, 0);

      } else {
         int status;
         pid_t child = waitpid(instance[tid].child, &status, 0);
         if (child != instance[tid].child) {
            abort_test(-1, ESRCH);
         }
      }
   }

   for (int i = 0; i < 2; ++i) {
      close(preparedfd[i]);
      close(startfd[i]);
      close(resultfd[i]);
   }
}

int main(int argc, const char* argv[])
{
   int err = 0;

   if (argc > 3) err = EINVAL;

   if (argc == 3) {
      if (0 == strcmp(argv[2], "thread")) {
         isthread = 1;
      } else if (0 == strcmp(argv[2], "process")) {
         isthread = 0;
      } else {
         err = EINVAL;
      }

      -- argc;
   }

   if (argc == 2) {
      nrinstance = 0;
      sscanf(argv[1], "%d", &nrinstance);
      if (nrinstance <= 0 || nrinstance >= 256) err = EINVAL;
   }

   if (err) {
      printf("Usage: %s [nr-test-instances] [thread|process]\n", argv[0]);
      printf("With: 0 < nr-test-instances < 256\n");
      exit(err);
   }

   printf("Run %d test %s%s\n", nrinstance, isthread ? "thread" : "process", nrinstance == 1 ? "" : isthread ? "s" : "es");

   instance = (instance_t*) malloc(sizeof(instance_t) * (size_t)nrinstance);
   if (! instance) err = ENOMEM;

   if (! err) {
      err = setpgid(0, 0);
      if (err) err = errno;
   }

   if (err) {
      print_error(-1, err);

   } else {
      prepare_instances();
      long long nrops = 1;
      run_instances(&nrops);

      long long sec = endtime.tv_sec - starttime.tv_sec;
      long long usec = endtime.tv_usec - starttime.tv_usec;
      usec = 1000000ll * sec + usec;
      printf("\nRESULT: %lld usec for %lld operations (%lld operations/usec)\n", usec, nrops, nrops/usec);
   }

   return err;
}
