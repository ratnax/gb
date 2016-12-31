#ifdef __KERNEL__
#include <linux/slab.h>
#else

#include <sys/types.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <list.h>
#endif

#include <db.h>

#define	__MPOOLINTERFACE_PRIVATE
#include <mpool.h>
#include "balloc.h"

static BKT *mpool_look (MPOOL *, pgno_t);
static int  mpool_write (MPOOL *, BKT *);
static BKT *mpool_bkt(MPOOL *, size_t);
static void *__mpool_get(MPOOL *, pgno_t, int, u_int);

/*
 * mpool_open --
 *	Initialize a memory pool.
 */
MPOOL *
mpool_open(fname, file, pagesize, maxcache)
	const char *fname;
#ifdef __KERNEL__
	struct file *file;
#else
	int file;
#endif
	pgno_t pagesize, maxcache;
{
	MPOOL *mp;
	int entry, ret;

	/* Allocate and initialize the MPOOL cookie. */
	if ((mp = (MPOOL *)kzalloc(sizeof(MPOOL), GFP_KERNEL)) == NULL)
		return (NULL);
	INIT_LIST_HEAD(&mp->lqh);
	for (entry = 0; entry < HASHSIZE; ++entry)
		INIT_HLIST_HEAD(&mp->hqh[entry]);
	mp->maxcache = maxcache;
	mp->pagesize = pagesize;
	mp->file = file;

	ret = balloc_init(mp);
	if (ret) {
		printk(KERN_ERR "GBFS: failed to init ballocator\n");
		kfree(mp);
		mp = NULL;
	}
	return (mp);
}

/*
 * mpool_bkt
 *	Get a page from the cache (or create one).
 */
static BKT *
mpool_bkt(mp, npgs)
	MPOOL *mp;
	size_t npgs;
{
	struct hlist_head *head;
	BKT *bp, *tmp;

	/* If under the max cached, always create a new page. */
	if (mp->curcache < mp->maxcache)
		goto new;

	/*
	 * If the cache is max'd out, walk the lru list for a buffer we
	 * can flush.  If we find one, write it (if necessary) and take it
	 * off any lists.  If we don't find anything we grow the cache anyway.
	 * The cache never shrinks.
	 */
	list_for_each_entry_safe(bp, tmp, &mp->lqh, q) {
		if (!(bp->pinned)) {
			/* Flush if dirty. */
			if (bp->flags & MPOOL_DIRTY &&
			    mpool_write(mp, bp) == RET_ERROR)
				return (NULL);

			/* Remove from the hash and lru queues. */
			head = &mp->hqh[HASHKEY(bp->pgno)];
			list_del(&bp->q);
			hlist_del(&bp->hq);
			kfree(bp);
			break;
		}
	}

new:	
	bp = (BKT *)kmalloc(sizeof(BKT) + npgs * mp->pagesize, GFP_KERNEL);
	if (bp) {
		bp->page = (char *)bp + sizeof(BKT);
		bp->npgs = npgs;
		bp->pinned = 0;
		INIT_HLIST_NODE(&bp->hq);
		++mp->curcache;
	}
	return (bp);
}

/*
 * mpool_get
 *	Get a page.
 */
static void *
__mpool_get(mp, pgno, npgs, flags)
	MPOOL *mp;
	pgno_t pgno;
	int npgs;
	u_int flags;				/* XXX not used? */
{
	struct hlist_head *head;
	BKT *bp;
	off_t off;
	int nr;

	if (pgno >= 40 && !(flags & MPOOL_NEW) &&
		!mpool_alloced(mp, pgno * mp->pagesize))
		return NULL;

	/* Check for a page that is cached. */
	if ((bp = mpool_look(mp, pgno)) != NULL) {
		if (bp->pinned) {
			printk(KERN_ERR "mpool_get: page %d already pinned\n", bp->pgno);
			BUG();
		}
		BUG_ON(bp->npgs != npgs);
		/*
		 * Move the page to the head of the hash chain and the tail
		 * of the lru chain.
		 */
		head = &mp->hqh[HASHKEY(bp->pgno)];
		hlist_del(&bp->hq);
		hlist_add_head(&bp->hq, head);
		list_del(&bp->q);
		list_add_tail(&bp->q, &mp->lqh);

		/* Return a pinned page. */
		bp->pinned++;

		return (bp->page);
	}

	/* Get a page from the cache. */
	if ((bp = mpool_bkt(mp, npgs)) == NULL) 
		return (NULL);
	

	/* Read in the contents. */
	if (!(flags & MPOOL_NEW)) {
		off = mp->pagesize * pgno;


		nr = kernel_read(mp->file, off, bp->page, npgs * mp->pagesize);
		if (nr != npgs * mp->pagesize) {
			printk(KERN_ERR "GBFS: Failed to read page at %ld\n", off);
	//		if (nr >= 0)
	//			errno = EFTYPE; TODO
			return (NULL);
		}
	}

	/* Set the page number, pin the page. */
	bp->pgno = pgno;
	bp->npgs = npgs;
	bp->pinned++;

	/*
	 * Add the page to the head of the hash chain and the tail
	 * of the lru chain.
	 */
	head = &mp->hqh[HASHKEY(bp->pgno)];
	hlist_add_head(&bp->hq, head);
	list_add_tail(&bp->q, &mp->lqh);

	return (bp->page);
}

