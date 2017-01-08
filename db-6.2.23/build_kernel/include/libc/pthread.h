#ifndef _LIBC_PTHREAD_H_
#define _LIBC_PTHREAD_H_

#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/rwsem.h>

typedef unsigned long pthread_t;
typedef struct mutex pthread_mutex_t;
typedef wait_queue_head_t pthread_cond_t;
typedef struct rw_semaphore pthread_rwlock_t;

typedef struct { } pthread_condattr_t;
typedef struct { } pthread_mutexattr_t;
typedef struct { } pthread_rwlockattr_t;

extern int pthread_condattr_destroy(pthread_condattr_t *attr);
extern int pthread_condattr_init(pthread_condattr_t *attr);
extern int pthread_mutex_init(pthread_mutex_t *mutex,
		pthread_mutexattr_t *attr);
extern int pthread_mutex_destroy(pthread_mutex_t *mutex);
extern int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
extern int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
extern int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
extern int pthread_mutexattr_init(pthread_mutexattr_t *attr);
extern int pthread_cond_init(pthread_cond_t *wq,
			    const pthread_condattr_t *attr);

#define pthread_mutex_trylock(mutex) mutex_trylock(mutex)

extern int pthread_mutex_lock(pthread_mutex_t *mutex);
extern int pthread_mutex_unlock(pthread_mutex_t *mutex);


extern int pthread_cond_timedwait(pthread_cond_t *wq,
					pthread_mutex_t *mutex, const struct timespec *abstime);
extern int pthread_cond_wait(pthread_cond_t *wq, pthread_mutex_t *mutex);
extern int pthread_cond_broadcast(pthread_cond_t *wq);
extern int pthread_cond_signal(pthread_cond_t *wq);
extern int pthread_cond_destroy(pthread_cond_t *wq);
extern int pthread_yield(void);
extern pthread_t pthread_self(void);
#endif
