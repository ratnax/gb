#ifdef __KERNEL__
#else
#include <sys/types.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#endif

#include <db.h>
#include "btree.h"

/*
 * __BT_GET -- Get a record from the btree.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key to find
 *	data:	data to return
 *	flag:	currently unused
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key not found.
 */
int
__bt_get(dbp, key, data, flags)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int flags;
{
	BTREE *t;
	EPG *e;
	int exact, status;

	t = dbp->internal;
	mutex_lock(&t->mutex);

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}

	/* Get currently doesn't take any flags. */
	if (flags) {
		//errno = EINVAL; TODORATNA
		return (RET_ERROR);
	}

	if ((e = __bt_search(t, key, &exact)) == NULL)
		return (RET_ERROR);
	if (!exact) {
		mpool_put(t->bt_mp, e->page, 0);
		mutex_unlock(&t->mutex);
		return (RET_SPECIAL);
	}

	status = __bt_ret(t, e, NULL, NULL, data, &t->bt_rdata, 0);

	t->bt_pinned = e->page;
	mutex_unlock(&t->mutex);
	return (status);
}
