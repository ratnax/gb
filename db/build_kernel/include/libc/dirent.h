#ifndef _LIBC_DIRENT_H_
#define _LIBC_DIRENT_H_

#include<linux/types.h>
struct dirent {
	ino_t          d_ino;       /* inode number */
	off_t          d_off;       /* not an offset; see NOTES */
	unsigned short d_reclen;    /* length of this record */
	unsigned char  d_type;      /* type of file; not supported
									   by all filesystem types */
	char           d_name[256]; /* filename */
};

typedef struct DIR_s DIR;

extern DIR *opendir(const char *name);
extern int closedir(DIR *d);
extern struct dirent *readdir(DIR *dirp);
#endif


