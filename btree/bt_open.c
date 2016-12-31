#ifdef __KERNEL__
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#else
#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db.h>
#include "btree.h"


static int nroot (BTREE *);

/*
 * __BT_OPEN -- Open a btree.
 *
 * Creates and fills a DB struct, and calls the routine that actually
 * opens the btree.
 *
 * Parameters:
 *	fname:	filename (NULL for in-memory trees)
 *	flags:	open flag bits
 *	mode:	open permission bits
 *	b:	BTREEINFO pointer
 *
 * Returns:
 *	NULL on failure, pointer to DB on success.
 *
 */
DB *
__bt_open(fname, flags, mode, openinfo, dflags)
	const char *fname;
	int flags, mode, dflags;
	const BTREEINFO *openinfo;
{
#ifdef __KERNEL__
	struct kstat sb;
#else
	struct stat sb;
#endif
	BTMETA m;
	BTREE *t;
	BTREEINFO b;
	DB *dbp;
	pgno_t ncache;
	ssize_t nr;
	int ret;
#ifdef __KERNEL__
	mm_segment_t old_fs = get_fs();
#endif

	t = NULL;

	/*
	 * Intention is to make sure all of the user's selections are okay
	 * here and then use them without checking.  Can't be complete, since
	 * we don't know the right page size, lorder or flags until the backing
	 * file is opened.  Also, the file's page size can cause the cachesize
	 * to change.
	 */
	if (openinfo) {
		b = *openinfo;

		/* Flags: R_DUP. */
		if (b.flags & ~(R_DUP))
			goto einval;

		/*
		 * Page size must be indx_t aligned and >= MINPSIZE.  Default
		 * page size is set farther on, based on the underlying file
		 * transfer size.
		 */
		if (b.psize &&
		    (b.psize < MINPSIZE || b.psize > MAX_PAGE_OFFSET + 1 ||
		    b.psize & (sizeof(indx_t) - 1)))
			goto einval;

		/* Minimum number of keys per page; absolute minimum is 2. */
		if (b.minkeypage) {
			if (b.minkeypage < 2)
				goto einval;
		} else
			b.minkeypage = DEFMINKEYPAGE;

		/* If no comparison, use default comparison and prefix. */
		if (b.compare == NULL) {
			b.compare = __bt_defcmp;
			if (b.prefix == NULL)
				b.prefix = __bt_defpfx;
		}

	} else {
		b.compare = __bt_defcmp;
		b.cachesize = 0;
		b.flags = 0;
		b.minkeypage = DEFMINKEYPAGE;
		b.prefix = __bt_defpfx;
		b.psize = 0;
	}

	/* Allocate and initialize DB and BTREE structures. */
	if ((t = (BTREE *)kmalloc(sizeof(BTREE), GFP_KERNEL)) == NULL)
		goto err;
	memset(t, 0, sizeof(BTREE));
	mutex_init(&t->mutex);
#ifdef __KERNEL__
	t->bt_file = NULL;			/* Don't close unopened fd on error. */
#else
	t->bt_file = 0;			/* Don't close unopened fd on error. */
#endif

	t->bt_cmp = b.compare;
	t->bt_pfx = b.prefix;

	if ((t->bt_dbp = dbp = (DB *)kmalloc(sizeof(DB), GFP_KERNEL)) == NULL)
		goto err;
	memset(t->bt_dbp, 0, sizeof(DB));

	dbp->type = DB_BTREE;
	dbp->internal = t;
	dbp->close = __bt_close;
	dbp->del = __bt_delete;
	dbp->get = __bt_get;
	dbp->put = __bt_put;
	dbp->sync = __bt_sync;

	/*
	 * If no file name was supplied, this is an in-memory btree and we
	 * open a backing temporary file.  Otherwise, it's a disk-based tree.
	 */
	if (fname) {
		switch (flags & O_ACCMODE) {
		case O_RDONLY:
			F_SET(t, B_RDONLY);
			break;
		case O_RDWR:
			break;
		case O_WRONLY:
		default:
			goto einval;
		}
		
#ifdef __KERNEL__
		if (IS_ERR((t->bt_file = filp_open(fname, flags, mode)))) {
			printk(KERN_ERR "GBFS: open failed (%s)\n", fname);
			goto err;
		}
#else
		if((t->bt_file = open(fname, flags, mode)) <= 0) {
			fprintf(stderr, "GBFS: open failed (%s)\n", fname);
			goto err;
		}
#endif
	}

#ifdef __KERNEL__
	set_fs(KERNEL_DS);
	if ((ret = vfs_stat(fname, &sb))) {
		set_fs(old_fs);
		printk(KERN_ERR "GBFS: stat failed (%s) errno=%d\n", fname, ret);
		goto err;
	}
	set_fs(old_fs);
#else
	if (ret = stat(fname, &sb)) {
		fprintf(stderr, "GBFS: stat failed (%s) errno=%d\n", fname, ret);
		goto err;
	}
#endif

#ifdef __KERNEL__
	if (sb.size) {
#else
	if (sb.st_size > 40 * 4 * 1024) {
#endif
		if ((nr = kernel_read(t->bt_file, P_META * 4096, (char *) &m, sizeof(BTMETA))) < 0)
			goto err;
		if (nr != sizeof(BTMETA))
			goto eftype;

		if (m.magic != BTREEMAGIC || m.version != BTREEVERSION)
			goto eftype;
		if (m.psize < MINPSIZE || m.psize > MAX_PAGE_OFFSET + 1 ||
		    m.psize & (sizeof(indx_t) - 1))
			goto eftype;
		b.psize = m.psize;
	} else {
		/*
		 * Set the page size to the best value for I/O to this file.
		 * Don't overflow the page offset type.
		 */
		if (b.psize == 0) {
#ifdef __KERNEL__
			b.psize = sb.blksize;
#else
			b.psize = sb.st_blksize;
#endif
			if (b.psize < MINPSIZE)
				b.psize = MINPSIZE;
			if (b.psize > MAX_PAGE_OFFSET + 1)
				b.psize = MAX_PAGE_OFFSET + 1;
		}

		F_SET(t, B_METADIRTY);
	}

	t->bt_psize = b.psize;

	/* Set the cache size; must be a multiple of the page size. */
	if (b.cachesize && b.cachesize & (b.psize - 1))
		b.cachesize += (~b.cachesize & (b.psize - 1)) + 1;
	if (b.cachesize < b.psize * MINCACHE)
		b.cachesize = b.psize * MINCACHE;

	/* Calculate number of pages to cache. */
	ncache = (b.cachesize + t->bt_psize - 1) / t->bt_psize;

	/*
	 * The btree data structure requires that at least two keys can fit on
	 * a page, but other than that there's no fixed requirement.  The user
	 * specified a minimum number per page, and we translated that into the
	 * number of bytes a key/data pair can use before being placed on an
	 * overflow page.  This calculation includes the page header, the size
	 * of the index referencing the leaf item and the size of the leaf item
	 * structure.  Also, don't let the user specify a minkeypage such that
	 * a key/data pair won't fit even if both key and data are on overflow
	 * pages.
	 */
	t->bt_ovflsize = (t->bt_psize - BTDATAOFF) / b.minkeypage -
	    (sizeof(indx_t) + NBLEAFDBT(0, 0));
	if (t->bt_ovflsize < NBLEAFDBT(NOVFLSIZE, NOVFLSIZE) + sizeof(indx_t))
		t->bt_ovflsize =
		    NBLEAFDBT(NOVFLSIZE, NOVFLSIZE) + sizeof(indx_t);

	/* Initialize the buffer pool. */
	if ((t->bt_mp =
	    mpool_open(fname, t->bt_file, t->bt_psize, ncache)) == NULL) {
//		printk(KERN_ERR "GBFS: mpool open failed\n");
		printk("GBFS: mpool open failed\n");
		goto err;
	}

	/* Create a root page if new tree. */
	if (nroot(t) == RET_ERROR) {
		printk(KERN_ERR "GBFS: nroot failed\n");
		goto err;
	}
		
	printk(KERN_ERR "GBFS: Btree open successfull\n");

	return (dbp);

einval:	
//	errno = EINVAL; TODO
	goto err;

eftype:	
//	errno = EFTYPE; TODO
	goto err;

err:	if (t) {
		if (t->bt_dbp)
			kfree(t->bt_dbp);
#ifdef __KERNEL__
		if (t->bt_file && !IS_ERR(t->bt_file))
			filp_close(t->bt_file, NULL);
#else 
		if (t->bt_file >= 0)
			close(t->bt_file);
#endif
		kfree(t);
	}
	return (NULL);
}

/*
 * NROOT -- Create the root of a new tree.
 *
 * Parameters:
 *	t:	tree
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
static int
nroot(t)
	BTREE *t;
{
	PAGE *meta, *root;
	pgno_t npg;

	if ((meta = mpool_get_pg(t->bt_mp, P_META, 0)) != NULL) {
		mpool_put(t->bt_mp, meta, 0);
		return (RET_SUCCESS);
	}

	// TODO
//	if (errno != EINVAL)		/* It's OK to not exist. */
//		return (RET_ERROR);
//	errno = 0;

	if ((meta = mpool_new_pg(t->bt_mp, &npg)) == NULL) 
		return (RET_ERROR);

	if ((root = mpool_new_pg(t->bt_mp, &npg)) == NULL)
		return (RET_ERROR);

	if (npg != P_ROOT) {
		printk(KERN_ERR "GBFS: ROOT page no mismatch\n");
		return (RET_ERROR);
	}

	root->pgno = npg;
	root->prevpg = root->nextpg = P_INVALID;
	root->lower = BTDATAOFF;
	root->upper = t->bt_psize;
	root->flags = P_BLEAF;
	memset(meta, 0, t->bt_psize);
	mpool_put(t->bt_mp, meta, MPOOL_DIRTY);
	mpool_put(t->bt_mp, root, MPOOL_DIRTY);
	return (RET_SUCCESS);
}
