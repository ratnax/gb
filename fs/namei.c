/*
 *  linux/fs/gbfs/namei.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include "gbfs.h"

static int add_nondir(struct dentry *dentry, struct inode *inode)
{
	int err = gbfs_add_link(dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}
	inode_dec_link_count(inode);
	iput(inode);
	return err;
}

static struct dentry *gbfs_lookup(struct inode * dir, struct dentry *dentry, unsigned int flags)
{
	struct inode * inode = NULL;
	ino_t ino;

	if (dentry->d_name.len > gbfs_sb(dir->i_sb)->s_namelen)
		return ERR_PTR(-ENAMETOOLONG);

	ino = gbfs_inode_by_name(dentry);
	if (ino) {
		inode = gbfs_iget(dir->i_sb, ino);
		if (IS_ERR(inode))
			return ERR_CAST(inode);
	}
	d_add(dentry, inode);
	return NULL;
}

static int gbfs_mknod(struct inode * dir, struct dentry *dentry, umode_t mode, dev_t rdev)
{
	int error;
	struct inode *inode;

	if (!old_valid_dev(rdev))
		return -EINVAL;

	inode = gbfs_new_inode(dir, mode, &error);

	if (inode) {
		gbfs_set_inode(inode, rdev);
		mark_inode_dirty(inode);
		error = add_nondir(dentry, inode);
	}
	return error;
}

static int gbfs_tmpfile(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int error;
	struct inode *inode = gbfs_new_inode(dir, mode, &error);
	if (inode) {
		gbfs_set_inode(inode, 0);
		mark_inode_dirty(inode);
		d_tmpfile(dentry, inode);
	}
	return error;
}

static int gbfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		bool excl)
{
	return gbfs_mknod(dir, dentry, mode, 0);
}

static int gbfs_symlink(struct inode * dir, struct dentry *dentry,
	  const char * symname)
{
	int err = -ENAMETOOLONG;
	int i = strlen(symname)+1;
	struct inode * inode;

	if (i > dir->i_sb->s_blocksize)
		goto out;

	inode = gbfs_new_inode(dir, S_IFLNK | 0777, &err);
	if (!inode)
		goto out;

	gbfs_set_inode(inode, 0);
	err = page_symlink(inode, symname, i);
	if (err)
		goto out_fail;

	err = add_nondir(dentry, inode);
out:
	return err;

out_fail:
	inode_dec_link_count(inode);
	iput(inode);
	goto out;
}

static int gbfs_link(struct dentry * old_dentry, struct inode * dir,
	struct dentry *dentry)
{
	struct inode *inode = d_inode(old_dentry);

	inode->i_ctime = current_time(inode);
	inode_inc_link_count(inode);
	ihold(inode);
	return add_nondir(dentry, inode);
}

static int gbfs_mkdir(struct inode * dir, struct dentry *dentry, umode_t mode)
{
	struct inode * inode;
	int err;

	inode_inc_link_count(dir);

	inode = gbfs_new_inode(dir, S_IFDIR | mode, &err);
	if (!inode)
		goto out_dir;

	gbfs_set_inode(inode, 0);

	inode_inc_link_count(inode);

	err = gbfs_make_empty(inode, dir);
	if (err)
		goto out_fail;

	err = gbfs_add_link(dentry, inode);
	if (err)
		goto out_fail;

	d_instantiate(dentry, inode);
out:
	return err;

out_fail:
	inode_dec_link_count(inode);
	inode_dec_link_count(inode);
	iput(inode);
out_dir:
	inode_dec_link_count(dir);
	goto out;
}

static int gbfs_unlink(struct inode * dir, struct dentry *dentry)
{
	int err = -ENOENT;
	struct inode * inode = d_inode(dentry);
	struct page * page;
	struct gbfs_dir_entry * de;

	de = gbfs_find_entry(dentry, &page);
	if (!de)
		goto end_unlink;

	err = gbfs_delete_entry(de, page);
	if (err)
		goto end_unlink;

	inode->i_ctime = dir->i_ctime;
	inode_dec_link_count(inode);
end_unlink:
	return err;
}

static int gbfs_rmdir(struct inode * dir, struct dentry *dentry)
{
	struct inode * inode = d_inode(dentry);
	int err = -ENOTEMPTY;

	if (gbfs_empty_dir(inode)) {
		err = gbfs_unlink(dir, dentry);
		if (!err) {
			inode_dec_link_count(dir);
			inode_dec_link_count(inode);
		}
	}
	return err;
}

static int gbfs_rename(struct inode * old_dir, struct dentry *old_dentry,
			struct inode * new_dir, struct dentry *new_dentry,
			unsigned int flags)
{
	struct inode * old_inode = d_inode(old_dentry);
	struct inode * new_inode = d_inode(new_dentry);
	struct page * dir_page = NULL;
	struct gbfs_dir_entry * dir_de = NULL;
	struct page * old_page;
	struct gbfs_dir_entry * old_de;
	int err = -ENOENT;

	if (flags & ~RENAME_NOREPLACE)
		return -EINVAL;

	old_de = gbfs_find_entry(old_dentry, &old_page);
	if (!old_de)
		goto out;

	if (S_ISDIR(old_inode->i_mode)) {
		err = -EIO;
		dir_de = gbfs_dotdot(old_inode, &dir_page);
		if (!dir_de)
			goto out_old;
	}

	if (new_inode) {
		struct page * new_page;
		struct gbfs_dir_entry * new_de;

		err = -ENOTEMPTY;
		if (dir_de && !gbfs_empty_dir(new_inode))
			goto out_dir;

		err = -ENOENT;
		new_de = gbfs_find_entry(new_dentry, &new_page);
		if (!new_de)
			goto out_dir;
		gbfs_set_link(new_de, new_page, old_inode);
		new_inode->i_ctime = current_time(new_inode);
		if (dir_de)
			drop_nlink(new_inode);
		inode_dec_link_count(new_inode);
	} else {
		err = gbfs_add_link(new_dentry, old_inode);
		if (err)
			goto out_dir;
		if (dir_de)
			inode_inc_link_count(new_dir);
	}

	gbfs_delete_entry(old_de, old_page);
	mark_inode_dirty(old_inode);

	if (dir_de) {
		gbfs_set_link(dir_de, dir_page, new_dir);
		inode_dec_link_count(old_dir);
	}
	return 0;

out_dir:
	if (dir_de) {
		kunmap(dir_page);
		put_page(dir_page);
	}
out_old:
	kunmap(old_page);
	put_page(old_page);
out:
	return err;
}

/*
 * directories can handle most operations...
 */
const struct inode_operations gbfs_dir_inode_operations = {
	.create		= gbfs_create,
	.lookup		= gbfs_lookup,
	.link		= gbfs_link,
	.unlink		= gbfs_unlink,
	.symlink	= gbfs_symlink,
	.mkdir		= gbfs_mkdir,
	.rmdir		= gbfs_rmdir,
	.mknod		= gbfs_mknod,
	.rename		= gbfs_rename,
	.getattr	= gbfs_getattr,
	.tmpfile	= gbfs_tmpfile,
};
