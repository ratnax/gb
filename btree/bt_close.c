#ifdef __KERNEL__
#include<linux/slab.h>
#include<linux/fs.h>
#else
#include <sys/param.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <list.h>
#endif

#include <db.h>
#include "btree.h"

static int bt_meta (BTREE *);

/*
 * BT_CLOSE -- Close a btree.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__bt_close(dbp)
	DB *dbp;
{
	BTREE *t;
#ifdef __KERNEL__ 
	struct file *file;
#else
	int file;
#endif

	t = dbp->internal;

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}

	/* Sync the tree. */
	if (__bt_sync(dbp, 0) == RET_ERROR)
		return (RET_ERROR);

	/* Close the memory pool. */
	if (mpool_close(t->bt_mp) == RET_ERROR)
		return (RET_ERROR);

	if (t->bt_rkey.data) {
		kfree(t->bt_rkey.data);
		t->bt_rkey.size = 0;
		t->bt_rkey.data = NULL;
	}
	if (t->bt_rdata.data) {
		kfree(t->bt_rdata.data);
		t->bt_rdata.size = 0;
		t->bt_rdata.data = NULL;
	}

	file = t->bt_file;
	kfree(t);
	kfree(dbp);
	return (filp_close(file, NULL) ? RET_ERROR : RET_SUCCESS);
}

/*
 * BT_SYNC -- sync the btree to disk.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
int
__bt_sync(dbp, flags)
	const DB *dbp;
	u_int flags;
{
	BTREE *t;
	int status;

	t = dbp->internal;

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}

	/* Sync doesn't currently take any flags. */
	if (flags != 0) {
		// errno = EINVAL; TODORATNA
		return (RET_ERROR);
	}

	if (!F_ISSET(t, B_MODIFIED))
		return (RET_SUCCESS);

	if (F_ISSET(t, B_METADIRTY) && bt_meta(t) == RET_ERROR)
		return (RET_ERROR);

	if ((status = mpool_sync(t->bt_mp)) == RET_SUCCESS)
		F_CLR(t, B_MODIFIED);

	return (status);
}

/*
 * BT_META -- write the tree meta data to disk.
 *
 * Parameters:
 *	t:	tree
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
static int
bt_meta(t)
	BTREE *t;
{
	BTMETA m;
	void *p;

	if ((p = mpool_get_pg(t->bt_mp, P_META, 0)) == NULL)
		return (RET_ERROR);

	/* Fill in metadata. */
	m.magic = BTREEMAGIC;
	m.version = BTREEVERSION;
	m.psize = t->bt_psize;

	memmove(p, &m, sizeof(BTMETA));
	mpool_put(t->bt_mp, p, MPOOL_DIRTY);
	return (RET_SUCCESS);
}
