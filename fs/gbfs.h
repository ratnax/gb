#ifndef FS_GBFS_H
#define FS_GBFS_H

#include <linux/fs.h>
#include <linux/pagemap.h>
#include <db.h>

#include "gbfs_uapi.h"

#define INODE_VERSION(inode)	gbfs_sb(inode->i_sb)->s_version
#define GBFS_V1		0x0001		/* original gbfs fs */
#define GBFS_V2		0x0002		/* gbfs V2 fs */
#define GBFS_V3		0x0003		/* gbfs V3 fs */

/*
 * gbfs fs inode data in memory
 */
struct gbfs_inode_info {
	union {
		__u16 i1_data[16];
		__u32 i2_data[16];
	} u;
	struct inode vfs_inode;
};

/*
 * gbfs super-block data in memory
 */
struct gbfs_sb_info {
	unsigned long s_ninodes;
	unsigned long s_nzones;
	unsigned long s_imap_blocks;
	unsigned long s_zmap_blocks;
	unsigned long s_firstdatazone;
	unsigned long s_log_zone_size;
	unsigned long s_max_size;
	int s_dirsize;
	int s_namelen;
	struct buffer_head ** s_imap;
	struct buffer_head ** s_zmap;
	struct buffer_head * s_sbh;
	struct gbfs_super_block * s_ms;
	DB *dbp;
	unsigned short s_mount_state;
	unsigned short s_version;
};

extern struct inode *gbfs_iget(struct super_block *, unsigned long);
extern struct gbfs_inode * gbfs_V1_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern struct gbfs2_inode * gbfs_V2_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern struct inode * gbfs_new_inode(const struct inode *, umode_t, int *);
extern void gbfs_free_inode(struct inode * inode);
extern unsigned long gbfs_count_free_inodes(struct super_block *sb);
extern int gbfs_new_block(struct inode * inode);
extern void gbfs_free_block(struct inode *inode, unsigned long block);
extern unsigned long gbfs_count_free_blocks(struct super_block *sb);
extern int gbfs_getattr(struct vfsmount *, struct dentry *, struct kstat *);
extern int gbfs_prepare_chunk(struct page *page, loff_t pos, unsigned len);

extern void V1_gbfs_truncate(struct inode *);
extern void V2_gbfs_truncate(struct inode *);
extern void gbfs_truncate(struct inode *);
extern void gbfs_set_inode(struct inode *, dev_t);
extern int V1_gbfs_get_block(struct inode *, long, struct buffer_head *, int);
extern int V2_gbfs_get_block(struct inode *, long, struct buffer_head *, int);
extern unsigned V1_gbfs_blocks(loff_t, struct super_block *);
extern unsigned V2_gbfs_blocks(loff_t, struct super_block *);

extern struct gbfs_dir_entry *gbfs_find_entry(struct dentry*, struct page**);
extern int gbfs_add_link(struct dentry*, struct inode*);
extern int gbfs_delete_entry(struct gbfs_dir_entry*, struct page*);
extern int gbfs_make_empty(struct inode*, struct inode*);
extern int gbfs_empty_dir(struct inode*);
extern void gbfs_set_link(struct gbfs_dir_entry*, struct page*, struct inode*);
extern struct gbfs_dir_entry *gbfs_dotdot(struct inode*, struct page**);
extern ino_t gbfs_inode_by_name(struct dentry*);

extern const struct inode_operations gbfs_file_inode_operations;
extern const struct inode_operations gbfs_dir_inode_operations;
extern const struct file_operations gbfs_file_operations;
extern const struct file_operations gbfs_dir_operations;

static inline struct gbfs_sb_info *gbfs_sb(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct gbfs_inode_info *gbfs_i(struct inode *inode)
{
	return container_of(inode, struct gbfs_inode_info, vfs_inode);
}

static inline unsigned gbfs_blocks_needed(unsigned bits, unsigned blocksize)
{
	return DIV_ROUND_UP(bits, blocksize * 8);
}

#if defined(CONFIG_GBFS_FS_NATIVE_ENDIAN) && \
	defined(CONFIG_GBFS_FS_BIG_ENDIAN_16BIT_INDEXED)

#error Minix file system byte order broken

#elif defined(CONFIG_GBFS_FS_NATIVE_ENDIAN)

/*
 * big-endian 32 or 64 bit indexed bitmaps on big-endian system or
 * little-endian bitmaps on little-endian system
 */

#define gbfs_test_and_set_bit(nr, addr)	\
	__test_and_set_bit((nr), (unsigned long *)(addr))
#define gbfs_set_bit(nr, addr)		\
	__set_bit((nr), (unsigned long *)(addr))
#define gbfs_test_and_clear_bit(nr, addr) \
	__test_and_clear_bit((nr), (unsigned long *)(addr))
#define gbfs_test_bit(nr, addr)		\
	test_bit((nr), (unsigned long *)(addr))
#define gbfs_find_first_zero_bit(addr, size) \
	find_first_zero_bit((unsigned long *)(addr), (size))

#elif defined(CONFIG_GBFS_FS_BIG_ENDIAN_16BIT_INDEXED)

/*
 * big-endian 16bit indexed bitmaps
 */

static inline int gbfs_find_first_zero_bit(const void *vaddr, unsigned size)
{
	const unsigned short *p = vaddr, *addr = vaddr;
	unsigned short num;

	if (!size)
		return 0;

	size >>= 4;
	while (*p++ == 0xffff) {
		if (--size == 0)
			return (p - addr) << 4;
	}

	num = *--p;
	return ((p - addr) << 4) + ffz(num);
}

#define gbfs_test_and_set_bit(nr, addr)	\
	__test_and_set_bit((nr) ^ 16, (unsigned long *)(addr))
#define gbfs_set_bit(nr, addr)	\
	__set_bit((nr) ^ 16, (unsigned long *)(addr))
#define gbfs_test_and_clear_bit(nr, addr)	\
	__test_and_clear_bit((nr) ^ 16, (unsigned long *)(addr))

static inline int gbfs_test_bit(int nr, const void *vaddr)
{
	const unsigned short *p = vaddr;
	return (p[nr >> 4] & (1U << (nr & 15))) != 0;
}

#else

/*
 * little-endian bitmaps
 */

#define gbfs_test_and_set_bit	__test_and_set_bit_le
#define gbfs_set_bit		__set_bit_le
#define gbfs_test_and_clear_bit	__test_and_clear_bit_le
#define gbfs_test_bit	test_bit_le
#define gbfs_find_first_zero_bit	find_first_zero_bit_le

#endif

#endif /* FS_GBFS_H */
