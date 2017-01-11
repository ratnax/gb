#ifndef _LIBC_TIME_H_
#define _LIBC_TIME_H_

#include <uapi/linux/time.h>
#include <linux/time.h>

extern time_t time(time_t *t);
extern struct tm *localtime(const time_t *t);
extern struct tm *localtime_r(const time_t *t, struct tm *result);
extern int clock_gettime(clockid_t clk_id, struct timespec *tp);
extern char *ctime(const time_t *t);
#endif
