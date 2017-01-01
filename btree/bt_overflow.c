#ifdef __KERNEL__
#include <linux/slab.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#else
#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <db.h>
#include "btree.h"


/*
 * Big key/data code.
 *
 * Big key and data entries are stored on linked lists of pages.  The initial
 * reference is byte string stored with the key or data and is the page number
 * and size.  The actual record is stored in a chain of pages linked by the
 * nextpg field of the PAGE header.
 *
 * The first page of the chain has a special property.  If the record is used
 * by an internal page, it cannot be deleted and the P_PRESERVE bit will be set
 * in the header.
 *
 * XXX
 * A single DBT is written to each chain, so a lot of space on the last page
 * is wasted.  This is a fairly major bug for some data sets.
 */

/*
 * __OVFL_GET -- Get an overflow key/data item.
 *
 * Parameters:
 *	t:	tree
 *	p:	pointer to { pgno_t, u_int32_t }
 *	buf:	storage address
 *	bufsz:	storage size
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__ovfl_get(t, p, ssz, buf, bufsz)
	BTREE *t;
	void *p;
	size_t *ssz;
	void **buf;
	size_t *bufsz;
{
	void *h;
	pgno_t pg;
	u_int32_t sz;

	memmove(&pg, p, sizeof(pgno_t));
	memmove(&sz, (char *)p + sizeof(pgno_t), sizeof(u_int32_t));
	*ssz = sz;

#ifdef DEBUG
	if (pg == P_INVALID || sz == 0)
		abort();
#endif

	/* Make the buffer bigger as necessary. */
	if (*bufsz < sz) {
		*buf = (char *)(*buf == NULL ? kmalloc(sz, GFP_KERNEL) : 
										krealloc(*buf, sz, GFP_KERNEL));
		if (*buf == NULL)
			return (RET_ERROR);
		*bufsz = sz;
	}

	if ((h = mpool_get(t->bt_mp, pg, sz, 0)) == NULL)
		return (RET_ERROR);

	memmove(*buf, h, sz);
	mpool_put(t->bt_mp, h, 0);
	return (RET_SUCCESS);
}

/*
 * __OVFL_PUT -- Store an overflow key/data item.
 *
 * Parameters:
 *	t:	tree
 *	data:	DBT to store
 *	pgno:	storage page number
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__ovfl_put(t, dbt, pg)
	BTREE *t;
	const DBT *dbt;
	pgno_t *pg;
{
	void *p;

	if ((p = __bt_data_new(t, pg, dbt->size)) == NULL)
		return (RET_ERROR);

	memmove(p, dbt->data, dbt->size);
	mpool_put(t->bt_mp, p, MPOOL_DIRTY);
	return (RET_SUCCESS);
}

/*
 * __OVFL_DELETE -- Delete an overflow chain.
 *
 * Parameters:
 *	t:	tree
 *	p:	pointer to { pgno_t, u_int32_t }
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__ovfl_delete(t, p)
	BTREE *t;
	void *p;
{
	void *h;
	pgno_t pg;
	u_int32_t sz;

	memmove(&pg, p, sizeof(pgno_t));
	memmove(&sz, (char *)p + sizeof(pgno_t), sizeof(u_int32_t));

#ifdef DEBUG
	if (pg == P_INVALID || sz == 0)
		abort();
#endif
	if ((h = mpool_get(t->bt_mp, pg, sz, 0)) == NULL)
		return (RET_ERROR);

	__bt_data_free(t, h);
	return (RET_SUCCESS);
}
