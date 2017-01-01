#ifdef __KERNEL__
#else
#include <sys/types.h>

#include <stdio.h>
#endif

#include <db.h>
#include "btree.h"

/*
 * __bt_search --
 *	Search a btree for a key.
 *
 * Parameters:
 *	t:	tree to search
 *	key:	key to find
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	The EPG for matching record, if any, or the EPG for the location
 *	of the key, if it were inserted into the tree, is entered into
 *	the bt_cur field of the tree.  A pointer to the field is returned.
 */
EPG *
__bt_search(t, key, exactp)
	BTREE *t;
	const DBT *key;
	int *exactp;
{
	PAGE *h;
	indx_t base, index, lim;
	pgno_t pg;
	int cmp;
	u_int8_t flags;

	BT_CLR(t);
	for (pg = P_ROOT;;) {
		if ((h = mpool_get_pg(t->bt_mp, pg, 0)) == NULL)
			return (NULL);

		/* Do a binary search on the current page. */
		t->bt_cur.page = h;
		for (base = 0, lim = NEXTINDEX(h); lim; lim >>= 1) {
			t->bt_cur.index = index = base + (lim >> 1);
			if ((cmp = __bt_cmp(t, key, &t->bt_cur)) == 0) {
				if (h->flags & P_BLEAF) {
					*exactp = 1;
					return (&t->bt_cur);
				}
				goto next;
			}
			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		/*
		 * If it's a leaf page, we're almost done.  If no duplicates
		 * are allowed, or we have an exact match, we're done.  Else,
		 * it's possible that there were matching keys on this page,
		 * which later deleted, and we're on a page with no matches
		 * while there are matches on other pages.  If at the start or
		 * end of a page, check the adjacent page.
		 */
		if (h->flags & P_BLEAF) {
			*exactp = 0;
			t->bt_cur.index = base;
			return (&t->bt_cur);
		}

		/*
		 * No match found.  Base is the smallest index greater than
		 * key and may be zero or a last + 1 index.  If it's non-zero,
		 * decrement by one, and record the internal page which should
		 * be a parent page for the key.  If a split later occurs, the
		 * inserted page will be to the right of the saved page.
		 */
		index = base ? base - 1 : base;

next:	
		flags = 0;
		if (__bt_isleftmost(t) && index == 0)
			flags |= P_LMOST;
		if (__bt_isrightmost(t) && index == NEXTINDEX(h) - 1)
			flags |= P_RMOST;

		BT_PUSH(t, h->pgno, index, flags);
		pg = GETBINTERNAL(h, index)->pgno;
		mpool_put(t->bt_mp, h, 0);
	}
}
