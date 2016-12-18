#ifndef _MINIX_KEYS_H_
#define _MINIX_KEYS_H_

typedef struct gbfs_data_key_t {
	ino_t ino;
	pgoff_t pgoff;
} gbfs_data_key_t;


#endif
