/*
 "sys_getpid"
 "sys_stat"  
 "sys_rmdir"  
 "sys_select"  
 "sys_fchmod"  
 "sys_rename"  
 "sys_fcntl"  
 "sys_pwrite64"  
 "sys_fdatasync"  
 "sys_unlink"  
 "sys_write"  
 "sys_old_readdir"  
 "sys_open"  
 "sys_fsync"  
 "sys_chmod"  
 "sys_fstat"  
 "sys_gettid"  
 "sys_read"  
 "sys_mkdir"  
 "sys_getuid"  
 "sys_lseek"  
 "sys_pread64"  
*/
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/security.h>
#include <linux/sched/rt.h>
#include <linux/poll.h>
#include <linux/fdtable.h>
#include <linux/vmalloc.h>
#include <linux/audit.h>
#include <linux/hashtable.h>
#include <linux/fsnotify.h>

#include <asm/uaccess.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

extern int errno;
/* sys_getpid */
pid_t getpid(void) 
{
	return task_tgid_vnr(current);
}

/* sys_stat */
int stat(const char *pathname, struct stat *st)
{
	mm_segment_t old_fs;
	struct kstat ks;
	ssize_t res;

	old_fs = get_fs();
	set_fs(get_ds());
	res = vfs_stat(pathname, &ks);
	set_fs(old_fs);

	if (res == 0) {
		st->st_dev = ks.dev;
		st->st_ino = ks.ino;
		st->st_mode = ks.mode;
		st->st_nlink = ks.nlink;
		st->st_uid = ks.uid.val;
		st->st_gid = ks.gid.val;
		st->st_rdev = ks.rdev;
		st->st_size = ks.size;
		st->st_blksize = ks.blksize;
		st->st_blocks = ks.blocks;
		st->st_atime = ks.atime.tv_sec;
		st->st_mtime = ks.mtime.tv_sec;
		st->st_ctime = ks.ctime.tv_sec;
		st->st_atime_nsec = ks.atime.tv_nsec;
		st->st_mtime_nsec = ks.mtime.tv_nsec;
		st->st_ctime_nsec = ks.ctime.tv_nsec;
	} else {
		errno = -res;
	}
	return res;
}

/* sys_unlink */
static inline int d_revalidate(struct dentry *dentry, unsigned int flags)
{
	return dentry->d_op->d_revalidate(dentry, flags);
}

static struct dentry *lookup_dcache(const struct qstr *name,
				    struct dentry *dir,
				    unsigned int flags)
{
	struct dentry *dentry;
	int error;

	dentry = d_lookup(dir, name);
	if (dentry) {
		if (dentry->d_flags & DCACHE_OP_REVALIDATE) {
			error = d_revalidate(dentry, flags);
			if (unlikely(error <= 0)) {
				if (!error)
					d_invalidate(dentry);
				dput(dentry);
				return ERR_PTR(error);
			}
		}
	}
	return dentry;
}

/*
 * Call i_op->lookup on the dentry.  The dentry must be negative and
 * unhashed.
 *
 * dir->d_inode->i_mutex must be held
 */
static struct dentry *lookup_real(struct inode *dir, struct dentry *dentry,
				  unsigned int flags)
{
	struct dentry *old;

	/* Don't create child dentry for a dead directory. */
	if (unlikely(IS_DEADDIR(dir))) {
		dput(dentry);
		return ERR_PTR(-ENOENT);
	}

	old = dir->i_op->lookup(dir, dentry, flags);
	if (unlikely(old)) {
		dput(dentry);
		dentry = old;
	}
	return dentry;
}

static struct dentry *__lookup_hash(const struct qstr *name,
		struct dentry *base, unsigned int flags)
{
	struct dentry *dentry = lookup_dcache(name, base, flags);

	if (dentry)
		return dentry;

	dentry = d_alloc(base, name);
	if (unlikely(!dentry))
		return ERR_PTR(-ENOMEM);

	return lookup_real(base->d_inode, dentry, flags);
}

static void _putname(struct filename *name)
{
	BUG_ON(name->refcnt <= 0);

	if (--name->refcnt > 0)
		return;

	if (name->name != name->iname) {
		__putname(name->name);
		kfree(name);
	} else
		__putname(name);
}


