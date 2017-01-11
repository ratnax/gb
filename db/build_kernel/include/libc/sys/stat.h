#ifndef _LIBC_SYS_STAT_H_
#define _LIBC_SYS_STAT_H_
#include <linux/types.h>
#include <uapi/asm/stat.h>

extern int stat(const char *pathname, struct stat *st);
extern int mkdir(const char *pathname, mode_t mode);
extern int chmod(const char *pathname, mode_t mode);
extern int fchmod(int fd, mode_t mode);
#endif
