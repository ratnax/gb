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

#include <db.h>
#include "keys.h"

void
minix_raw_inode(struct gbfs_inode *inode)
{
	inode->i_nlinks = 2;
	inode->i_atime = inode->i_mtime = inode->i_ctime = jiffies;
	inode->i_size = 0;
	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = 0;
	inode->i_gid = 0;
}

struct gbfs_inode *
gbfs_raw_inode(struct super_block *sb, ino_t ino)
{
	int err;
	struct gbfs_inode *raw_inode;
	struct gbfs_sb_info *sbi = gbfs_sb(sb);

	gbfs_inode_key_t key;
	DBT kdbt, vdbt;

	memset(&kdbt, 0, sizeof(DBT));
	memset(&vdbt, 0, sizeof(DBT));	

	raw_inode = kmalloc(sizeof(struct gbfs_inode), GFP_KERNEL);
	if (!raw_inode) 
		return NULL;

	key.type = GBFS_DB_INODE;
	key.ino = ino;

	kdbt.data = &key;
	kdbt.size = sizeof(key);

	err = sbi->dbp->get(sbi->dbp, NULL, &kdbt, &vdbt, 0);
	if (!err) {
		BUG_ON(vdbt.size != sizeof(struct gbfs_inode));
		memcpy(raw_inode, vdbt.data, sizeof(struct gbfs_inode));
	} else if (err == DB_NOTFOUND) {
		if (ino == GBFS_ROOT_INO) {
			minix_raw_inode(raw_inode);
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
	gbfs_clear_inode(inode);	/* clear on-disk copy */
}

struct inode *gbfs_new_inode(const struct inode *dir, umode_t mode, int *error)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode = new_inode(sb);
	static unsigned long ino = 5;
	static DEFINE_SPINLOCK(lock);

	if (!inode) {
		*error = -ENOMEM;
		return NULL;
	}
	inode_init_owner(inode, dir, mode);

	spin_lock(&lock);
	inode->i_ino = ino++;
	spin_unlock(&lock);

	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode->i_blocks = 0;
	insert_inode_hash(inode);
	mark_inode_dirty(inode);

	*error = 0;
	return inode;
}