extern struct filename *
kernel_path_parent(int dfd, const char __user *path,
		 struct path *parent,
		 struct qstr *last,
		 int *type,
		 unsigned int flags);

static long do_unlinkat(int dfd, const char *pathname)
{
	int error;
	struct filename *name;
	struct dentry *dentry;
	struct path path;
	struct qstr last;
	int type;
	struct inode *inode = NULL;
	struct inode *delegated_inode = NULL;
	unsigned int lookup_flags = 0;
retry:
	name = kernel_path_parent(dfd, pathname,
				&path, &last, &type, lookup_flags);
	if (IS_ERR(name)) {
		errno = -PTR_ERR(name);
		return PTR_ERR(name);
	}

	error = -EISDIR;
	if (type != LAST_NORM)
		goto exit1;

	error = mnt_want_write(path.mnt);
	if (error)
		goto exit1;
retry_deleg:
	inode_lock_nested(path.dentry->d_inode, I_MUTEX_PARENT);
	dentry = __lookup_hash(&last, path.dentry, lookup_flags);
	error = PTR_ERR(dentry);
	if (!IS_ERR(dentry)) {
		/* Why not before? Because we want correct error value */
		if (last.name[last.len])
			goto slashes;
		inode = dentry->d_inode;
		if (d_is_negative(dentry))
			goto slashes;
		ihold(inode);
		error = 0; //security_path_unlink(&path, dentry);
		if (error)
			goto exit2;
		error = vfs_unlink(path.dentry->d_inode, dentry, &delegated_inode);
exit2:
		dput(dentry);
	}
	inode_unlock(path.dentry->d_inode);
	if (inode)
		iput(inode);	/* truncate the inode here */
	inode = NULL;
	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error)
			goto retry_deleg;
	}
	mnt_drop_write(path.mnt);
exit1:
	path_put(&path);
	_putname(name);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		inode = NULL;
		goto retry;
	}
	errno = -error;
	return error;

slashes:
	if (d_is_negative(dentry))
		error = -ENOENT;
	else if (d_is_dir(dentry))
		error = -EISDIR;
	else
		error = -ENOTDIR;
	goto exit2;
}

int unlink(const char *pathname)
{
	int ret = do_unlinkat(AT_FDCWD, pathname);
	if (ret < 0)
		errno = -ret;
	return ret;
}

/* sys_rmdir */
int rmdir(const char *pathname)
{
	int error = 0;
	struct filename *name;
	struct dentry *dentry;
	struct path path;
	struct qstr last;
	int type;
	unsigned int lookup_flags = 0;
retry:
	name = kernel_path_parent(AT_FDCWD, pathname,
				&path, &last, &type, lookup_flags);
	if (IS_ERR(name)) {
		errno = -PTR_ERR(name);
		return PTR_ERR(name);
	}

	switch (type) {
	case LAST_DOTDOT:
		error = -ENOTEMPTY;
		goto exit1;
	case LAST_DOT:
		error = -EINVAL;
		goto exit1;
	case LAST_ROOT:
		error = -EBUSY;
		goto exit1;
	}

	error = mnt_want_write(path.mnt);
	if (error)
		goto exit1;

	inode_lock_nested(path.dentry->d_inode, I_MUTEX_PARENT);
	dentry = __lookup_hash(&last, path.dentry, lookup_flags);
	error = PTR_ERR(dentry);
	if (IS_ERR(dentry))
		goto exit2;
	if (!dentry->d_inode) {
		error = -ENOENT;
		goto exit3;
	}
	error = 0;//security_path_rmdir(&path, dentry);
	if (error)
		goto exit3;
	error = vfs_rmdir(path.dentry->d_inode, dentry);
exit3:
	dput(dentry);
exit2:
	inode_unlock(path.dentry->d_inode);
	mnt_drop_write(path.mnt);
exit1:
	path_put(&path);
	_putname(name);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	errno = -error;
	return error;
}

/* sys_select */
int select(int n, fd_set *inp, fd_set *outp, fd_set *exp, struct timeval *tv)
{
	BUG_ON(n || inp || outp || exp);

	if (tv) {
		unsigned long secs = tv->tv_sec + (tv->tv_usec / USEC_PER_SEC);
		schedule_timeout(HZ * secs);
	}
	return 0;
}


/* sys_fchmod */

