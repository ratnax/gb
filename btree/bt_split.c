#ifdef __KERNEL__
#include <linux/slab.h>
#else
#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <db.h>
#include "btree.h"

static int	 bt_broot (BTREE *, PAGE *, PAGE *, PAGE *);
static PAGE	*bt_page
		    (BTREE *, PAGE *, PAGE **, PAGE **, indx_t *, size_t);
static PAGE	*bt_psplit
		    (BTREE *, PAGE *, PAGE *, PAGE *, indx_t *, size_t);
static PAGE	*bt_root
		    (BTREE *, PAGE *, PAGE **, PAGE **, indx_t *, size_t);

/*
 * __BT_SPLIT -- Split the tree.
 *
 * Parameters:
 *	t:	tree
 *	sp:	page to split
 *	key:	key to insert
 *	data:	data to insert
 *	flags:	BIGKEY/BIGDATA flags
 *	ilen:	insert length
 *	skip:	index to leave open
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__bt_split(t, sp, key, data, flags, ilen, argskip)
	BTREE *t;
	PAGE *sp;
	const DBT *key, *data;
	int flags;
	size_t ilen;
	u_int32_t argskip;
{
	BINTERNAL *bi = NULL;
	BLEAF *bl = NULL, *tbl;
	DBT a, b;
	EPGNO *parent;
	PAGE *h, *l, *r, *lchild, *rchild;
	indx_t nxtindex;
	u_int16_t skip;
	u_int32_t n, nbytes, nksize = 0;
	int parentsplit;
	char *dest;

	/*
	 * Split the page into two pages, l and r.  The split routines return
	 * a pointer to the page into which the key should be inserted and with
	 * skip set to the offset which should be used.  Additionally, l and r
	 * are pinned.
	 */
	skip = argskip;
	h = sp->pgno == P_ROOT ?
	    bt_root(t, sp, &l, &r, &skip, ilen) :
	    bt_page(t, sp, &l, &r, &skip, ilen);
	if (h == NULL) 
		return (RET_ERROR);

	/*
	 * Insert the new key/data pair into the leaf page.  (Key inserts
	 * always cause a leaf page to split first.)
	 */
	h->linp[skip] = h->upper -= ilen;
	dest = (char *)h + h->upper;
	WR_BLEAF(dest, key, data, flags)

	/* If the root page was split, make it look right. */
	if (sp->pgno == P_ROOT && bt_broot(t, sp, l, r) == RET_ERROR)
		goto err2;

	/*
	 * Now we walk the parent page stack -- a LIFO stack of the pages that
	 * were traversed when we searched for the page that split.  Each stack
	 * entry is a page number and a page index offset.  The offset is for
	 * the page traversed on the search.  We've just split a page, so we
	 * have to insert a new key into the parent page.
	 *
	 * If the insert into the parent page causes it to split, may have to
	 * continue splitting all the way up the tree.  We stop if the root
	 * splits or the page inserted into didn't have to split to hold the
	 * new key.  Some algorithms replace the key for the old page as well
	 * as the new page.  We don't, as there's no reason to believe that the
	 * first key on the old page is any better than the key we have, and,
	 * in the case of a key being placed at index 0 causing the split, the
	 * key is unavailable.
	 *
	 * There are a maximum of 5 pages pinned at any time.  We keep the left
	 * and right pages pinned while working on the parent.   The 5 are the
	 * two children, left parent and right parent (when the parent splits)
	 * and the root page or the overflow key page when calling bt_preserve.
	 * This code must make sure that all pins are released other than the
	 * root page or overflow page which is unlocked elsewhere.
	 */
	while ((parent = BT_POP(t)) != NULL) {
		lchild = l;
		rchild = r;

		/* Get the parent page. */
		if ((h = mpool_get_pg(t->bt_mp, parent->pgno, 0)) == NULL)
			goto err2;

	 	/*
		 * The new key goes ONE AFTER the index, because the split
		 * was to the right.
		 */
		skip = parent->index + 1;

		/*
		 * Calculate the space needed on the parent page.
		 *
		 * Prefix trees: space hack when inserting into BINTERNAL
		 * pages.  Retain only what's needed to distinguish between
		 * the new entry and the LAST entry on the page to its left.
		 * If the keys compare equal, retain the entire key.  Note,
		 * we don't touch overflow keys, and the entire key must be
		 * retained for the next-to-left most key on the leftmost
		 * page of each level, or the search will fail.  Applicable
		 * ONLY to internal pages that have leaf pages as children.
		 * Further reduction of the key between pairs of internal
		 * pages loses too much information.
		 */
		switch (rchild->flags & P_TYPE) {
		case P_BINTERNAL:
			bi = GETBINTERNAL(rchild, 0);
			nbytes = NBINTERNAL(bi->ksize);
			break;
		case P_BLEAF:
			bl = GETBLEAF(rchild, 0);
			nbytes = NBINTERNAL(bl->ksize);
			if (t->bt_pfx && 
			    (!__bt_isleftmost(t) || skip > 1)) {
				tbl = GETBLEAF(lchild, NEXTINDEX(lchild) - 1);
				a.size = tbl->ksize;
				a.data = tbl->bytes;
				b.size = bl->ksize;
				b.data = bl->bytes;
				nksize = t->bt_pfx(&a, &b);
				n = NBINTERNAL(nksize);
				if (n < nbytes) {
					nbytes = n;
				} else
					nksize = 0;
			} else
				nksize = 0;
			break;
		default:
			BUG();
		}

		/* Split the parent page if necessary or shift the indices. */
		if (h->upper - h->lower < nbytes + sizeof(indx_t)) {
			sp = h;
			h = h->pgno == P_ROOT ?
			    bt_root(t, h, &l, &r, &skip, nbytes) :
			    bt_page(t, h, &l, &r, &skip, nbytes);
			if (h == NULL)
				goto err1;
			parentsplit = 1;
		} else {
			if (skip < (nxtindex = NEXTINDEX(h)))
				memmove(h->linp + skip + 1, h->linp + skip,
				    (nxtindex - skip) * sizeof(indx_t));
			h->lower += sizeof(indx_t);
			parentsplit = 0;
		}

		/* Insert the key into the parent page. */
		switch (rchild->flags & P_TYPE) {
		case P_BINTERNAL:
			h->linp[skip] = h->upper -= nbytes;
			dest = (char *)h + h->linp[skip];
			memmove(dest, bi, nbytes);
			((BINTERNAL *)dest)->pgno = rchild->pgno;
			break;
		case P_BLEAF:
			h->linp[skip] = h->upper -= nbytes;
			dest = (char *)h + h->linp[skip];
			WR_BINTERNAL(dest, nksize ? nksize : bl->ksize,
			    rchild->pgno, 0);
			memmove(dest, bl->bytes, nksize ? nksize : bl->ksize);
			break;
		default:
			BUG();	
		}

		/* Unpin the held pages. */
		if (!parentsplit) {
			mpool_put(t->bt_mp, h, MPOOL_DIRTY);
			break;
		}

		/* If the root page was split, make it look right. */
		if (sp->pgno == P_ROOT &&
		    bt_broot(t, sp, l, r) == RET_ERROR)
			goto err1;

		mpool_put(t->bt_mp, lchild, MPOOL_DIRTY);
		mpool_put(t->bt_mp, rchild, MPOOL_DIRTY);
	}

	/* Unpin the held pages. */
	mpool_put(t->bt_mp, l, MPOOL_DIRTY);
	mpool_put(t->bt_mp, r, MPOOL_DIRTY);

	/* Clear any pages left on the stack. */
	return (RET_SUCCESS);

	/*
	 * If something fails in the above loop we were already walking back
	 * up the tree and the tree is now inconsistent.  Nothing much we can
	 * do about it but release any memory we're holding.
	 */
