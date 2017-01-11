#ifndef _LIBC_FCNTL_H_
#define _LIBC_FCNTL_H_

#include <linux/types.h>
#include <uapi/asm/fcntl.h>

extern int open(const char *pathname, int flags, mode_t mode);
extern int fcntl(int fd, int cmd, struct flock *l);

#endif