static int chmod_common(const struct path *path, umode_t mode)
{   
	struct inode *inode = path->dentry->d_inode;
	struct inode *delegated_inode = NULL;
	struct iattr newattrs;
	int error;

	error = mnt_want_write(path->mnt);
	if (error) {
		errno = -error;
		return error;
	}
retry_deleg:
	inode_lock(inode);
/*	error = security_path_chmod(path, mode);
	if (error)
		goto out_unlock;
*/
	newattrs.ia_mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
	newattrs.ia_valid = ATTR_MODE | ATTR_CTIME;
	error = notify_change(path->dentry, &newattrs, &delegated_inode);

	inode_unlock(inode);
	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error)
			goto retry_deleg;
	}
	mnt_drop_write(path->mnt);
	errno = -error;
	return error;
}   

static struct file *ftable[1024];
static unsigned long next_free = 3, free_head;

static DEFINE_SPINLOCK(ftable_lock);

static int __install_file(struct file *fp)
{
	int fd = 0;

	spin_lock(&ftable_lock);
	if (next_free == 1024) {
		if (free_head) {
			fd = free_head;
			free_head = (unsigned long) ftable[free_head];
		} 
	} else {
		fd = next_free++;
	}
	spin_unlock(&ftable_lock);

	if (fd > 0) {
		ftable[fd] = fp;
		return fd;
	}
	return -1;
}

static struct file *__get_file(int fd)
{
	struct file *fp;

	fp = ftable[fd];
	if ((unsigned long) fp < 1024)
	   return NULL;
	return fp;	
}

static struct file *__uninstall_file(int fd)
{
	struct file *fp;
	spin_lock(&ftable_lock);
	fp = __get_file(fd);
	if (fp) {
		ftable[fd] = (void *) free_head;
		free_head = fd;
	}
	spin_unlock(&ftable_lock);
	return fp;
}

static struct file *__fget(unsigned int fd, fmode_t mask)
{
	struct file *file;

	rcu_read_lock();
loop:
	file = __get_file(fd);
	if (file) {
		/* File object ref couldn't be taken.
		 * dup2() atomicity guarantee is the reason
		 * we loop to catch the new file (or NULL pointer)
		 */
		if (file->f_mode & mask)
			file = NULL;
		else if (!get_file_rcu(file))
			goto loop;
	}
	rcu_read_unlock();

	return file;
}

static unsigned long __fget_light(unsigned int fd, fmode_t mask)
{
	struct files_struct *files = current->files;
	struct file *file;

	if (atomic_read(&files->count) == 1) {
		file = __get_file(fd);
		if (!file || unlikely(file->f_mode & mask))
			return 0;
		return (unsigned long)file;
	} else {
		file = __fget(fd, mask);
		if (!file)
			return 0;
		return FDPUT_FPUT | (unsigned long)file;
	}
}

static unsigned long ___fdget_raw(unsigned int fd)
{
	return __fget_light(fd, 0);
}

static inline struct fd _fdget_raw(unsigned int fd)
{
	return __to_fd(___fdget_raw(fd));
}

static unsigned long ___fdget(unsigned int fd)
{
	return __fget_light(fd, FMODE_PATH);
}

static inline struct fd _fdget(unsigned int fd)
{
	return __to_fd(___fdget(fd));
}

int fchmod(int fd, mode_t mode)
{
	struct fd f = _fdget(fd);
	int err = -EBADF;

	if (f.file) {
		// audit_file(f.file);
		err = chmod_common(&f.file->f_path, mode);
		fdput(f);
	}
	errno = -err;
	return err;
}

static int fchmodat(int dfd, const char *filename, umode_t mode)
{
	struct path path;
	int error; 
	unsigned int lookup_flags = LOOKUP_FOLLOW;
retry:
	error = kern_path(filename, lookup_flags, &path);
	if (!error) {
		error = chmod_common(&path, mode);
		path_put(&path);
		if (retry_estale(error, lookup_flags)) {
			lookup_flags |= LOOKUP_REVAL;
			goto retry;
		}
	}
	errno = -error;
	return error;
}

int chmod(const char *filename, mode_t mode)
{
	return fchmodat(AT_FDCWD, filename, mode);
}

/* sys_rename */

