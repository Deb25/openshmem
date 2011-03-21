#define _POSIX_C_SOURCE 199309
#include <time.h>		/* for nanosleep */
#undef _POSIX_C_SOURCE

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>
#include <sched.h>

#include "comms.h"
#include "trace.h"
#include "state.h"

#include "service.h"

/*
 * TODO: should be user-controllable.  Could also make the library
 * monitor frequency of communication and allow it to fine-tune its
 * own polling speed.
 *
 */
static double backoff_secs = 0.00001;
/* static int num_polls_per_loop = 10; */

static struct timespec backoff;

static pthread_t service_thr;

/*
 * unused
 *
 */
void
__shmem_service_set_pause(double ms)
{
  double s = floor(ms);
  double f = ms - s;

  backoff.tv_sec  = (long) s;
  backoff.tv_nsec = (long) (f * 1.0e9);
}

/*
 * the real service thread: poll the network.  Should we include a
 * short refractory period to help other things happen in parallel?
 *
 */

static volatile poll_mode_t poll_mode;

void
__shmem_service_set_mode(poll_mode_t m)
{
  poll_mode = m;
}

static
void *
service_thread(void *unused_arg)
{
  while (poll_mode != SERVICE_FINISH) {

    switch (poll_mode) {

    case SERVICE_POLL:
      __shmem_comms_poll_service();
      /* nanosleep(& backoff, (struct timespec *) NULL); */
      break;

    case SERVICE_FENCE:
      __shmem_comms_fence_service();
      break;

    default:
      /* TODO: shouldn't get here */
      break;

    }

  }

  return (void *) NULL;
}

static void
set_low_priority(pthread_attr_t *p)
{
  struct sched_param sp;

  pthread_attr_init(p);
  pthread_attr_getschedparam(p, & sp);
  sp.sched_priority = sched_get_priority_min(SCHED_RR);
  pthread_attr_setschedpolicy(p, SCHED_RR);
  pthread_attr_setschedparam(p, & sp);
}

/*
 * start the service sub-system.  Initiate polling sentinel and get
 * the service thread going.  Fatal error if we cant create the
 * thread.
 *
 */

void
__shmem_service_thread_init(void)
{
  int s;
  pthread_attr_t pa;

  poll_mode = SERVICE_POLL;

  /* set the refractory period */
  __shmem_service_set_pause(backoff_secs);

  /* prefer the processing thread over the service thread */
  set_low_priority(& pa);

  s = pthread_create(& service_thr, & pa, service_thread, NULL);
  if (s != 0) {
    __shmem_trace(SHMEM_LOG_FATAL,
		  "internal error: can't create network service thread (%s)",
		  strerror(errno)
		  );
    /* NOT REACHED */
  }

  __shmem_trace(SHMEM_LOG_SERVICE,
		"thread started"
		);
}

/*
 * stop the service sub-system.  Disable polling sentinel and reap the
 * service thread.  If we can't shut down the thread, complain but
 * continue with the rest of the shut down.
 *
 */

void
__shmem_service_thread_finalize(void)
{
  int s;

  poll_mode = SERVICE_FINISH;

  s = pthread_join(service_thr, NULL);
  if (s != 0) {
    __shmem_trace(SHMEM_LOG_NOTICE,
		  "internal error: couldn't clean up network service thread (%s)",
		  strerror(errno)
		  );
  }

  __shmem_trace(SHMEM_LOG_SERVICE,
		"thread finished"
		);
}
