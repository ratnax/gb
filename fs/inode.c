/*
 *  linux/fs/gbfs/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Copyright (C) 1996  Gertjan van Wingerde
 *	Minix V2 fs support.
 *
 *  Modified for 680x0 by Andreas Schwab
 *  Updated to filesystem version 3 by Daniel Aragones
 */

#include <linux/module.h>
#include "gbfs.h"
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/highuid.h>
#include <linux/vfs.h>
#include <linux/writeback.h>

#include "keys.h"

static int gbfs_write_inode(struct inode *inode,
		struct writeback_control *wbc);
static int gbfs_statfs(struct dentry *dentry, struct kstatfs *buf);
static int gbfs_remount (struct super_block * sb, int * flags, char * data);

static void gbfs_evict_inode(struct inode *inode)
{
	truncate_inode_pages_final(&inode->i_data);
	if (!inode->i_nlink) {
		inode->i_size = 0;
		gbfs_truncate(inode);
	}
	invalidate_inode_buffers(inode);
	clear_inode(inode);
	if (!inode->i_nlink)
		gbfs_free_inode(inode);
}

static void gbfs_put_super(struct super_block *sb)
{
	int i;
	struct gbfs_sb_info *sbi = gbfs_sb(sb);

	if (sbi->dbp) {
		int ret;
	   
		ret	= sbi->dbp->sync(sbi->dbp, 0);
		printk("gbfs: btree sync status (%d).\n", ret);

		ret = sbi->dbp->close(sbi->dbp);
		printk("gbfs: btree close status (%d).\n", ret);
	}
	if (!(sb->s_flags & MS_RDONLY)) {
		mark_buffer_dirty(sbi->s_sbh);
	}
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	brelse (sbi->s_sbh);
	kfree(sbi->s_imap);
	sb->s_fs_info = NULL;
	kfree(sbi);
}

static struct kmem_cache * gbfs_inode_cachep;

static struct inode *gbfs_alloc_inode(struct super_block *sb)
{
	struct gbfs_inode_info *ei;
	ei = kmem_cache_alloc(gbfs_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}

static void gbfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(gbfs_inode_cachep, gbfs_i(inode));
}

static void gbfs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, gbfs_i_callback);
}

static void init_once(void *foo)
{
	struct gbfs_inode_info *ei = (struct gbfs_inode_info *) foo;

	inode_init_once(&ei->vfs_inode);
}