static int renameat2(int olddfd, const char *oldname,
		int newdfd, const char *newname, unsigned int flags)
{
	struct dentry *old_dentry, *new_dentry;
	struct dentry *trap;
	struct path old_path, new_path;
	struct qstr old_last, new_last;
	int old_type, new_type;
	struct inode *delegated_inode = NULL;
	struct filename *from;
	struct filename *to;
	unsigned int lookup_flags = 0, target_flags = LOOKUP_RENAME_TARGET;
	bool should_retry = false;
	int error;

	if (flags & ~(RENAME_NOREPLACE | RENAME_EXCHANGE | RENAME_WHITEOUT)) 
		return -EINVAL;

	if ((flags & (RENAME_NOREPLACE | RENAME_WHITEOUT)) &&
	    (flags & RENAME_EXCHANGE))
		return -EINVAL;

	if ((flags & RENAME_WHITEOUT) && !capable(CAP_MKNOD))
		return -EPERM;

	if (flags & RENAME_EXCHANGE)
		target_flags = 0;

retry:
	from = kernel_path_parent(olddfd, oldname,
				&old_path, &old_last, &old_type, lookup_flags);
	if (IS_ERR(from)) {
		error = PTR_ERR(from);
		goto exit;
	}

	to = kernel_path_parent(newdfd, newname,
				&new_path, &new_last, &new_type, lookup_flags);
	if (IS_ERR(to)) {
		error = PTR_ERR(to);
		goto exit1;
	}

	error = -EXDEV;
	if (old_path.mnt != new_path.mnt)
		goto exit2;

	error = -EBUSY;
	if (old_type != LAST_NORM)
		goto exit2;

	if (flags & RENAME_NOREPLACE)
		error = -EEXIST;
	if (new_type != LAST_NORM)
		goto exit2;

	error = mnt_want_write(old_path.mnt);
	if (error)
		goto exit2;

retry_deleg:
	trap = lock_rename(new_path.dentry, old_path.dentry);

	old_dentry = __lookup_hash(&old_last, old_path.dentry, lookup_flags);
	error = PTR_ERR(old_dentry);
	if (IS_ERR(old_dentry))
		goto exit3;
	/* source must exist */
	error = -ENOENT;
	if (d_is_negative(old_dentry))
		goto exit4;
	new_dentry = __lookup_hash(&new_last, new_path.dentry, lookup_flags | target_flags);
	error = PTR_ERR(new_dentry);
	if (IS_ERR(new_dentry))
		goto exit4;
	error = -EEXIST;
	if ((flags & RENAME_NOREPLACE) && d_is_positive(new_dentry))
		goto exit5;
	if (flags & RENAME_EXCHANGE) {
		error = -ENOENT;
		if (d_is_negative(new_dentry))
			goto exit5;

		if (!d_is_dir(new_dentry)) {
			error = -ENOTDIR;
			if (new_last.name[new_last.len])
				goto exit5;
		}
	}
	/* unless the source is a directory trailing slashes give -ENOTDIR */
	if (!d_is_dir(old_dentry)) {
		error = -ENOTDIR;
		if (old_last.name[old_last.len])
			goto exit5;
		if (!(flags & RENAME_EXCHANGE) && new_last.name[new_last.len])
			goto exit5;
	}
	/* source should not be ancestor of target */
	error = -EINVAL;
	if (old_dentry == trap)
		goto exit5;
	/* target should not be an ancestor of source */
	if (!(flags & RENAME_EXCHANGE))
		error = -ENOTEMPTY;
	if (new_dentry == trap)
		goto exit5;

	error = 0;//security_path_rename(&old_path, old_dentry,
				//     &new_path, new_dentry, flags);
	if (error)
		goto exit5;
	error = vfs_rename(old_path.dentry->d_inode, old_dentry,
			   new_path.dentry->d_inode, new_dentry,
			   &delegated_inode, flags);
exit5:
	dput(new_dentry);
exit4:
	dput(old_dentry);
exit3:
	unlock_rename(new_path.dentry, old_path.dentry);
	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error)
			goto retry_deleg;
	}
	mnt_drop_write(old_path.mnt);
exit2:
	if (retry_estale(error, lookup_flags))
		should_retry = true;
	path_put(&new_path);
	_putname(to);
exit1:
	path_put(&old_path);
	_putname(from);
	if (should_retry) {
		should_retry = false;
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
exit:
	return error;
}

