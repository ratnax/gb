#ifdef __KERNEL__
#else
#include <sys/types.h>

#include <stdio.h>
#endif

#include <db.h>
#include "btree.h"

/*
 * __bt_free --
 *	Put a page on the freelist.
 *
 * Parameters:
 *	t:	tree
 *	h:	page to free
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 *
 * Side-effect:
 *	mpool_put's the page.
 */
int
__bt_free(t, h)
	BTREE *t;
	PAGE *h;
{
	return mpool_free_pg(t->bt_mp, h);
}

/*
 * __bt_new --
 *	Get a new page, preferably from the freelist.
 *
 * Parameters:
 *	t:	tree
 *	npg:	storage for page number.
 *
 * Returns:
 *	Pointer to a page, NULL on error.
 */
PAGE *
__bt_new(t, npg)
	BTREE *t;
	pgno_t *npg;
{
	return (mpool_new_pg(t->bt_mp, npg));
}
