#ifdef __KERNEL__
#include<asm/string.h>
#else
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#endif


#include <db.h>
#include "btree.h"

static int __bt_bdelete (BTREE *, const DBT *);
static int __bt_pdelete (BTREE *, PAGE *);

/*
 * __bt_delete
 *	Delete the item(s) referenced by a key.
 *
 * Return RET_SPECIAL if the key is not found.
 */
int
__bt_delete(dbp, key, flags)
	const DB *dbp;
	const DBT *key;
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

	/* Check for change to a read-only tree. */
	if (F_ISSET(t, B_RDONLY)) {
//		errno = EPERM; TODORATNA
		return (RET_ERROR);
	}

	if ((status = __bt_bdelete(t, key)) == RET_SUCCESS) 
		F_SET(t, B_MODIFIED);
	return (status);
}

/*
 * __bt_bdelete --
 *	Delete all key/data pairs matching the specified key.
 *
 * Parameters:
 *	  t:	tree
 *	key:	key to delete
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key not found.
 */
static int
__bt_bdelete(t, key)
	BTREE *t;
	const DBT *key;
{
	EPG *e;
	PAGE *h;
	int deleted, exact, redo;

	deleted = 0;

	/* Find any matching record; __bt_search pins the page. */
	if ((e = __bt_search(t, key, &exact)) == NULL)
		return (deleted ? RET_SUCCESS : RET_ERROR);

	if (!exact) {
		mpool_put(t->bt_mp, e->page, 0);
		return (deleted ? RET_SUCCESS : RET_SPECIAL);
	}

	/*
	 * Delete forward, then delete backward, from the found key.  If
	 * there are duplicates and we reach either side of the page, do
	 * the key search again, so that we get them all.
	 */
	redo = 0;
	h = e->page;

	if (__bt_dleaf(t, key, h, e->index)) {
		mpool_put(t->bt_mp, h, 0);
		return (RET_ERROR);
	}
	if (NEXTINDEX(h) == 0) {
		if (__bt_pdelete(t, h))
			return (RET_ERROR);
	} else
		mpool_put(t->bt_mp, h, MPOOL_DIRTY);
	return (RET_SUCCESS);
}

/*
 * __bt_pdelete --
 *	Delete a single page from the tree.
 *
 * Parameters:
 *	t:	tree
 *	h:	leaf page
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 *
 * Side-effects:
 *	mpool_put's the page
 */
static int
__bt_pdelete(t, h)
	BTREE *t;
	PAGE *h;
{
	BINTERNAL *bi;
	PAGE *pg;
	EPGNO *parent;
	indx_t cnt, index, *ip, offset;
	u_int32_t nksize;
	char *from;

	/*
	 * Walk the parent page stack -- a LIFO stack of the pages that were
	 * traversed when we searched for the page where the delete occurred.
	 * Each stack entry is a page number and a page index offset.  The
	 * offset is for the page traversed on the search.  We've just deleted
	 * a page, so we have to delete the key from the parent page.
	 *
	 * If the delete from the parent page makes it empty, this process may
	 * continue all the way up the tree.  We stop if we reach the root page
	 * (which is never deleted, it's just not worth the effort) or if the
	 * delete does not empty the page.
	 */
	while ((parent = BT_POP(t)) != NULL) {
		/* Get the parent page. */
		if ((pg = mpool_get_pg(t->bt_mp, parent->pgno, 0)) == NULL)
			return (RET_ERROR);
		
		index = parent->index;
		bi = GETBINTERNAL(pg, index);

		/*
		 * Free the parent if it has only the one key and it's not the
		 * root page. If it's the rootpage, turn it back into an empty
		 * leaf page.
		 */
		if (NEXTINDEX(pg) == 1)
			if (pg->pgno == P_ROOT) {
				pg->lower = BTDATAOFF;
				pg->upper = t->bt_psize;
				pg->flags = P_BLEAF;
			} else {
				if (__bt_free(t, pg))
					return (RET_ERROR);
				continue;
			}
		else {
			/* Pack remaining key items at the end of the page. */
			nksize = NBINTERNAL(bi->ksize);
			from = (char *)pg + pg->upper;
			memmove(from + nksize, from, (char *)bi - from);
			pg->upper += nksize;

			/* Adjust indices' offsets, shift the indices down. */
			offset = pg->linp[index];
			for (cnt = index, ip = &pg->linp[0]; cnt--; ++ip)
				if (ip[0] < offset)
					ip[0] += nksize;
			for (cnt = NEXTINDEX(pg) - index; --cnt; ++ip)
				ip[0] = ip[1] < offset ? ip[1] + nksize : ip[1];
			pg->lower -= sizeof(indx_t);
		}

		mpool_put(t->bt_mp, pg, MPOOL_DIRTY);
		break;
	}

	/* Free the leaf page, as long as it wasn't the root. */
	if (h->pgno == P_ROOT) {
		mpool_put(t->bt_mp, h, MPOOL_DIRTY);
		return (RET_SUCCESS);
	}
	return (__bt_free(t, h));
}

/*
 * __bt_dleaf --
 *	Delete a single record from a leaf page.
 *
 * Parameters:
 *	t:	tree
 *    key:	referenced key
 *	h:	page
 *	index:	index on page to delete
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
int
__bt_dleaf(t, key, h, index)
	BTREE *t;
	const DBT *key;
	PAGE *h;
	u_int index;
{
	BLEAF *bl;
	indx_t cnt, *ip, offset;
	u_int32_t nbytes;
	void *to;
	char *from;

	/* If the entry uses overflow pages, make them available for reuse. */
	to = bl = GETBLEAF(h, index);
	if (bl->flags & P_BIGDATA &&
	    __ovfl_delete(t, bl->bytes + bl->ksize) == RET_ERROR)
		return (RET_ERROR);

	/* Pack the remaining key/data items at the end of the page. */
	nbytes = NBLEAF(bl);
	from = (char *)h + h->upper;
	memmove(from + nbytes, from, (char *)to - from);
	h->upper += nbytes;

	/* Adjust the indices' offsets, shift the indices down. */
	offset = h->linp[index];
	for (cnt = index, ip = &h->linp[0]; cnt--; ++ip)
		if (ip[0] < offset)
			ip[0] += nbytes;
	for (cnt = NEXTINDEX(h) - index; --cnt; ++ip)
		ip[0] = ip[1] < offset ? ip[1] + nbytes : ip[1];
	h->lower -= sizeof(indx_t);

	return (RET_SUCCESS);
}