int rename(const char *oldname, const char *newname)
{
	int ret = renameat2(AT_FDCWD, oldname, AT_FDCWD, newname, 0);
	errno = -ret;
	return ret;
}

/* sys_fcntl */

static DEFINE_SPINLOCK(blocked_lock_lock);

static int assign_type(struct file_lock *fl, long type)
{
	switch (type) {
	case F_RDLCK:
	case F_WRLCK:
	case F_UNLCK:
		fl->fl_type = type;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int flock64_to_posix_lock(struct file *filp, struct file_lock *fl,
		struct flock64 *l)
{
	switch (l->l_whence) {
		case SEEK_SET:
			fl->fl_start = 0;
			break;
		case SEEK_CUR:
			fl->fl_start = filp->f_pos;
			break;
		case SEEK_END:
			fl->fl_start = i_size_read(file_inode(filp));
			break;
		default:
			return -EINVAL;
	}
	if (l->l_start > OFFSET_MAX - fl->fl_start)
		return -EOVERFLOW;
	fl->fl_start += l->l_start;
	if (fl->fl_start < 0)
		return -EINVAL;

	/* POSIX-1996 leaves the case l->l_len < 0 undefined;
	 *        POSIX-2001 defines it. */
	if (l->l_len > 0) {
		if (l->l_len - 1 > OFFSET_MAX - fl->fl_start)
			return -EOVERFLOW;
		fl->fl_end = fl->fl_start + l->l_len - 1;

	} else if (l->l_len < 0) {
		if (fl->fl_start + l->l_len < 0)
			return -EINVAL;
		fl->fl_end = fl->fl_start - 1;
		fl->fl_start += l->l_len;
	} else
		fl->fl_end = OFFSET_MAX;

	fl->fl_owner = current->files;
	fl->fl_pid = current->tgid;
	fl->fl_file = filp;
	fl->fl_flags = FL_POSIX;
	fl->fl_ops = NULL;
	fl->fl_lmops = NULL;

	return assign_type(fl, l->l_type);
}

static int flock_to_posix_lock(struct file *filp, struct file_lock *fl,
		struct flock *l)
{
	struct flock64 ll = {
		.l_type = l->l_type,
		.l_whence = l->l_whence,
		.l_start = l->l_start,
		.l_len = l->l_len,
	};

	return flock64_to_posix_lock(filp, fl, &ll);
}

	static int
check_fmode_for_setlk(struct file_lock *fl)
{
	switch (fl->fl_type) {
		case F_RDLCK:
			if (!(fl->fl_file->f_mode & FMODE_READ))
				return -EBADF;
			break;
		case F_WRLCK:
			if (!(fl->fl_file->f_mode & FMODE_WRITE))
				return -EBADF;
	}
	return 0;
}

static void locks_delete_global_blocked(struct file_lock *waiter)
{
	lockdep_assert_held(&blocked_lock_lock);

	hash_del(&waiter->fl_link);
}

/* Remove waiter from blocker's block list.
 *  * When blocker ends up pointing to itself then the list is empty.
 *   *
 *    * Must be called with blocked_lock_lock held.
 *     */
static void __locks_delete_block(struct file_lock *waiter)
{
	locks_delete_global_blocked(waiter);
	list_del_init(&waiter->fl_block);
	waiter->fl_next = NULL;
}

static void locks_delete_block(struct file_lock *waiter)
{
	spin_lock(&blocked_lock_lock);
	__locks_delete_block(waiter);
	spin_unlock(&blocked_lock_lock);
}

static int do_lock_file_wait(struct file *filp, unsigned int cmd,
		struct file_lock *fl)
{
	int error;

	error = 0; //security_file_lock(filp, fl->fl_type);
	if (error)
		return error;

	for (;;) {
		error = vfs_lock_file(filp, cmd, fl, NULL);
		if (error != FILE_LOCK_DEFERRED)
			break;
		error = wait_event_interruptible(fl->fl_wait, !fl->fl_next);
		if (!error)
			continue;

		locks_delete_block(fl);
		break;
	}

	return error;
}


static int __fcntl_setlk(unsigned int fd, struct file *filp, unsigned int cmd,
		struct flock *flock)
{
	struct file_lock *file_lock = locks_alloc_lock();
	struct inode *inode;
	struct file *f;
	int error;

	if (file_lock == NULL)
		return -ENOLCK;

	inode = locks_inode(filp);

	/* Don't allow mandatory locks on files that may be memory mapped
	 *      * and shared.
	 *           */
	if (mandatory_lock(inode) && mapping_writably_mapped(filp->f_mapping)) {
		error = -EAGAIN;
		goto out;
	}

	error = flock_to_posix_lock(filp, file_lock, flock);
	if (error)
		goto out;

	error = check_fmode_for_setlk(file_lock);
	if (error)
		goto out;

	/*
	 *      * If the cmd is requesting file-private locks, then set the
	 *           * FL_OFDLCK flag and override the owner.
	 *                */
	switch (cmd) {
		case F_OFD_SETLK:
			error = -EINVAL;
			if (flock->l_pid != 0)
				goto out;

			cmd = F_SETLK;
			file_lock->fl_flags |= FL_OFDLCK;
			file_lock->fl_owner = filp;
			break;
		case F_OFD_SETLKW:
			error = -EINVAL;
			if (flock->l_pid != 0)
				goto out;

			cmd = F_SETLKW;
			file_lock->fl_flags |= FL_OFDLCK;
			file_lock->fl_owner = filp;
			/* Fallthrough */
		case F_SETLKW:
			file_lock->fl_flags |= FL_SLEEP;
	}

	error = do_lock_file_wait(filp, cmd, file_lock);

	if (!error && file_lock->fl_type != F_UNLCK &&
			!(file_lock->fl_flags & FL_OFDLCK)) {

		f = ftable[fd];
		if (f != filp) {
			file_lock->fl_type = F_UNLCK;
			error = do_lock_file_wait(filp, cmd, file_lock);
			WARN_ON_ONCE(error);
			error = -EBADF;
		}
	}
out:
	locks_free_lock(file_lock);
	return error;
}

static long do_fcntl(int fd, unsigned int cmd, unsigned long arg,
		struct file *filp)
{
	long err = -EINVAL;

	switch (cmd) {
		case F_OFD_SETLK:
		case F_OFD_SETLKW:
		case F_SETLK:
		case F_SETLKW:
			err = __fcntl_setlk(fd, filp, cmd, (struct flock __user *) arg);
			break;
		default:
			break;
	}
	return err;
}


static int check_fcntl_cmd(unsigned cmd)
{
	switch (cmd) {
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
		case F_GETFD:
		case F_SETFD:
		case F_GETFL:
			return 1;
	}
	return 0;
}

int fcntl(int fd, int cmd, struct flock *arg)
{   
	struct fd f = _fdget_raw(fd);
	long err = -EBADF;

	if (!f.file)
		goto out;

	if (unlikely(f.file->f_mode & FMODE_PATH)) {
		if (!check_fcntl_cmd(cmd))
			goto out1;
	}

	err = 0; // security_file_fcntl(f.file, cmd, arg);
	if (!err)
		err = do_fcntl(fd, cmd, (unsigned long) arg, f.file);

out1:
	fdput(f);
out:
	errno = -err;
	return err;
}

/* sys_close */

int close(int fd)
{
	struct file *file;
	int ret;

	file = __uninstall_file(fd);
	if (!file)
		goto out_unlock;

	ret = filp_close(file, NULL);
	if (ret < 0) 
		errno = -ret;
	return ret;

out_unlock:
	errno = EBADF;
	return -EBADF;
}

/* sys_fsync */
int fsync(int fd)
{
	int ret;
	struct file *f;

	f = __fget(fd, FMODE_PATH);
	if(!f) {
		errno = EINVAL;
		return -1;
	}

	ret = vfs_fsync(f, 0);
	fput(f);
	errno = -ret;
	return ret;
}

/* sys_fdatasync */

int fdatasync(int fd)
{
	int ret;
	struct file *f;

	f = __fget(fd, FMODE_PATH);
	if (!f) {
		errno = EINVAL;
		return -1;
	}

	ret = vfs_fsync(f, 1);
	fput(f);
	errno = -ret;
	return ret;
}

/* sys_read */

static void _f_unlock_pos(struct file *f)
{
	mutex_unlock(&f->f_pos_lock);
}

static inline void _fdput_pos(struct fd f)
{
	if (f.flags & FDPUT_POS_UNLOCK)
		_f_unlock_pos(f.file);
	fdput(f);
}

static inline loff_t file_pos_read(struct file *file)
{
	return file->f_pos;
}

static inline void file_pos_write(struct file *file, loff_t pos)
{
	file->f_pos = pos;
}

static unsigned long ___fdget_pos(unsigned int fd)
{
	unsigned long v = __fget_light(fd, FMODE_PATH);
	struct file *file = (struct file *)(v & ~3);

	if (file && (file->f_mode & FMODE_ATOMIC_POS)) {
		if (file_count(file) > 1) {
			v |= FDPUT_POS_UNLOCK;
			mutex_lock(&file->f_pos_lock);
		}
	}
	return v;
}

static inline struct fd _fdget_pos(int fd)
{
	return __to_fd(___fdget_pos(fd));
}

ssize_t read(int fd, const void *buf, size_t count)
{
	mm_segment_t old_fs;
	struct fd f = _fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos = file_pos_read(f.file);

		old_fs = get_fs();
		set_fs(get_ds());
		ret = vfs_read(f.file, (void __user *)buf, count, &pos);
		set_fs(old_fs);

		if (ret >= 0)
			file_pos_write(f.file, pos);
		_fdput_pos(f);
	}
	if (ret < 0)
		errno = -ret;
	return ret;
}

/* sys_write */

ssize_t write(int fd, const void *buf, size_t count)
{
	mm_segment_t old_fs;
	struct fd f = _fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		loff_t pos = file_pos_read(f.file);

		old_fs = get_fs();    
		set_fs(get_ds()); 
		ret = vfs_write(f.file, (void __user *) buf, count, &pos);
		set_fs(old_fs);

		if (ret >= 0)
			file_pos_write(f.file, pos);
		_fdput_pos(f);
	}

	if (ret < 0)
		errno = -ret;
	return ret;
}

