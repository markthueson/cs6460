#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
 
time_t stop;
long thread_num;
volatile int *cs_entries;
volatile int in_cs = 0;

/*
 * atomic_xadd
 *
 * equivalent to atomic execution of this code:
 *
 * return (*ptr)++;
 * 
 */
static inline int atomic_xadd (volatile int *ptr)
{
  register int val __asm__("eax") = 1;
  asm volatile ("lock xaddl %0,%1"
  : "+r" (val)
  : "m" (*ptr)
  : "memory"
  );  
  return val;
}

struct spin_lock_t {
    volatile int serving;
    volatile int next;
};

struct spin_lock_t thread_lock;

void spin_lock (struct spin_lock_t *s)
{
    int retv = atomic_xadd(&s->next);
    while(retv != s->serving);
}

void spin_unlock (struct spin_lock_t *s)
{
    atomic_xadd(&s->serving);
}

/* create thread argument struct for thr_func() */
typedef struct _thread_data_t {
    int tid;
    double stuff;
} thread_data_t;
 
void mfence (void) {
  asm volatile ("mfence" : : : "memory");
}

/* thread function */
void *thr_func(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    cs_entries[data->tid] = 0;
 
    while(time(NULL) < stop)
    {
	spin_lock(&thread_lock);
	mfence();
	
	//critical section
	assert(in_cs == 0);
	in_cs++;
	assert(in_cs == 1);
	in_cs++;
	assert(in_cs == 2);
	in_cs++;
	assert(in_cs == 3);
	in_cs = 0;
	cs_entries[data->tid] += 1;
	//end of critical section

	/* A fence is needed at the end of the critical section to ensure that all 
	   threads see changes to shared data in the critical section.  Without
	   this fence it should still provide mutual exclusion but data may not
	   be correct. */
	mfence();
	spin_unlock(&thread_lock);

	/* More fences shouldn't be needed since the first one prevents mutual exclusion
	   from being broken and the second one ensures data coherence. */
    }
    pthread_exit(NULL);
}
 
int main(int argc, char **argv) {
/* Error checking code taken directly from the man page for strtol and adapted */
    char *endptr;
    //long thread_num;
    long seconds;

    /* Error if not 2 arguments */

    if (argc != 3) 
    {
        fprintf(stderr, "Incorrect number of arguments\n");
        fprintf(stderr, "Usage: %s <Num of Threads> <Seconds to Run>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    errno = 0;    /* To distinguish success/failure after call */
    thread_num = strtol(argv[1], &endptr, 10);

    /* Check for various possible errors */

    if ((errno == ERANGE && (thread_num == LONG_MAX || thread_num == LONG_MIN)) || (errno != 0 && thread_num == 0)) 
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    /* Error if number of threads exceeds 99 or seconds < 1*/
    if (endptr == argv[1] || *endptr != '\0' || thread_num < 1 || thread_num > 99) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s [1-99] <Seconds>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    seconds = strtol(argv[2], &endptr, 10);

    if ((errno == ERANGE && (seconds == LONG_MAX || seconds == LONG_MIN)) || (errno != 0 && seconds == 0)) 
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    /* Error if number of seconds < 1*/
    if (endptr == argv[1] || *endptr != '\0' || seconds < 1) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s [1-99] <Seconds>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

  /* Arguments ok, so proceed */
    thread_lock.serving = 0;
    thread_lock.next = 0;
    pthread_t thr[thread_num];
    stop = time(NULL) + seconds;
    int i, rc;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[thread_num];

    cs_entries = malloc(sizeof(int) * thread_num);
 
    /* create threads */
    for (i = 0; i < thread_num; ++i) {
      thr_data[i].tid = i;
      if ((rc = pthread_create(&thr[i], NULL, thr_func, &thr_data[i]))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
      }
    }
    /* block until all threads complete */
    for (i = 0; i < thread_num; ++i) {
      pthread_join(thr[i], NULL);
    }

    for(i=0;i<thread_num;++i)
	printf("Thread %d entered CS %d times\n",i,cs_entries[i]);
 
    return EXIT_SUCCESS;
}
