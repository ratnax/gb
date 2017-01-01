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


int
__bt_isleftmost(t)
	BTREE *t;
{
	EPGNO *e = BT_TOP(t);

	return (!e || (e->flags & P_LMOST));
}

int
__bt_isrightmost(t)
	BTREE *t;
{
	EPGNO *e = BT_TOP(t);

	return (!e || (e->flags & P_LMOST));
}

/*
 * __bt_ret --
 *	Build return key/data pair.
 *
 * Parameters:
 *	t:	tree
 *	e:	key/data pair to be returned
 *	key:	user's key structure (NULL if not to be filled in)
 *	rkey:	memory area to hold key
 *	data:	user's data structure (NULL if not to be filled in)
 *	rdata:	memory area to hold data
 *       copy:	always copy the key/data item
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
int
__bt_ret(t, e, key, rkey, data, rdata, copy)
	BTREE *t;
	EPG *e;
	DBT *key, *rkey, *data, *rdata;
	int copy;
{
	BLEAF *bl;
	void *p;

	bl = GETBLEAF(e->page, e->index);

	/*
	 * We must copy big keys/data to make them contigous.  Otherwise,
	 * leave the page pinned and don't copy unless the user specified
	 * concurrent access.
	 */
	if (key == NULL)
		goto dataonly;

	if (copy) {
		if (bl->ksize > rkey->size) {
			p = (void *)(rkey->data == NULL ?
			    kmalloc(bl->ksize, GFP_KERNEL) : 
				krealloc(rkey->data, bl->ksize, GFP_KERNEL));
			if (p == NULL)
				return (RET_ERROR);
			rkey->data = p;
			rkey->size = bl->ksize;
		}
		memmove(rkey->data, bl->bytes, bl->ksize);
		key->size = bl->ksize;
		key->data = rkey->data;
	} else {
		key->size = bl->ksize;
		key->data = bl->bytes;
	}

dataonly:
	if (data == NULL)
		return (RET_SUCCESS);

	if (bl->flags & P_BIGDATA) {
		if (__ovfl_get(t, bl->bytes + bl->ksize,
		    &data->size, &rdata->data, &rdata->size))
			return (RET_ERROR);
		data->data = rdata->data;
	} else if (copy) {
		/* Use +1 in case the first record retrieved is 0 length. */
		if (bl->dsize + 1 > rdata->size) {
			p = (void *)(rdata->data == NULL ?
			    kmalloc(bl->dsize + 1, GFP_KERNEL) :
			    krealloc(rdata->data, bl->dsize + 1, GFP_KERNEL));
			if (p == NULL)
				return (RET_ERROR);
			rdata->data = p;
			rdata->size = bl->dsize + 1;
		}
		memmove(rdata->data, bl->bytes + bl->ksize, bl->dsize);
		data->size = bl->dsize;
		data->data = rdata->data;
	} else {
		data->size = bl->dsize;
		data->data = bl->bytes + bl->ksize;
	}

	return (RET_SUCCESS);
}

/*
 * __BT_CMP -- Compare a key to a given record.
 *
 * Parameters:
 *	t:	tree
 *	k1:	DBT pointer of first arg to comparison
 *	e:	pointer to EPG for comparison
 *
 * Returns:
 *	< 0 if k1 is < record
 *	= 0 if k1 is = record
 *	> 0 if k1 is > record
 */
int
__bt_cmp(t, k1, e)
	BTREE *t;
	const DBT *k1;
	EPG *e;
{
	BINTERNAL *bi;
	BLEAF *bl;
	DBT k2;
	PAGE *h;

	/*
	 * The left-most key on internal pages, at any level of the tree, is
	 * guaranteed by the following code to be less than any user key.
	 * This saves us from having to update the leftmost key on an internal
	 * page when the user inserts a new key in the tree smaller than
	 * anything we've yet seen.
	 */
	h = e->page;
	if (e->index == 0 && __bt_isleftmost(t) && !(h->flags & P_BLEAF))
		return (1);

	if (h->flags & P_BLEAF) {
		bl = GETBLEAF(h, e->index);
		k2.data = bl->bytes;
		k2.size = bl->ksize;
	} else {
		bi = GETBINTERNAL(h, e->index);
		k2.data = bi->bytes;
		k2.size = bi->ksize;
	}
	return ((*t->bt_cmp)(k1, &k2));
}

/*
 * __BT_DEFCMP -- Default comparison routine.
 *
 * Parameters:
 *	a:	DBT #1
 *	b: 	DBT #2
 *
 * Returns:
 *	< 0 if a is < b
 *	= 0 if a is = b
 *	> 0 if a is > b
 */
int
__bt_defcmp(a, b)
	const DBT *a, *b;
{
	register size_t len;
	register u_char *p1, *p2;

	/*
	 * XXX
	 * If a size_t doesn't fit in an int, this routine can lose.
	 * What we need is a integral type which is guaranteed to be
	 * larger than a size_t, and there is no such thing.
	 */
	len = MIN(a->size, b->size);
	for (p1 = a->data, p2 = b->data; len--; ++p1, ++p2)
		if (*p1 != *p2)
			return ((int)*p1 - (int)*p2);
	return ((int)a->size - (int)b->size);
}

/*
 * __BT_DEFPFX -- Default prefix routine.
 *
 * Parameters:
 *	a:	DBT #1
 *	b: 	DBT #2
 *
 * Returns:
 *	Number of bytes needed to distinguish b from a.
 */
size_t
__bt_defpfx(a, b)
	const DBT *a, *b;
{
	register u_char *p1, *p2;
	register size_t cnt, len;

	cnt = 1;
	len = MIN(a->size, b->size);
	for (p1 = a->data, p2 = b->data; len--; ++p1, ++p2, ++cnt)
		if (*p1 != *p2)
			return (cnt);

	/* a->size must be <= b->size, or they wouldn't be in this order. */
	return (a->size < b->size ? a->size + 1 : a->size);
}