/* sys_pread */
ssize_t pread(int fd, void* buf, size_t count, loff_t pos)
{
	mm_segment_t old_fs;
	struct fd f;
	ssize_t ret = -EBADF;

	if (pos < 0) {
		errno = EINVAL;
		return -EINVAL;
	}

	f = _fdget(fd);
	if (f.file) {
		ret = -ESPIPE;
		if (f.file->f_mode & FMODE_PREAD) {
			old_fs = get_fs();    
			set_fs(get_ds()); 
			ret = vfs_read(f.file, (void __user *) buf, count, &pos);
			set_fs(old_fs);
		}
		fdput(f);
	}

	if (ret < 0)
		errno = -ret;
	return ret;
}

/* sys_pread */
ssize_t pwrite(int fd, const void *buf, size_t count, loff_t pos)
{
	mm_segment_t old_fs;
	struct fd f;
	ssize_t ret = -EBADF;

	if (pos < 0) {
		errno = EINVAL;
		return -EINVAL;
	}

	f = _fdget(fd);
	if (f.file) {
		ret = -ESPIPE;
		if (f.file->f_mode & FMODE_PWRITE) {
			old_fs = get_fs();    
			set_fs(get_ds()); 
			ret = vfs_write(f.file, (void __user *) buf, count, &pos);
			set_fs(old_fs);
		}
		fdput(f);
	}
	if (ret < 0)
		errno = -ret;
	return ret;
}