err1:	
	mpool_put(t->bt_mp, lchild, MPOOL_DIRTY);
	mpool_put(t->bt_mp, rchild, MPOOL_DIRTY);

err2:	
	mpool_put(t->bt_mp, l, 0);
	mpool_put(t->bt_mp, r, 0);
	return (RET_ERROR);
}

/*
 * BT_PAGE -- Split a non-root page of a btree.
 *
 * Parameters:
 *	t:	tree
 *	h:	root page
 *	lp:	pointer to left page pointer
 *	rp:	pointer to right page pointer
 *	skip:	pointer to index to leave open
 *	ilen:	insert length
 *
 * Returns:
 *	Pointer to page in which to insert or NULL on error.
 */
static PAGE *
bt_page(t, h, lp, rp, skip, ilen)
	BTREE *t;
	PAGE *h, **lp, **rp;
	indx_t *skip;
	size_t ilen;
{
	PAGE *l, *r, *tp;
	pgno_t npg;

	/* Put the new right page for the split into place. */
	if ((r = __bt_new(t, &npg)) == NULL)
		return (NULL);
	r->pgno = npg;
	r->lower = BTDATAOFF;
	r->upper = t->bt_psize;
	r->flags = h->flags & P_TYPE;

	/*
	 * If we're splitting the last page on a level because we're appending
	 * a key to it (skip is NEXTINDEX()), it's likely that the data is
	 * sorted.  Adding an empty page on the side of the level is less work
	 * and can push the fill factor much higher than normal.  If we're
	 * wrong it's no big deal, we'll just do the split the right way next
	 * time.  It may look like it's equally easy to do a similar hack for
	 * reverse sorted data, that is, split the tree left, but it's not.
	 * Don't even try.
	 */
	if (__bt_isrightmost(t) && *skip == NEXTINDEX(h)) {
		r->lower = BTDATAOFF + sizeof(indx_t);
		*skip = 0;
		*lp = h;
		*rp = r;
		return (r);
	}

	/* Put the new left page for the split into place. */
	if ((l = (PAGE *)kmalloc(t->bt_psize, GFP_KERNEL)) == NULL) {
		mpool_put(t->bt_mp, r, 0);
		return (NULL);
	}
#ifdef PURIFY
	memset(l, 0xff, t->bt_psize);
#endif
	l->pgno = h->pgno;
	l->lower = BTDATAOFF;
	l->upper = t->bt_psize;
	l->flags = h->flags & P_TYPE; 

	/*
	 * Split right.  The key/data pairs aren't sorted in the btree page so
	 * it's simpler to copy the data from the split page onto two new pages
	 * instead of copying half the data to the right page and compacting
	 * the left page in place.  Since the left page can't change, we have
	 * to swap the original and the allocated left page after the split.
	 */
	tp = bt_psplit(t, h, l, r, skip, ilen);

	/* Move the new left page onto the old left page. */
	memmove(h, l, t->bt_psize);
	if (tp == l)
		tp = h;
	kfree(l);

	*lp = h;
	*rp = r;
	return (tp);
}

