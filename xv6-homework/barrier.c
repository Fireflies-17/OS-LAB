#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

// #define SOL

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()
{
  // 获取互斥保护锁
  pthread_mutex_lock(&bstate.barrier_mutex);

  // 记录当前线程到来时的轮数
  int round = bstate.round;

  // 当前已抵达的线程数+1
  bstate.nthread++;

  // 检查是不是本轮次中最后到达的
  // 是最后到达的，开始放行
  if(bstate.nthread == nthread) {
    bstate.round++;
    bstate.nthread = 0;
    
    // 唤醒所有休眠的线程
    pthread_cond_broadcast(&bstate.barrier_cond);

  } 

  // 不是最后一个，进入等待
  else {
    // 防范提前唤醒或者虚假苏醒
    while(round == bstate.round) {
        pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    }
  }

  // 释放锁
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();
    usleep(random() % 100);
  }
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
