/*
 *  linux/fs/gbfs/bitmap.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 * Modified for 680x0 by Hamish Macdonald
 * Fixed for 680x0 by Andreas Schwab
 */

/* bitmap.c contains the code that handles the inode and block bitmaps */

#include "gbfs.h"
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/bitops.h>
#include <linux/sched.h>

#include "keys.h"

static DEFINE_SPINLOCK(bitmap_lock);

/*
 * bitmap consists of blocks filled with 16bit words
 * bit set == busy, bit clear == free
 * endianness is a mess, but for counting zero bits it really doesn't matter...
 */
static __u32 count_free(struct buffer_head *map[], unsigned blocksize, __u32 numbits)
{
	__u32 sum = 0;
	unsigned blocks = DIV_ROUND_UP(numbits, blocksize * 8);

	while (blocks--) {
		unsigned words = blocksize / 2;
		__u16 *p = (__u16 *)(*map++)->b_data;
		while (words--)
			sum += 16 - hweight16(*p++);
	}

	return sum;
}

void gbfs_free_block(struct inode *inode, unsigned long block)
{
	struct super_block *sb = inode->i_sb;
	struct gbfs_sb_info *sbi = gbfs_sb(sb);
	struct buffer_head *bh;
	int k = sb->s_blocksize_bits + 3;
	unsigned long bit, zone;

	if (block < sbi->s_firstdatazone || block >= sbi->s_nzones) {
		printk("Trying to free block not in datazone\n");
		return;
	}
	zone = block - sbi->s_firstdatazone + 1;
	bit = zone & ((1<<k) - 1);
	zone >>= k;
	if (zone >= sbi->s_zmap_blocks) {
		printk("gbfs_free_block: nonexistent bitmap buffer\n");
		return;
	}
	bh = sbi->s_zmap[zone];
	spin_lock(&bitmap_lock);
	if (!gbfs_test_and_clear_bit(bit, bh->b_data))
		printk("gbfs_free_block (%s:%lu): bit already cleared\n",
		       sb->s_id, block);
	spin_unlock(&bitmap_lock);
	mark_buffer_dirty(bh);
	return;
}

int gbfs_new_block(struct inode * inode)
{
	struct gbfs_sb_info *sbi = gbfs_sb(inode->i_sb);
	int bits_per_zone = 8 * inode->i_sb->s_blocksize;
	int i;

	for (i = 0; i < sbi->s_zmap_blocks; i++) {
		struct buffer_head *bh = sbi->s_zmap[i];
		int j;

		spin_lock(&bitmap_lock);
		j = gbfs_find_first_zero_bit(bh->b_data, bits_per_zone);
		if (j < bits_per_zone) {
			gbfs_set_bit(j, bh->b_data);
			spin_unlock(&bitmap_lock);
			mark_buffer_dirty(bh);
			j += i * bits_per_zone + sbi->s_firstdatazone-1;
			if (j < sbi->s_firstdatazone || j >= sbi->s_nzones)
				break;
			return j;
		}
		spin_unlock(&bitmap_lock);
	}
	return 0;
}

unsigned long gbfs_count_free_blocks(struct super_block *sb)
{
	struct gbfs_sb_info *sbi = gbfs_sb(sb);
	u32 bits = sbi->s_nzones - sbi->s_firstdatazone + 1;

	return (count_free(sbi->s_zmap, sb->s_blocksize, bits)
		<< sbi->s_log_zone_size);
}

static struct gbfs_inode *
minix_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh)
{
	int block;
	struct gbfs_sb_info *sbi = gbfs_sb(sb);
	struct gbfs_inode *p;
	int gbfs_inodes_per_block = sb->s_blocksize / sizeof(struct gbfs_inode);

	*bh = NULL;
	if (!ino || ino > sbi->s_ninodes) {
		printk("Bad inode number on dev %s: %ld is out of range\n",
		       sb->s_id, (long)ino);
		return NULL;
	}
	ino--;
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks +
		 ino / gbfs_inodes_per_block;
	*bh = sb_bread(sb, block);
	if (!*bh) {
		printk("Unable to read inode block\n");
		return NULL;
	}
	p = (void *)(*bh)->b_data;
	return p + ino % gbfs_inodes_per_block;
}