static int __init init_inodecache(void)
{
	gbfs_inode_cachep = kmem_cache_create("gbfs_inode_cache",
					     sizeof(struct gbfs_inode_info),
					     0, (SLAB_RECLAIM_ACCOUNT|
						SLAB_MEM_SPREAD|SLAB_ACCOUNT),
					     init_once);
	if (gbfs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	/*
	 * Make sure all delayed rcu free inodes are flushed before we
	 * destroy cache.
	 */
	rcu_barrier();
	kmem_cache_destroy(gbfs_inode_cachep);
}

static const struct super_operations gbfs_sops = {
	.alloc_inode	= gbfs_alloc_inode,
	.destroy_inode	= gbfs_destroy_inode,
	.write_inode	= gbfs_write_inode,
	.evict_inode	= gbfs_evict_inode,
	.put_super	= gbfs_put_super,
	.statfs		= gbfs_statfs,
	.remount_fs	= gbfs_remount,
};

static int gbfs_remount (struct super_block * sb, int * flags, char * data)
{
	struct gbfs_sb_info * sbi = gbfs_sb(sb);
	struct gbfs_super_block * ms;

	sync_filesystem(sb);
	ms = sbi->s_ms;
	if ((*flags & MS_RDONLY) == (sb->s_flags & MS_RDONLY))
		return 0;
	if (*flags & MS_RDONLY) {
		if (!(sbi->s_mount_state & GBFS_VALID_FS))
			return 0;
		/* Mounting a rw partition read-only. */
		mark_buffer_dirty(sbi->s_sbh);
	} else {
	  	/* Mount a partition which is read-only, read-write. */
		sbi->s_mount_state = GBFS_VALID_FS;
		mark_buffer_dirty(sbi->s_sbh);

		if (!(sbi->s_mount_state & GBFS_VALID_FS))
			printk("GBFS-fs warning: remounting unchecked fs, "
				"running fsck is recommended\n");
		else if ((sbi->s_mount_state & GBFS_ERROR_FS))
			printk("GBFS-fs warning: remounting fs with errors, "
				"running fsck is recommended\n");
	}
	return 0;
}

static int gbfs_fill_super(struct super_block *s, void *data, int silent)
{
	struct buffer_head *bh;
	struct buffer_head **map;
	struct gbfs_super_block *ms;
	unsigned long i, block;
	struct inode *root_inode;
	struct gbfs_sb_info *sbi;
	int ret = -EINVAL;

	sbi = kzalloc(sizeof(struct gbfs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	s->s_fs_info = sbi;

	if (!sb_set_blocksize(s, BLOCK_SIZE))
		goto out_bad_hblock;

	if (!(bh = sb_bread(s, 1)))
		goto out_bad_sb;

	ms = (struct gbfs_super_block *) bh->b_data;
	sbi->s_ms = ms;
	sbi->s_sbh = bh;
	if (ms->s_magic == GBFS_SUPER_MAGIC) {
		s->s_magic = ms->s_magic;
		sbi->s_imap_blocks = ms->s_imap_blocks;
		sbi->s_zmap_blocks = ms->s_zmap_blocks;
		sbi->s_firstdatazone = ms->s_firstdatazone;
		sbi->s_log_zone_size = ms->s_log_zone_size;
		sbi->s_max_size = ms->s_max_size;
		sbi->s_ninodes = ms->s_ninodes;
		sbi->s_nzones = ms->s_zones;
		sbi->s_dirsize = 64;
		sbi->s_namelen = 60;
		sbi->s_mount_state = GBFS_VALID_FS;
		sb_set_blocksize(s, ms->s_blocksize);
		s->s_max_links = GBFS_LINK_MAX;
	} else
		goto out_no_fs;


	sbi->dbp = dbopen("/media/x/gbdev", O_CREAT|O_RDWR, 
			S_IRUSR | S_IWUSR, DB_BTREE, NULL);
	printk("dbopen: %p\n", sbi->dbp);
	/*
	 * Allocate the buffer map to keep the superblock small.
	 */
	if (sbi->s_imap_blocks == 0 || sbi->s_zmap_blocks == 0)
		goto out_illegal_sb;
	i = (sbi->s_imap_blocks + sbi->s_zmap_blocks) * sizeof(bh);
	map = kzalloc(i, GFP_KERNEL);
	if (!map)
		goto out_no_map;
	sbi->s_imap = &map[0];
	sbi->s_zmap = &map[sbi->s_imap_blocks];

	block=2;
	for (i=0 ; i < sbi->s_imap_blocks ; i++) {
		if (!(sbi->s_imap[i]=sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}
	for (i=0 ; i < sbi->s_zmap_blocks ; i++) {
		if (!(sbi->s_zmap[i]=sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}

	gbfs_set_bit(0,sbi->s_imap[0]->b_data);
	gbfs_set_bit(0,sbi->s_zmap[0]->b_data);

	/* Apparently gbfs can create filesystems that allocate more blocks for
	 * the bitmaps than needed.  We simply ignore that, but verify it didn't
	 * create one with not enough blocks and bail out if so.
	 */
	block = gbfs_blocks_needed(sbi->s_ninodes, s->s_blocksize);
	if (sbi->s_imap_blocks < block) {
		printk("GBFS-fs: file system does not have enough "
				"imap blocks allocated.  Refusing to mount.\n");
		goto out_no_bitmap;
	}

	block = gbfs_blocks_needed(
			(sbi->s_nzones - sbi->s_firstdatazone + 1),
			s->s_blocksize);
	if (sbi->s_zmap_blocks < block) {
		printk("GBFS-fs: file system does not have enough "
				"zmap blocks allocated.  Refusing to mount.\n");
		goto out_no_bitmap;
	}

	/* set up enough so that it can read an inode */
	s->s_op = &gbfs_sops;
	root_inode = gbfs_iget(s, GBFS_ROOT_INO);
	if (IS_ERR(root_inode)) {
		ret = PTR_ERR(root_inode);
		goto out_no_root;
	}

	ret = -ENOMEM;
	s->s_root = d_make_root(root_inode);
	if (!s->s_root)
		goto out_no_root;

	if (!(s->s_flags & MS_RDONLY)) {
		mark_buffer_dirty(bh);
	}
	if (!(sbi->s_mount_state & GBFS_VALID_FS))
		printk("GBFS-fs: mounting unchecked file system, "
			"running fsck is recommended\n");
	else if (sbi->s_mount_state & GBFS_ERROR_FS)
		printk("GBFS-fs: mounting file system with errors, "
			"running fsck is recommended\n");

	return 0;

out_no_root:
	if (!silent)
		printk("GBFS-fs: get root inode failed\n");
	goto out_freemap;

out_no_bitmap:
	printk("GBFS-fs: bad superblock or unable to read bitmaps\n");
out_freemap:
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	kfree(sbi->s_imap);
	goto out_release;

out_no_map:
	ret = -ENOMEM;
	if (!silent)
		printk("GBFS-fs: can't allocate map\n");
	goto out_release;

out_illegal_sb:
	if (!silent)
		printk("GBFS-fs: bad superblock\n");
	goto out_release;

out_no_fs:
	if (!silent)
		printk("VFS: Can't find a Minix filesystem V1 | V2 | V3 "
		       "on device %s.\n", s->s_id);
out_release:
	brelse(bh);
	goto out;

out_bad_hblock:
	printk("GBFS-fs: blocksize too small for device\n");
	goto out;

out_bad_sb:
	printk("GBFS-fs: unable to read superblock\n");
out:
	s->s_fs_info = NULL;
	kfree(sbi);
	return ret;
}

static int gbfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	struct gbfs_sb_info *sbi = gbfs_sb(sb);
	u64 id = huge_encode_dev(sb->s_bdev->bd_dev);
	buf->f_type = sb->s_magic;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = (sbi->s_nzones - sbi->s_firstdatazone) << sbi->s_log_zone_size;
	buf->f_bfree = gbfs_count_free_blocks(sb);
	buf->f_bavail = buf->f_bfree;
	buf->f_files = sbi->s_ninodes;
	buf->f_ffree = gbfs_count_free_inodes(sb);
	buf->f_namelen = sbi->s_namelen;
	buf->f_fsid.val[0] = (u32)id;
	buf->f_fsid.val[1] = (u32)(id >> 32);

	return 0;
}

static int gbfs_get_block(struct inode *inode, sector_t block,
		    struct buffer_head *bh_result, int create)
{
	return __gbfs_get_block(inode, block, bh_result, create);
}

static int gbfs_writepage(struct page *page, struct writeback_control *wbc)
{
	int err;
	struct inode *inode = page->mapping->host;
	struct gbfs_sb_info *sbi = gbfs_sb(inode->i_sb);

	gbfs_data_key_t key;
	DBT kdbt, vdbt;

	key.type = GBFS_DB_PAGE;
	key.ino = inode->i_ino;
	key.pgoff = page_offset(page);

	kdbt.data = &key;
	kdbt.size = sizeof(key);

	kmap(page);
	vdbt.data = page_address(page);
	vdbt.size = PAGE_SIZE;

    set_page_writeback(page);

	err = sbi->dbp->put(sbi->dbp, &kdbt, &vdbt, 0); 
    end_page_writeback(page);
    unlock_page(page);

	printk(KERN_ERR "GBFS: writepage: %lu %llu %d\n", inode->i_ino, 
				page_offset(page), err);
	kunmap(page);
	return err;
}

static int gbfs_readpage(struct file *file, struct page *page)
{
	int err;
	struct inode *inode = page->mapping->host;
	struct gbfs_sb_info *sbi = gbfs_sb(inode->i_sb);

	gbfs_data_key_t key;
	DBT kdbt, vdbt;

	key.type = GBFS_DB_PAGE;
	key.ino = inode->i_ino;
	key.pgoff = page_offset(page);

	kdbt.data = &key;
	kdbt.size = sizeof(key);

	kmap(page);
	err = sbi->dbp->get(sbi->dbp, &kdbt, &vdbt, 0);
	if (!err) {
		memcpy(page_address(page), vdbt.data, PAGE_SIZE);
		SetPageUptodate(page);
	} else if (err == 1) {
		memset(page_address(page), 0, PAGE_SIZE);
		SetPageUptodate(page);
		err = 0;
	}
	unlock_page(page);

	printk(KERN_ERR "GBFS: readpage: %lu %llu %d\n", inode->i_ino, 
			page_offset(page), err);
	kunmap(page);
	return err;	
}

int gbfs_prepare_chunk(struct page *page, loff_t pos, unsigned len)
{
	return __block_write_begin(page, pos, len, gbfs_get_block);
}

static void gbfs_write_failed(struct address_space *mapping, loff_t to)
{
	struct inode *inode = mapping->host;

	if (to > inode->i_size) {
		truncate_pagecache(inode, inode->i_size);
		gbfs_truncate(inode);
	}
}

static int gbfs_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	int ret;

	ret = block_write_begin(mapping, pos, len, flags, pagep,
				gbfs_get_block);
	if (unlikely(ret))
		gbfs_write_failed(mapping, pos + len);

	return ret;
}

static sector_t gbfs_bmap(struct address_space *mapping, sector_t block)
{
	return generic_block_bmap(mapping,block,gbfs_get_block);
}

static const struct address_space_operations gbfs_aops = {
	.readpage = gbfs_readpage,
	.writepage = gbfs_writepage,
	.write_begin = gbfs_write_begin,
	.write_end = generic_write_end,
	.bmap = gbfs_bmap
};

static const struct inode_operations gbfs_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.get_link	= page_get_link,
	.getattr	= gbfs_getattr,
};

void gbfs_set_inode(struct inode *inode, dev_t rdev)
{
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &gbfs_file_inode_operations;
		inode->i_fop = &gbfs_file_operations;
		inode->i_mapping->a_ops = &gbfs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &gbfs_dir_inode_operations;
		inode->i_fop = &gbfs_dir_operations;
		inode->i_mapping->a_ops = &gbfs_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &gbfs_symlink_inode_operations;
		inode_nohighmem(inode);
		inode->i_mapping->a_ops = &gbfs_aops;
	} else
		init_special_inode(inode, inode->i_mode, rdev);
}

/*
 * The gbfs V2 function to read an inode.
 */
static struct inode *__gbfs_iget(struct inode *inode)
{
	struct gbfs_inode * raw_inode;
	struct gbfs_inode_info *gbfs_inode = gbfs_i(inode);
	int i;

	raw_inode = gbfs_raw_inode(inode->i_sb, inode->i_ino);
	if (!raw_inode) {
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}
	inode->i_mode = raw_inode->i_mode;
	i_uid_write(inode, raw_inode->i_uid);
	i_gid_write(inode, raw_inode->i_gid);
	set_nlink(inode, raw_inode->i_nlinks);
	inode->i_size = raw_inode->i_size;
	inode->i_mtime.tv_sec = raw_inode->i_mtime;
	inode->i_atime.tv_sec = raw_inode->i_atime;
	inode->i_ctime.tv_sec = raw_inode->i_ctime;
	inode->i_mtime.tv_nsec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_ctime.tv_nsec = 0;
	inode->i_blocks = 0;
	for (i = 0; i < 10; i++)
		gbfs_inode->u.i2_data[i] = raw_inode->i_zone[i];
	gbfs_set_inode(inode, old_decode_dev(raw_inode->i_zone[0]));
	unlock_new_inode(inode);
	kfree(raw_inode);
	return inode;
}

/*
 * The global function to read an inode.
 */
struct inode *gbfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	return __gbfs_iget(inode);
}

int gbfs_update_inode(struct inode *inode, struct gbfs_inode *raw_inode)
{
	int err;
	struct gbfs_sb_info *sbi = gbfs_sb(inode->i_sb);

	gbfs_inode_key_t key;
	DBT kdbt, vdbt;

	key.type = GBFS_DB_INODE;
	key.ino = inode->i_ino;

	kdbt.data = &key;
	kdbt.size = sizeof(key);

	vdbt.data = raw_inode;
	vdbt.size = sizeof(struct gbfs_inode);

	err = sbi->dbp->put(sbi->dbp, &kdbt, &vdbt, 0); 

	printk(KERN_ERR "GBFS: writeinode: %lu %d\n", inode->i_ino, err); 
	kfree(raw_inode);
	return err;
}

static int gbfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct gbfs_inode_info *gbfs_inode = gbfs_i(inode);
	struct gbfs_inode * raw_inode;
	int i;

	raw_inode = gbfs_raw_inode(inode->i_sb, inode->i_ino);
	if (!raw_inode)
		return -EIO;

	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid = fs_high2lowuid(i_uid_read(inode));
	raw_inode->i_gid = fs_high2lowgid(i_gid_read(inode));
	raw_inode->i_nlinks = inode->i_nlink;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_mtime = inode->i_mtime.tv_sec;
	raw_inode->i_atime = inode->i_atime.tv_sec;
	raw_inode->i_ctime = inode->i_ctime.tv_sec;
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		raw_inode->i_zone[0] = old_encode_dev(inode->i_rdev);
	else for (i = 0; i < 10; i++)
		raw_inode->i_zone[i] = gbfs_inode->u.i2_data[i];

	return gbfs_update_inode(inode, raw_inode);
}

int gbfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	struct super_block *sb = dentry->d_sb;
	generic_fillattr(d_inode(dentry), stat);
	stat->blocks = (sb->s_blocksize / 512) * gbfs_blocks(stat->size, sb);
	stat->blksize = sb->s_blocksize;
	return 0;
}

/*
 * The function that is called for file truncation.
 */
void gbfs_truncate(struct inode * inode)
{
	if (!(S_ISREG(inode->i_mode) ||
		S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)))
		return;

	__gbfs_truncate(inode);
}

static struct dentry *gbfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, gbfs_fill_super);
}

static struct file_system_type gbfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "gbfs",
	.mount		= gbfs_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("gbfs");

static int __init init_gbfs_fs(void)
{
	int err = init_inodecache();
	if (err)
		goto out1;
	err = register_filesystem(&gbfs_fs_type);
	if (err)
		goto out;
	return 0;
out:
	destroy_inodecache();
out1:
	return err;
}

static void __exit exit_gbfs_fs(void)
{
        unregister_filesystem(&gbfs_fs_type);
	destroy_inodecache();
}

module_init(init_gbfs_fs)
module_exit(exit_gbfs_fs)
MODULE_LICENSE("GPL");