/*
 * mpool_new --
 *	Get a new page of memory.
 */
void *
mpool_new(mp, pgnoaddr, offaddr, size)
	MPOOL *mp;
	pgno_t *pgnoaddr;
	off_t *offaddr;
	size_t size;
{
	int ret;
	loff_t blk;
	size_t order;

	for (order = 0; size > (1 << order); order++);

	ret = mpool_balloc(mp, order, &blk);
	if (ret)
		return NULL;

	*pgnoaddr = blk / mp->pagesize;
	*offaddr = blk % mp->pagesize;


	return __mpool_get(mp, blk / mp->pagesize, 
			((1 << order) / mp->pagesize) ? : 1, MPOOL_NEW);
}


void *
mpool_new_pg (mp, pgnoaddr)
	MPOOL *mp;
	pgno_t *pgnoaddr;
{
	int ret;
	int order;
	uint64_t blk;

	for (order = 0; mp->pagesize > (1 << order); order++);

#if 0
	static int k = 0;
	if (!k) 
		k = lseek(mp->file, 0, SEEK_END) / 4096;	
	*pgnoaddr = k++; 
#else

	ret = mpool_balloc(mp, order, &blk);
	if (ret) {
		printk(KERN_ERR "alloc failed\n");
		return NULL;
	}

	*pgnoaddr = blk / mp->pagesize;
#endif
	return __mpool_get(mp, *pgnoaddr, 1, MPOOL_NEW);
}


void *
mpool_get(mp, blk, size, pgnoaddr, offaddr, flags)
	MPOOL *mp;
	loff_t blk;
	size_t size;
	pgno_t *pgnoaddr;
	off_t *offaddr;
	u_int flags;			
{
	int order;

	for (order = 0; size > (1 << order); order++);
	
	*pgnoaddr = blk / mp->pagesize;
	*offaddr = blk % mp->pagesize;

	return __mpool_get(mp, blk / mp->pagesize, 
			((1 << order) / mp->pagesize) ? : 1, 0);
}

void *
mpool_get_pg(mp, pgno, flags)
	MPOOL *mp;
	pgno_t pgno;
	u_int flags;		
{
	return __mpool_get(mp, pgno, 1, flags);
}


/*
 * mpool_put
 *	Return a page.
 */
int
mpool_put(mp, page, flags)
	MPOOL *mp;
	void *page;
	u_int flags;
{
	BKT *bp = (BKT *)((char *)page - sizeof(BKT));

	if (!(bp->pinned)) {
		printk(KERN_ERR "mpool_put: page %d not pinned\n", bp->pgno);
		BUG();
	}

	bp->pinned--;
	bp->flags |= flags & MPOOL_DIRTY;
	return (RET_SUCCESS);
}

int mpool_free_pg(mp, page)
	MPOOL *mp;
	void *page;
{
	int order;
	BKT *bp = (BKT *)((char *)page - sizeof(BKT));

	bp->pinned--;
	bp->flags &= ~MPOOL_DIRTY;

	for (order = 0; mp->pagesize > (1 << order); order++);

	mpool_bfree(mp, bp->pgno * mp->pagesize);
	return 0;
}

/*
 * mpool_close
 *	Close the buffer pool.
 */
int
mpool_close(mp)
	MPOOL *mp;
{
	BKT *bp, *tmp;

	/* Free up any space allocated to the lru pages. */
	list_for_each_entry_safe(bp, tmp, &mp->lqh, q) {
		list_del(&bp->q);
		kfree(bp);
	}

	/* Free the MPOOL cookie. */
	kfree(mp);
	return (RET_SUCCESS);
}

/*
 * mpool_sync
 *	Sync the pool to disk.
 */
int
mpool_sync(mp)
	MPOOL *mp;
{
	BKT *bp;

	/* Walk the lru chain, flushing any dirty pages to disk. */
	list_for_each_entry(bp, &mp->lqh, q) {
		if (bp->flags & MPOOL_DIRTY &&
		    mpool_write(mp, bp) == RET_ERROR)
			return (RET_ERROR);
	}

	/* Sync the file descriptor. */
	return (vfs_fsync(mp->file, 0) ? RET_ERROR : RET_SUCCESS);
}

/*
 * mpool_write
 *	Write a page to disk.
 */
static int
mpool_write(mp, bp)
	MPOOL *mp;
	BKT *bp;
{
	off_t off;
	size_t bwrite;

	off = mp->pagesize * bp->pgno;
	bwrite = kernel_write(mp->file, bp->page, mp->pagesize * bp->npgs, off);
   	if (bwrite != bp->npgs * mp->pagesize)
		return (RET_ERROR);

	bp->flags &= ~MPOOL_DIRTY;
	return (RET_SUCCESS);
}

/*
 * mpool_look
 *	Lookup a page in the cache.
 */
static BKT *
mpool_look(mp, pgno)
	MPOOL *mp;
	pgno_t pgno;
{
	struct hlist_head *head;
	BKT *bp;

	head = &mp->hqh[HASHKEY(pgno)];
	hlist_for_each_entry(bp, head, hq) {
		if (bp->pgno == pgno) {
			return (bp);
		}
	}
	return (NULL);
}
