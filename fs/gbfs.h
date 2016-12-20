#ifndef FS_GBFS_H
#define FS_GBFS_H

#include <linux/fs.h>
#include <linux/pagemap.h>
#include <db.h>

#include "gbfs_uapi.h"

/*
 * gbfs super-block data in memory
 */
struct gbfs_sb_info {
	unsigned long s_max_size;
	int s_dirsize;
	int s_namelen;
	struct gbfs_super_block * s_ms;
	DB *dbp;
	unsigned short s_mount_state;
	unsigned short s_max_links;
};

extern struct inode *gbfs_iget(struct super_block *, unsigned long);
extern struct gbfs_inode * gbfs_raw_inode(struct super_block *, ino_t);
extern int gbfs_update_inode(struct inode *, struct gbfs_inode *);
extern struct inode * gbfs_new_inode(const struct inode *, umode_t, int *);
extern void gbfs_free_inode(struct inode * inode);
extern unsigned long gbfs_count_free_inodes(struct super_block *sb);
extern int gbfs_new_block(struct inode * inode);
extern void gbfs_free_block(struct inode *inode, unsigned long block);
extern unsigned long gbfs_count_free_blocks(struct super_block *sb);
extern int gbfs_getattr(struct vfsmount *, struct dentry *, struct kstat *);
extern int gbfs_prepare_chunk(struct page *page, loff_t pos, unsigned len);

extern void __gbfs_truncate(struct inode *);
extern void gbfs_truncate(struct inode *);
extern void gbfs_set_inode(struct inode *, dev_t);
extern int __gbfs_get_block(struct inode *, long, struct buffer_head *, int);
extern unsigned gbfs_blocks(loff_t, struct super_block *);

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
