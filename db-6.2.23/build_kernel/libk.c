#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/security.h>
#include <linux/namei.h>
#include <linux/syscalls.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/sort.h>



#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

/* stdlib.h */
void abort(void)
{
	BUG();
}

void srand(unsigned int seed)
{
	return;
}

int rand(void)
{
	int x;
	get_random_bytes(&x, sizeof(int));
	return x;
}

void exit(int status)
{
	printk(KERN_CRIT "KDB: Exit called\n");
	while (1) ;
}

char *getenv(const char *name)
{
	// TODO
	return NULL;
}

void *realloc(void *ptr, size_t size)
{
	return krealloc(ptr, size, GFP_KERNEL);
}

void *malloc(size_t size)
{
	return kmalloc(size, GFP_KERNEL);
}

void *calloc(size_t nmemb, size_t size)
{
	return kzalloc(nmemb * size, GFP_KERNEL);
}

void free(void *ptr)
{
	kfree(ptr);
}

void qsort(void *base, size_t nmemb, size_t size,
			int (*compar)(const void *, const void *))
{
	sort(base, nmemb, size, compar, NULL);
}

/* pthread.h */

pthread_t pthread_self(void)
{
	extern pid_t gettid(void);
	return gettid();
}

int pthread_yield(void)
{
	schedule();
	return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
	return 0;
}

int pthread_condattr_init(pthread_condattr_t *attr) 
{
	return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex,
		pthread_mutexattr_t *attr) 
{
	mutex_init(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	return 0;
}

#define PTHREAD_PROCESS_SHARED 0

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) 
{ 
	return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) 
{ 
	return 0;
}

int pthread_cond_init(pthread_cond_t *wq, const pthread_condattr_t *attr)
{
	init_waitqueue_head(wq);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	mutex_lock(mutex);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	mutex_unlock(mutex);
	return 0;
}

int pthread_cond_timedwait(pthread_cond_t *wq,
		pthread_mutex_t *mutex, const struct timespec *abstime)
{
	int ret = 0;
	long timeout = timespec_to_jiffies(abstime);

	might_sleep();
	if (timeout) {
		wait_queue_t wait;

		init_wait_entry(&wait, 0);
		
		if (!prepare_to_wait_event(wq, &wait, TASK_KILLABLE)) {
			pthread_mutex_unlock(mutex);
			ret = schedule_timeout(timeout);
			pthread_mutex_lock(mutex);
		}
		finish_wait(wq, &wait);	
	} 

	return ret ? 0: ETIMEDOUT;	
}

int pthread_cond_wait(pthread_cond_t *wq,
		pthread_mutex_t *mutex)
{
	wait_queue_t wait;

	might_sleep();
	init_wait_entry(&wait, 0);
		
	if (!prepare_to_wait_event(wq, &wait, TASK_KILLABLE)) {
		pthread_mutex_unlock(mutex);
		schedule();
		pthread_mutex_lock(mutex);
	}
	finish_wait(wq, &wait);	
	return 0;	
}


int pthread_cond_broadcast(pthread_cond_t *wq)
{
	wake_up_all(wq);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *wq)
{
	wake_up(wq);
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *wq)
{
	return 0;
}

/* errno.h */
int errno;

/* stdio.h */

static int __sflags(const char *mode)
{
	int m, o;
	switch (*mode++) {
		case 'r':	/* open for reading */
			m = O_RDONLY;
			o = 0;
			break;
		case 'w':	/* open for writing */
			m = O_WRONLY;
			o = O_CREAT | O_TRUNC;
			break;
		case 'a':	/* open for appending */
			m = O_WRONLY;
			o = O_CREAT | O_APPEND;
			break;
		default:	/* illegal mode */
			errno = EINVAL;
			return (0);
	}
	/* [rwa]\+ or [rwa]b\+ means read and write */
	if (*mode == '+' || (*mode == 'b' && mode[1] == '+')) {
		m = O_RDWR;
	}
	return m | o;
}

FILE * fopen(const char *name, const char *mode)
{
	return filp_open(name, __sflags(mode), 0666);
}

int fflush(FILE *fp) 
{
	return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
	return kernel_read(fp, fp->f_pos, ptr, size * nmemb);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *fp)
{
	return kernel_write(fp, ptr, size * nmemb, fp->f_pos);
}

int fgetc(FILE *fp)
{
	char ch;

	if (1 == fread(&ch, sizeof(ch), 1, fp))
		return ch;
	return EOF;
}

char *fgets(char *str, int size, FILE *fp)
{
	int bread;

	bread = fread(str, 1, size - 1, fp);

	if (bread > 0) {
		char *p;

		p = strnchr(str, bread, '\n');

		if (p) {
			fp->f_pos -= bread - (p - str + 1);
			*(p+1) = 0;
		} else 
			p[size - 1] = 0;

		return str;
	}
	return NULL;
}

/* string.h */

char *strerror(int errno)
{
	static char msg[64];

	sprintf(msg, "[errno: %d]", errno);
	return msg;
}

/* time.h */
time_t time(time_t *t)
{
	*t = get_seconds();
	return *t;
}

struct tm *localtime(const time_t *t)
{
	static struct tm __tm;
	time_to_tm(*t, 0, &__tm);
	return &__tm;
}

struct tm *localtime_r(const time_t *t, struct tm *result)
{
	time_to_tm(*t, 0, result);
	return result;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	switch(clk_id) {
	case CLOCK_MONOTONIC:
		getrawmonotonic(tp);
		break;
	case CLOCK_REALTIME:
		ktime_get_real_ts(tp);
		break;
	default:
		BUG();
	}
	return 0;
}

char *ctime(const time_t *t)
{
	static char str[64];
	sprintf(str, "[ctime:%lu]", *t);
	return str;
}