struct gbfs_inode *
gbfs_raw_inode(struct super_block *sb, ino_t ino)
{
	int err;
	struct gbfs_inode *raw_inode;
	struct gbfs_sb_info *sbi = gbfs_sb(sb);

	gbfs_inode_key_t key;
	DBT kdbt, vdbt;

	raw_inode = kmalloc(sizeof(struct gbfs_inode), GFP_KERNEL);
	if (!raw_inode) 
		return NULL;

	key.type = GBFS_DB_INODE;
	key.ino = ino;

	kdbt.data = &key;
	kdbt.size = sizeof(key);

	err = sbi->dbp->get(sbi->dbp, &kdbt, &vdbt, 0);
	if (!err) {
		BUG_ON(vdbt.size != sizeof(struct gbfs_inode));
		memcpy(raw_inode, vdbt.data, sizeof(struct gbfs_inode));
	} else if (err == 1) {
		if (ino == GBFS_ROOT_INO) {
			struct buffer_head *bh;
			struct gbfs_inode *p = minix_raw_inode(sb, ino, &bh);
			if (p) {
				*raw_inode = *p;
				brelse(bh);
			}
		} else {
			memset(raw_inode, 0, sizeof(struct gbfs_inode));
		}
	} else {
		kfree(raw_inode);
		raw_inode = NULL;
	}

	printk("GBFS: readinode: %lu %d\n", ino, err);
	return raw_inode;
}

/* Clear the link count and mode of a deleted inode on disk. */

static void gbfs_clear_inode(struct inode *inode)
{
	struct gbfs_inode *raw_inode;

	raw_inode = gbfs_raw_inode(inode->i_sb, inode->i_ino);
	if (raw_inode) {
		raw_inode->i_nlinks = 0;
		raw_inode->i_mode = 0;
		
		gbfs_update_inode(inode, raw_inode);
	}
}

void gbfs_free_inode(struct inode * inode)
{
	struct super_block *sb = inode->i_sb;
	struct gbfs_sb_info *sbi = gbfs_sb(inode->i_sb);
	struct buffer_head *bh;
	int k = sb->s_blocksize_bits + 3;
	unsigned long ino, bit;

	ino = inode->i_ino;
	if (ino < 1 || ino > sbi->s_ninodes) {
		printk("gbfs_free_inode: inode 0 or nonexistent inode\n");
		return;
	}
	bit = ino & ((1<<k) - 1);
	ino >>= k;
	if (ino >= sbi->s_imap_blocks) {
		printk("gbfs_free_inode: nonexistent imap in superblock\n");
		return;
	}

	gbfs_clear_inode(inode);	/* clear on-disk copy */

	bh = sbi->s_imap[ino];
	spin_lock(&bitmap_lock);
	if (!gbfs_test_and_clear_bit(bit, bh->b_data))
		printk("gbfs_free_inode: bit %lu already cleared\n", bit);
	spin_unlock(&bitmap_lock);
	mark_buffer_dirty(bh);
}

struct inode *gbfs_new_inode(const struct inode *dir, umode_t mode, int *error)
{
	struct super_block *sb = dir->i_sb;
	struct gbfs_sb_info *sbi = gbfs_sb(sb);
	struct inode *inode = new_inode(sb);
	struct buffer_head * bh;
	int bits_per_zone = 8 * sb->s_blocksize;
	unsigned long j;
	int i;

	if (!inode) {
		*error = -ENOMEM;
		return NULL;
	}
	j = bits_per_zone;
	bh = NULL;
	*error = -ENOSPC;
	spin_lock(&bitmap_lock);
	for (i = 0; i < sbi->s_imap_blocks; i++) {
		bh = sbi->s_imap[i];
		j = gbfs_find_first_zero_bit(bh->b_data, bits_per_zone);
		if (j < bits_per_zone)
			break;
	}
	if (!bh || j >= bits_per_zone) {
		spin_unlock(&bitmap_lock);
		iput(inode);
		return NULL;
	}
	if (gbfs_test_and_set_bit(j, bh->b_data)) {	/* shouldn't happen */
		spin_unlock(&bitmap_lock);
		printk("gbfs_new_inode: bit already set\n");
		iput(inode);
		return NULL;
	}
	spin_unlock(&bitmap_lock);
	mark_buffer_dirty(bh);
	j += i * bits_per_zone;
	if (!j || j > sbi->s_ninodes) {
		iput(inode);
		return NULL;
	}
	inode_init_owner(inode, dir, mode);
	inode->i_ino = j;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode->i_blocks = 0;
	memset(&gbfs_i(inode)->u, 0, sizeof(gbfs_i(inode)->u));
	insert_inode_hash(inode);
	mark_inode_dirty(inode);

	*error = 0;
	return inode;
}

unsigned long gbfs_count_free_inodes(struct super_block *sb)
{
	struct gbfs_sb_info *sbi = gbfs_sb(sb);
	u32 bits = sbi->s_ninodes + 1;

	return count_free(sbi->s_imap, sb->s_blocksize, bits);
}