/* sys_opendir */

struct DIR_s {
	int fd;
	struct dirent dirent;
};

DIR *opendir(const char *name)
{
	DIR *d = kmalloc(sizeof(DIR), GFP_KERNEL);
	
	if (!d) {
		errno = ENOMEM;
		return NULL;
	}	

	d->fd = open(name, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC, 0666);
	if (d->fd <= 0) {
		errno = -d->fd;
		kfree(d);
		return NULL;
	}
	return d;
}

int closedir(DIR *d)
{
	close(d->fd);
	kfree(d);
	return 0;
}

struct getdents_callback {
	struct dir_context ctx;
	struct dirent *dirent;
	int count;
};

static int fillone(struct dir_context *ctx, const char *name, int namlen,
		loff_t offset, u64 ino, unsigned int d_type)
{
	struct dirent *dirent;
	struct getdents_callback *buf =
		container_of(ctx, struct getdents_callback, ctx);
	int reclen = ALIGN(offsetof(struct dirent, d_name) + namlen + 1,
			sizeof(u64));

	if (buf->count)
		return -EINVAL;

	dirent = buf->dirent;
	dirent->d_ino = ino;
	dirent->d_off = 0;
	dirent->d_reclen = reclen;
	dirent->d_type = d_type;
	strncpy(dirent->d_name, name, namlen);
	dirent->d_name[namlen] = 0;
	buf->count++;
	return 0;
}   