/*
 * BT_ROOT -- Split the root page of a btree.
 *
 * Parameters:
 *	t:	tree
 *	h:	root page
 *	lp:	pointer to left page pointer
 *	rp:	pointer to right page pointer
 *	skip:	pointer to index to leave open
 *	ilen:	insert length
 *
 * Returns:
 *	Pointer to page in which to insert or NULL on error.
 */
static PAGE *
bt_root(t, h, lp, rp, skip, ilen)
	BTREE *t;
	PAGE *h, **lp, **rp;
	indx_t *skip;
	size_t ilen;
{
	PAGE *l, *r, *tp;
	pgno_t lnpg, rnpg;

	/* Put the new left and right pages for the split into place. */
	if ((l = __bt_new(t, &lnpg)) == NULL ||
	    (r = __bt_new(t, &rnpg)) == NULL)
		return (NULL);
	l->pgno = lnpg;
	r->pgno = rnpg;
	l->lower = r->lower = BTDATAOFF;
	l->upper = r->upper = t->bt_psize;
	l->flags = r->flags = h->flags & P_TYPE;

	/* Split the root page. */
	tp = bt_psplit(t, h, l, r, skip, ilen);

	*lp = l;
	*rp = r;
	return (tp);
}

/*
 * BT_BROOT -- Fix up the btree root page after it has been split.
 *
 * Parameters:
 *	t:	tree
 *	h:	root page
 *	l:	left page
 *	r:	right page
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
static int
bt_broot(t, h, l, r)
	BTREE *t;
	PAGE *h, *l, *r;
{
	BINTERNAL *bi;
	BLEAF *bl;
	u_int32_t nbytes;
	char *dest;

	/*
	 * If the root page was a leaf page, change it into an internal page.
	 * We copy the key we split on (but not the key's data, in the case of
	 * a leaf page) to the new root page.
	 *
	 * The btree comparison code guarantees that the left-most key on any
	 * level of the tree is never used, so it doesn't need to be filled in.
	 */
	nbytes = NBINTERNAL(0);
	h->linp[0] = h->upper = t->bt_psize - nbytes;
	dest = (char *)h + h->upper;
	WR_BINTERNAL(dest, 0, l->pgno, 0);

	switch (h->flags & P_TYPE) {
	case P_BLEAF:
		bl = GETBLEAF(r, 0);
		nbytes = NBINTERNAL(bl->ksize);
		h->linp[1] = h->upper -= nbytes;
		dest = (char *)h + h->upper;
		WR_BINTERNAL(dest, bl->ksize, r->pgno, 0);
		memmove(dest, bl->bytes, bl->ksize);
		break;
	case P_BINTERNAL:
		bi = GETBINTERNAL(r, 0);
		nbytes = NBINTERNAL(bi->ksize);
		h->linp[1] = h->upper -= nbytes;
		dest = (char *)h + h->upper;
		memmove(dest, bi, nbytes);
		((BINTERNAL *)dest)->pgno = r->pgno;
		break;
	default:
		BUG();
	}

	/* There are two keys on the page. */
	h->lower = BTDATAOFF + 2 * sizeof(indx_t);

	/* Unpin the root page, set to btree internal page. */
	h->flags &= ~P_TYPE;
	h->flags |= P_BINTERNAL;
	mpool_put(t->bt_mp, h, MPOOL_DIRTY);

	return (RET_SUCCESS);
}

