#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <math.h>

//#define MY_PTHREAD
//#define FAIR_SPIN
//#define MY_SPIN
#define BAKERY

 
time_t stop;
long producers;
long consumers;
volatile int *cs_entries;
volatile int in_cs = 0;
volatile int next_enq = 0;
volatile int next_deq = 0;

#ifdef MY_PTHREAD
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct spin_lock_t {			//added just to conform to other API
    volatile int placeholder;
};

struct spin_lock_t thread_lock;

void spin_lock (struct spin_lock_t *s)
{
    s->placeholder = 1;
    pthread_mutex_lock(&mutex);
}

void spin_unlock (struct spin_lock_t *s)
{
    s->placeholder = 0;
    pthread_mutex_unlock(&mutex);
}
#endif

#ifdef FAIR_SPIN
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
#endif

#ifdef MY_SPIN
/*
 * atomic_cmpxchg
 * 
 * equivalent to atomic execution of this code:
 *
 * if (*ptr == old) {
 *   *ptr = new;
 *   return old;
 * } else {
 *   return *ptr;
 * }
 *
 */
static inline int atomic_cmpxchg (volatile int *ptr, int old, int new)
{
  int ret;
  asm volatile ("lock cmpxchgl %2,%1"
    : "=a" (ret), "+m" (*ptr)     
    : "r" (new), "0" (old)      
    : "memory");         
  return ret;                            
}

struct spin_lock_t {
    volatile int lock;
};

struct spin_lock_t thread_lock;

void spin_lock (struct spin_lock_t *s)
{
    while(atomic_cmpxchg(&s->lock,0,1));
}

void spin_unlock (struct spin_lock_t *s)
{
    atomic_cmpxchg(&s->lock,1,0);
}
#endif

//queue API
struct _queue {
    int array[32];
    int push;
    int pull;
    int count;
};

struct _queue queue;

int enq(int number)
{
    if(queue.count == 32)
	return 0;

    queue.array[queue.push] = number;
    queue.push = (queue.push+1)%32;
    queue.count += 1;
    return 1;
}

int deq(int *value)
{
    if(queue.count == 0)
	return 0;

    *value = queue.array[queue.pull];
    queue.pull = (queue.pull+1)%32;
    queue.count -= 1;
    return 1;
}

/* create thread argument struct */
typedef struct _thread_data_t {
    int tid;
} thread_data_t;
 
void mfence (void) {
  asm volatile ("mfence" : : : "memory");
}

#ifdef BAKERY
volatile int *choosing;
volatile int *number;
/* thread function */
void *thr_func(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    int retval;
    choosing[data->tid] = 0;
    number[data->tid] = 0;
    cs_entries[data->tid] = 0;
 
    while(time(NULL) < stop)
    {
	choosing[data->tid] = 1;
	int i;
	int max = 0;
	for(i=0;i<(producers + consumers);++i)
	    if(max < number[i])
		max = number[i];
	number[data->tid] = max + 1;
	/* A fence is needed here to make sure that another thread doesn't see the next step
	   of setting choosing to 0 without seeing the change to the number.  Without this
	   fence it would be possible for a thread with a larger number (or equal number and
	   higher id) to access the critical section out of order.  This would also cause the
	   thread with the smaller number to enter the critical section when it reaches it, breaking
	   mutual exclusion. */
	mfence();
	choosing[data->tid] = 0;

	for(i=0;i<(producers+consumers);++i)
	{
	    while(choosing[i]);
	    while((number[i] != 0) && ((number[i] < number[data->tid]) || (number[i] == number[data->tid] && i < data->tid)));
	}
	
	//critical section
	assert(in_cs == 0);
	in_cs++;
	assert(in_cs == 1);
	in_cs++;
	assert(in_cs == 2);
	in_cs++;
	assert(in_cs == 3);
	in_cs = 0;
	/* Producers continally enqueue a counting value so it is easy to compare when dequeued.
	   Since the value is always increasing a dropped, duplicate, or out of order queue/dequeue
	   would be easily detected. */
	if(data->tid < producers)  //producer thread
	{
	    if(enq(next_enq))
	    {
	        next_enq += 1;
	        //printf("queued %d\n",next_enq);
	        cs_entries[data->tid] += 1;
	    }
	}
	else			//consumer thread
	{
	    if(deq(&retval))
	    {
	        cs_entries[data->tid] += 1;
	        //printf("expected:%d dequeued:%d\n",next_deq,retval);
	        assert(retval == next_deq++);
	    }
	}
	//end of critical section

	/* A fence is needed at the end of the critical section to ensure that all 
	   threads see changes to shared data in the critical section.  Without
	   this fence it should still provide mutual exclusion but data may not
	   be correct. */
	mfence();
	number[data->tid] = 0;

	/* More fences shouldn't be needed since the first one prevents mutual exclusion
	   from being broken and the second one ensures data coherence. */
    }
    pthread_exit(NULL);
}

#endif