/* sys_readdir */
struct dirent *readdir(DIR *d)
{
	int ret;
	struct fd f;
	struct getdents_callback buf = {
		.ctx.actor = fillone,
		.dirent = &d->dirent
	};

	f = _fdget_pos(d->fd);
	if (!f.file)
		return NULL;

	ret = iterate_dir(f.file, &buf.ctx);
	if (ret < 0)
		errno = -ret;

	_fdput_pos(f);

	if (buf.count) { 
		d->dirent.d_off = buf.ctx.pos;
		return &d->dirent;
	}	
	return NULL;
}

/* sys_open */
int open(const char *pathname, int flags, mode_t mode)
{
	int fd = get_unused_fd_flags(flags);

	struct file *f = filp_open(pathname, flags, mode);
	if (IS_ERR(f)) {
		fd = PTR_ERR(f);
	} else {
		fd = __install_file(f);
		if (fd == -1)  {
			fd = -EMFILE;
			filp_close(f, 0);
		} else {
			fsnotify_open(f);
		}
	}
	if (fd < 0)
		errno = -fd;
	return fd;
}

/* sys_fstat */

int fstat(int fd, struct stat *st)
{
	struct kstat ks;
	struct fd f = _fdget_raw(fd);
	int error = -EBADF;

	if (f.file) {
		error = vfs_getattr(&f.file->f_path, &ks);
		fdput(f);
	}

	if (error == 0) {
		st->st_dev = ks.dev;
		st->st_ino = ks.ino;
		st->st_mode = ks.mode;
		st->st_nlink = ks.nlink;
		st->st_uid = ks.uid.val;
		st->st_gid = ks.gid.val;
		st->st_rdev = ks.rdev;
		st->st_size = ks.size;
		st->st_blksize = ks.blksize;
		st->st_blocks = ks.blocks;
		st->st_atime = ks.atime.tv_sec;
		st->st_mtime = ks.mtime.tv_sec;
		st->st_ctime = ks.ctime.tv_sec;
		st->st_atime_nsec = ks.atime.tv_nsec;
		st->st_mtime_nsec = ks.mtime.tv_nsec;
		st->st_ctime_nsec = ks.ctime.tv_nsec;
	} else {
		errno = -error;
	}
	return error;
}

/* sys_lseek */
off_t lseek(int fd, off_t offset, int whence)
{
	off_t retval;
	struct fd f = _fdget_pos(fd);
	if (!f.file) {
		errno = EBADF;
		return -EBADF;
	}

	retval = -EINVAL;
	if (whence <= SEEK_MAX) {
		loff_t res = vfs_llseek(f.file, offset, whence);
		retval = res;
		if (res != (loff_t)retval)
			retval = -EOVERFLOW;	
	}
	_fdput_pos(f);
	if (retval < 0)
		errno = -retval;
	return retval;
}

/* sys_gettid */

pid_t gettid(void)
{
	return task_pid_vnr(current); 
}

/* sys_mkdir */

int mkdir(const char *pathname, mode_t mode)
{
	struct dentry *dentry;
	struct path path;
	int error;
	unsigned int lookup_flags = LOOKUP_DIRECTORY;

retry:
	dentry = kern_path_create(AT_FDCWD, pathname, &path, lookup_flags);
	if (IS_ERR(dentry)) {
		errno = -PTR_ERR(dentry);
		return PTR_ERR(dentry);
	}

	if (!IS_POSIXACL(path.dentry->d_inode))
		mode &= ~current_umask();
	error = 0; //security_path_mkdir(&path, dentry, mode);
	if (!error)
		error = vfs_mkdir(path.dentry->d_inode, dentry, mode);
	done_path_create(&path, dentry);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	errno = -error;
	return error;
}

/* sys_getuid */
uid_t getuid(void)
{
	return from_kuid_munged(current_user_ns(), current_uid());
}	   

#include <db.h>
EXPORT_SYMBOL(db_strerror);
EXPORT_SYMBOL(db_env_create);
EXPORT_SYMBOL(db_create);