/*
 * BT_PSPLIT -- Do the real work of splitting the page.
 *
 * Parameters:
 *	t:	tree
 *	h:	page to be split
 *	l:	page to put lower half of data
 *	r:	page to put upper half of data
 *	pskip:	pointer to index to leave open
 *	ilen:	insert length
 *
 * Returns:
 *	Pointer to page in which to insert.
 */
static PAGE *
bt_psplit(t, h, l, r, pskip, ilen)
	BTREE *t;
	PAGE *h, *l, *r;
	indx_t *pskip;
	size_t ilen;
{
	BINTERNAL *bi;
	BLEAF *bl;
	PAGE *rval;
	void *src;
	indx_t full, half, nxt, off, skip, top, used;
	u_int32_t nbytes;

	/*
	 * Split the data to the left and right pages.  Leave the skip index
	 * open.  Additionally, make some effort not to split on an overflow
	 * key.  This makes internal page processing faster and can save
	 * space as overflow keys used by internal pages are never deleted.
	 */
	skip = *pskip;
	full = t->bt_psize - BTDATAOFF;
	half = full / 2;
	used = 0;
	for (nxt = off = 0, top = NEXTINDEX(h); nxt < top; ++off) {
		if (skip == off) {
			nbytes = ilen;
		} else
			switch (h->flags & P_TYPE) {
			case P_BINTERNAL:
				src = bi = GETBINTERNAL(h, nxt);
				nbytes = NBINTERNAL(bi->ksize);
				break;
			case P_BLEAF:
				src = bl = GETBLEAF(h, nxt);
				nbytes = NBLEAF(bl);
				break;
			default:
				BUG();
			}

		/*
		 * If the key/data pairs are substantial fractions of the max
		 * possible size for the page, it's possible to get situations
		 * where we decide to try and copy too much onto the left page.
		 * Make sure that doesn't happen.
		 */
		if (skip <= off && used + nbytes + sizeof(indx_t) >= full) {
			--off;
			break;
		}

		/* Copy the key/data pair, if not the skipped index. */
		if (skip != off) {
			++nxt;

			l->linp[off] = l->upper -= nbytes;
			memmove((char *)l + l->upper, src, nbytes);
		}

		used += nbytes + sizeof(indx_t);
		if (used >= half) 
			break;
	}

	/*
	 * Off is the last offset that's valid for the left page.
	 * Nxt is the first offset to be placed on the right page.
	 */
	l->lower += (off + 1) * sizeof(indx_t);

	/*
	 * If the skipped index was on the left page, just return that page.
	 * Otherwise, adjust the skip index to reflect the new position on
	 * the right page.
	 */
	if (skip <= off) {
		skip = 0;
		rval = l;
	} else {
		rval = r;
		*pskip -= nxt;
	}

	for (off = 0; nxt < top; ++off) {
		if (skip == nxt) {
			++off;
			skip = 0;
		}
		switch (h->flags & P_TYPE) {
		case P_BINTERNAL:
			src = bi = GETBINTERNAL(h, nxt);
			nbytes = NBINTERNAL(bi->ksize);
			break;
		case P_BLEAF:
			src = bl = GETBLEAF(h, nxt);
			nbytes = NBLEAF(bl);
			break;
		default:
			BUG();
		}
		++nxt;
		r->linp[off] = r->upper -= nbytes;
		memmove((char *)r + r->upper, src, nbytes);
	}
	r->lower += off * sizeof(indx_t);

	/* If the key is being appended to the page, adjust the index. */
	if (skip == top)
		r->lower += sizeof(indx_t);

	return (rval);
}
