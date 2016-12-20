#ifndef _GBFS_KEYS_H_
#define _GBFS_KEYS_H_

#define GBFS_DB_PAGE	0
#define GBFS_DB_INODE	1

typedef struct gbfs_data_key_t {
	uint8_t type;
	ino_t ino;
	pgoff_t pgoff;
} __packed gbfs_data_key_t;

typedef struct gbfs_inode_key_t {
	uint8_t type;
	ino_t ino;
} __packed gbfs_inode_key_t;

#endif
