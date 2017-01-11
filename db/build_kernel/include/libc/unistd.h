#ifndef _LIBC_UNISTD_H_
#define _LIBC_UNISTD_H_

extern ssize_t write(int fd, const void *buf, size_t count);
extern ssize_t read(int fd, const void *buf, size_t count);
extern ssize_t pread(int fd, void *buf, size_t count, loff_t offset);
extern ssize_t pwrite(int fd, const void *buf, size_t count, loff_t offset);
extern int fsync(int fd);
extern int fdatasync(int fd);
extern int close(int fd);

extern pid_t getpid(void);
extern int rmdir(const char *pathname);
extern uid_t getuid(void);

extern off_t lseek(int fd, off_t offset, int whence);
extern int fstat(int fd, struct stat *buf);
extern int unlink(const char *pathname);
extern int select(int nfds, fd_set *readfds, fd_set *writefds,
					fd_set *exceptfds, struct timeval *timeout);

#endif
