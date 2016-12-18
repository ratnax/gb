#include <db.h>
#include "btree.h"

/*
 * __BT_PUT -- Add a btree item to the tree.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key
 *	data:	data
 *	flag:	R_NOOVERWRITE
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key is already in the
 *	tree and R_NOOVERWRITE specified.
 */
int
__bt_put(dbp, key, data, flags)
	const DB *dbp;
	DBT *key;
	const DBT *data;
	u_int flags;
{
	BTREE *t;
	DBT tkey, tdata;
	EPG *e;
	PAGE *h;
	indx_t index, nxtindex;
	pgno_t pg;
	u_int32_t nbytes;
	int dflags, exact, status;
	char *dest, db[NOVFLSIZE], kb[NOVFLSIZE];

	t = dbp->internal;

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}

	/*
	 * If the key/data pair won't fit on a page, store it on overflow
	 * pages.  Only put the key on the overflow page if the pair are
	 * still too big after moving the data to an overflow page.
	 *
	 * XXX
	 * If the insert fails later on, the overflow pages aren't recovered.
	 */
	dflags = 0;
	if (key->size + data->size > t->bt_ovflsize) {
		if (key->size > t->bt_ovflsize) {
storekey:		if (__ovfl_put(t, key, &pg) == RET_ERROR)
				return (RET_ERROR);
			tkey.data = kb;
			tkey.size = NOVFLSIZE;
			memmove(kb, &pg, sizeof(pgno_t));
			memmove(kb + sizeof(pgno_t),
			    &key->size, sizeof(u_int32_t));
			dflags |= P_BIGKEY;
			key = &tkey;
		}
		if (key->size + data->size > t->bt_ovflsize) {
			if (__ovfl_put(t, data, &pg) == RET_ERROR)
				return (RET_ERROR);
			tdata.data = db;
			tdata.size = NOVFLSIZE;
			memmove(db, &pg, sizeof(pgno_t));
			memmove(db + sizeof(pgno_t),
			    &data->size, sizeof(u_int32_t));
			dflags |= P_BIGDATA;
			data = &tdata;
		}
		if (key->size + data->size > t->bt_ovflsize)
			goto storekey;
	}

	/*
	 * Find the key to delete, or, the location at which to insert.
	 * Bt_fast and __bt_search both pin the returned page.
	 */
	if ((e = __bt_search(t, key, &exact)) == NULL)
		return (RET_ERROR);
	h = e->page;
	index = e->index;

	/*
	 * Add the key/data pair to the tree.  If an identical key is already
	 * in the tree, and R_NOOVERWRITE is set, an error is returned.  If
	 * R_NOOVERWRITE is not set, the key is either added (if duplicates are
	 * permitted) or an error is returned.
	 */
	if (exact) {
		if (__bt_dleaf(t, key, h, index) == RET_ERROR) {
			mpool_put(t->bt_mp, h, 0);
			return (RET_ERROR);
		}
	}

	/*
	 * If not enough room, or the user has put a ceiling on the number of
	 * keys permitted in the page, split the page.  The split code will
	 * insert the key and data and unpin the current page.  If inserting
	 * into the offset array, shift the pointers up.
	 */
	nbytes = NBLEAFDBT(key->size, data->size);
	if (h->upper - h->lower < nbytes + sizeof(indx_t)) {
		if ((status = __bt_split(t, h, key,
		    data, dflags, nbytes, index)) != RET_SUCCESS)
			return (status);
		goto success;
	}

	if (index < (nxtindex = NEXTINDEX(h)))
		memmove(h->linp + index + 1, h->linp + index,
		    (nxtindex - index) * sizeof(indx_t));
	h->lower += sizeof(indx_t);

	h->linp[index] = h->upper -= nbytes;
	dest = (char *)h + h->upper;
	WR_BLEAF(dest, key, data, dflags);

	mpool_put(t->bt_mp, h, MPOOL_DIRTY);

success:
	F_SET(t, B_MODIFIED);
	return (RET_SUCCESS);
}
