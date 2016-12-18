/*
 *  linux/fs/gbfs/file.c
 *
 *  Copyright (C) 1991, 1992 Linus Torvalds
 *
 *  gbfs regular file handling primitives
 */

#include "gbfs.h"

/*
 * We have mostly NULLs here: the current defaults are OK for
 * the gbfs filesystem.
 */
const struct file_operations gbfs_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
	.fsync		= generic_file_fsync,
	.splice_read	= generic_file_splice_read,
};

static int gbfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	int error;

	error = setattr_prepare(dentry, attr);
	if (error)
		return error;

	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size != i_size_read(inode)) {
		error = inode_newsize_ok(inode, attr->ia_size);
		if (error)
			return error;

		truncate_setsize(inode, attr->ia_size);
		gbfs_truncate(inode);
	}

	setattr_copy(inode, attr);
	mark_inode_dirty(inode);
	return 0;
}

const struct inode_operations gbfs_file_inode_operations = {
	.setattr	= gbfs_setattr,
	.getattr	= gbfs_getattr,
};