#ifndef BAKERY
/* thread function */
void *thr_func(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;

    cs_entries[data->tid] = 0;
    int retval;
 
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
	/* Producers continally enqueue a counting value so it is easy to compare when dequeued.
	   Since the value is always increasing a dropped, duplicate, or out of order queue/dequeue
	   would be easily detected. */
	if(data->tid < producers)  //producer thread
	{
	    if(enq(next_enq))
	    {
	        next_enq += 1;
	        //printf("queued %d\n",next_enq);
	        cs_entries[data->tid] += 1;
	    }
	}
	else			//consumer thread
	{
	    if(deq(&retval))
	    {
	        cs_entries[data->tid] += 1;
	        //printf("expected:%d dequeued:%d\n",next_deq,retval);
	        assert(retval == next_deq++);
	    }
	}
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
#endif
 
int main(int argc, char **argv) {
/* Error checking code taken directly from the man page for strtol and adapted */
    char *endptr;
    long seconds;

    /* Error if not 3 arguments */

    if (argc != 4) 
    {
        fprintf(stderr, "Incorrect number of arguments\n");
        fprintf(stderr, "Usage: %s <Num of Producers> <Num of Consumers> <Seconds to Run>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    errno = 0;    /* To distinguish success/failure after call */
    producers = strtol(argv[1], &endptr, 10);

    /* Check for various possible errors */

    if ((errno == ERANGE && (producers == LONG_MAX || producers == LONG_MIN)) || (errno != 0 && producers == 0)) 
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    /* Error if number of producers exceeds 99 */
    if (endptr == argv[1] || *endptr != '\0' || producers < 1 || producers > 99) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s <Num of Producers> <Num of Consumers> <Seconds to Run>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    consumers = strtol(argv[2], &endptr, 10);

    /* Check for various possible errors */

    if ((errno == ERANGE && (consumers == LONG_MAX || consumers == LONG_MIN)) || (errno != 0 && consumers == 0)) 
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    /* Error if number of producers exceeds 99 */
    if (endptr == argv[2] || *endptr != '\0' || consumers < 1 || consumers > 99) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s <Num of Producers> <Num of Consumers> <Seconds to Run>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    seconds = strtol(argv[3], &endptr, 10);

    if ((errno == ERANGE && (seconds == LONG_MAX || seconds == LONG_MIN)) || (errno != 0 && seconds == 0)) 
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    /* Error if number of seconds < 1*/
    if (endptr == argv[3] || *endptr != '\0' || seconds < 1) {
        fprintf(stderr, "Invalid argument\n");
        fprintf(stderr, "Usage: %s <Num of Producers> <Num of Consumers> <Seconds to Run>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

  /* Arguments ok, so proceed */
#ifdef BAKERY
    choosing = malloc(sizeof(int) * (producers + consumers));
    number = malloc(sizeof(int) * (producers + consumers));
#endif
    pthread_t thr[producers + consumers];
    stop = time(NULL) + seconds;
    int i, rc;
    /* create a thread_data_t argument array */
    thread_data_t thr_data[producers + consumers];

    cs_entries = malloc(sizeof(int) * (producers + consumers));
 
    /* create threads */
    for (i = 0; i < (producers + consumers); ++i) {
      thr_data[i].tid = i;
      if ((rc = pthread_create(&thr[i], NULL, thr_func, &thr_data[i]))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
      }
    }

    /* block until all threads complete */
    for (i = 0; i < (producers + consumers); ++i) {
      pthread_join(thr[i], NULL);
    }

    double pro_avg = 0;
    double con_avg = 0;
    // print results
    for(i=0;i<producers;++i)
    {
	pro_avg += cs_entries[i];
	printf("Producer %d queued %d items\n",i,cs_entries[i]);
    }
    for(i=producers;i<(producers + consumers);++i)
    {
	con_avg += cs_entries[i];
	printf("Consumer %d dequeued %d items\n",i,cs_entries[i]);
    }

    //calc std deviation

    //find mean of producers and consumers
    pro_avg = pro_avg/producers;
    con_avg = con_avg/consumers;

    //compute deviation squared for each
    double pro_dev[producers];
    double con_dev[consumers];
    double pro_result = 0;
    double con_result = 0;
    for(i=0;i<producers;++i)
    {
	pro_dev[i] = (pro_avg - cs_entries[i]) * (pro_avg - cs_entries[i]);
	pro_result += pro_dev[i];
    }
    for(i=producers;i<(producers + consumers);++i)
    {
	con_dev[i] = (con_avg - cs_entries[i]) * (con_avg - cs_entries[i]);
	con_result += con_dev[i];
    }

    //divide by N and sqrt to find std dev
    pro_result = pro_result/(producers);
    con_result = con_result/(consumers);
    double pro_deviation = sqrt(pro_result);
    double con_deviation = sqrt(con_result);
    printf("Std dev of Producers is %f\n",pro_deviation);
    printf("Std dev of Consumers is %f\n",con_deviation);
 
    return EXIT_SUCCESS;
}
