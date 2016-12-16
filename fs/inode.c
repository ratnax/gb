/*
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */

/*
 * NOTE! This filesystem is probably most useful
 * not as a real filesystem, but as an example of
 * how virtual filesystems can be written.
 *
 * It doesn't get much simpler than this. Consider
 * that this file implements the full semantics of
 * a POSIX-compliant read-write filesystem.
 *
 * Note in particular how the filesystem does not
 * need to implement any data structures of its own
 * to keep track of the virtual data: using the VFS
 * caches is sufficient.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "gbfs.h"
#include "internal.h"
#include "db.h"

#define RAMFS_DEFAULT_MODE	0755

static const struct super_operations gbfs_ops;
static const struct inode_operations gbfs_dir_inode_operations;

struct gbfs_mount_opts {
	umode_t mode;
};

struct gbfs_fs_info {
	struct gbfs_mount_opts mount_opts;
	struct backing_dev_info bdi;
	DB *dbp;
};

typedef struct gbfs_key_s {
	ino_t ino;
	loff_t off;
} gbfs_key_t;

static int gbfs_writepage(struct page *page, struct writeback_control *wbc)
{
	int err;
	struct inode *inode = page->mapping->host;
	struct gbfs_fs_info *fsi = inode->i_sb->s_fs_info;
	gbfs_key_t key;
	DBT kdbt, vdbt;

	key.ino = inode->i_ino;
	key.off = page_offset(page);

	kdbt.data = &key;
	kdbt.size = sizeof(key);

	vdbt.data = page_address(page);
	vdbt.size = PAGE_SIZE;

    set_page_writeback(page);

	err = fsi->dbp->put(fsi->dbp, &kdbt, &vdbt, 0); 
    end_page_writeback(page);
    unlock_page(page);
	return err;
}

static int gbfs_readpage(struct file *file, struct page *page)
{
	int err;
	struct inode *inode = page->mapping->host;
	struct gbfs_fs_info *fsi = inode->i_sb->s_fs_info;
	gbfs_key_t key;
	DBT kdbt, vdbt;

	key.ino = inode->i_ino;
	key.off = page_offset(page);

	kdbt.data = &key;
	kdbt.size = sizeof(key);

	err = fsi->dbp->get(fsi->dbp, &kdbt, &vdbt, 0);
	unlock_page(page);
	return err;	
}

static const struct address_space_operations gbfs_aops = {
	.readpage	= gbfs_readpage,
	.writepage	= gbfs_writepage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
};

struct inode *gbfs_get_inode(struct super_block *sb,
				const struct inode *dir, umode_t mode, dev_t dev)
{
	struct inode * inode = new_inode(sb);

	if (inode) {
		inode->i_ino = get_next_ino();
		inode_init_owner(inode, dir, mode);
		inode->i_mapping->a_ops = &gbfs_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &gbfs_file_inode_operations;
			inode->i_fop = &gbfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &gbfs_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;

			/* directory inodes start off with i_nlink == 2 (for "." entry) */
			inc_nlink(inode);
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			inode_nohighmem(inode);
			break;
		}
	}
	return inode;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
/* SMP-safe */
static int
gbfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	struct inode * inode = gbfs_get_inode(dir->i_sb, dir, mode, dev);
	int error = -ENOSPC;

	if (inode) {
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
		dir->i_mtime = dir->i_ctime = current_time(dir);
	}
	return error;
}

static int gbfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode)
{
	int retval = gbfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		inc_nlink(dir);
	return retval;
}

static int gbfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	return gbfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int gbfs_symlink(struct inode * dir, struct dentry *dentry, const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = gbfs_get_inode(dir->i_sb, dir, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
		error = page_symlink(inode, symname, l);
		if (!error) {
			d_instantiate(dentry, inode);
			dget(dentry);
			dir->i_mtime = dir->i_ctime = current_time(dir);
		} else
			iput(inode);
	}
	return error;
}

static const struct inode_operations gbfs_dir_inode_operations = {
	.create		= gbfs_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= simple_unlink,
	.symlink	= gbfs_symlink,
	.mkdir		= gbfs_mkdir,
	.rmdir		= simple_rmdir,
	.mknod		= gbfs_mknod,
	.rename		= simple_rename,
};

static const struct super_operations gbfs_ops = {
	.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
	.show_options	= generic_show_options,
};

enum {
	Opt_mode,
	Opt_err
};

static const match_table_t tokens = {
	{Opt_mode, "mode=%o"},
	{Opt_err, NULL}
};

static int gbfs_parse_options(char *data, struct gbfs_mount_opts *opts)
{
	substring_t args[MAX_OPT_ARGS];
	int option;
	int token;
	char *p;

	opts->mode = RAMFS_DEFAULT_MODE;

	while ((p = strsep(&data, ",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_mode:
			if (match_octal(&args[0], &option))
				return -EINVAL;
			opts->mode = option & S_IALLUGO;
			break;
		/*
		 * We might like to report bad mount options here;
		 * but traditionally gbfs has ignored all mount options,
		 * and as it is used as a !CONFIG_SHMEM simple substitute
		 * for tmpfs, better continue to ignore other mount options.
		 */
		}
	}

	return 0;
}

int gbfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct gbfs_fs_info *fsi;
	struct inode *inode;
	int err;

	save_mount_options(sb, data);

	fsi = kzalloc(sizeof(struct gbfs_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi)
		return -ENOMEM;

	err = gbfs_parse_options(data, &fsi->mount_opts);
	if (err)
		return err;

	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_SIZE;
	sb->s_blocksize_bits	= PAGE_SHIFT;
	sb->s_magic		= RAMFS_MAGIC;
	sb->s_op		= &gbfs_ops;
	sb->s_time_gran		= 1;


	fsi->bdi.name = "gbfs";
	fsi->bdi.ra_pages = (VM_MAX_READAHEAD * 1024) / PAGE_SIZE;

	err = bdi_init(&fsi->bdi);
	if (err)
		return err;

	err = bdi_register_dev(&fsi->bdi, sb->s_dev);
	if (err)
		return err;

	sb->s_bdi = &fsi->bdi;

	inode = gbfs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	fsi->dbp = dbopen("/media/x/gbdev", O_CREAT|O_RDWR, 
			S_IRUSR | S_IWUSR, DB_BTREE, NULL);
	printk("dbopen: %p\n", fsi->dbp);
	
	return 0;
}

struct dentry *gbfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags, data, gbfs_fill_super);
}

static void gbfs_kill_sb(struct super_block *sb)
{
	if (sb->s_bdi)
		bdi_destroy(sb->s_bdi);
	kfree(sb->s_fs_info);
	kill_litter_super(sb);
}

static struct file_system_type gbfs_fs_type = {
	.owner 		= THIS_MODULE,
	.name		= "gbfs",
	.mount		= gbfs_mount,
	.kill_sb	= gbfs_kill_sb,
	.fs_flags	= FS_USERNS_MOUNT,
};

/*
 * Get things set up.
 */
static int __init gbfs_init(void)
{
	return register_filesystem(&gbfs_fs_type);
}

static void __exit gbfs_exit(void)
{
	unregister_filesystem(&gbfs_fs_type);
}

module_init(gbfs_init);
module_exit(gbfs_exit);
