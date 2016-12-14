/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)mpool.c	8.5 (Berkeley) 7/26/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <db.h>

#define	__MPOOLINTERFACE_PRIVATE
#include <mpool.h>
#include <buddy.h>

static BKT *mpool_look __P((MPOOL *, pgno_t));
static int  mpool_write __P((MPOOL *, BKT *));

/*
 * mpool_open --
 *	Initialize a memory pool.
 */
MPOOL *
mpool_open(key, fd, pagesize, maxcache)
	void *key;
	int fd;
	pgno_t pagesize, maxcache;
{
	struct stat sb;
	MPOOL *mp;
	int entry;

	/*
	 * Get information about the file.
	 *
	 * XXX
	 * We don't currently handle pipes, although we should.
	 */
	if (fstat(fd, &sb))
		return (NULL);
	if (!S_ISREG(sb.st_mode)) {
		errno = ESPIPE;
		return (NULL);
	}

	/* Allocate and initialize the MPOOL cookie. */
	if ((mp = (MPOOL *)calloc(1, sizeof(MPOOL))) == NULL)
		return (NULL);
	CIRCLEQ_INIT(&mp->lqh);
	for (entry = 0; entry < HASHSIZE; ++entry)
		CIRCLEQ_INIT(&mp->hqh[entry]);
	mp->maxcache = maxcache;
	mp->npages = sb.st_size / pagesize;
	mp->pagesize = pagesize;
	mp->fd = fd;

	buddy_init(0);
	return (mp);
}

/*
 * mpool_filter --
 *	Initialize input/output filters.
 */
void
mpool_filter(mp, pgin, pgout, pgcookie)
	MPOOL *mp;
	void (*pgin) __P((void *, pgno_t, void *));
	void (*pgout) __P((void *, pgno_t, void *));
	void *pgcookie;
{
	mp->pgin = pgin;
	mp->pgout = pgout;
	mp->pgcookie = pgcookie;
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
	struct _hqh *head;
	BKT *bp;

	/* If under the max cached, always create a new page. */
	if (mp->curcache < mp->maxcache)
		goto new;

	/*
	 * If the cache is max'd out, walk the lru list for a buffer we
	 * can flush.  If we find one, write it (if necessary) and take it
	 * off any lists.  If we don't find anything we grow the cache anyway.
	 * The cache never shrinks.
	 */
	for (bp = mp->lqh.cqh_first;
	    bp != (void *)&mp->lqh; bp = bp->q.cqe_next)
		if (!(bp->pinned)) {
			/* Flush if dirty. */
			if (bp->flags & MPOOL_DIRTY &&
			    mpool_write(mp, bp) == RET_ERROR)
				return (NULL);

			/* Remove from the hash and lru queues. */
			head = &mp->hqh[HASHKEY(bp->pgno)];
			CIRCLEQ_REMOVE(head, bp, hq);
			CIRCLEQ_REMOVE(&mp->lqh, bp, q);
			free(bp);
			break;
		}

new:	if ((bp = (BKT *)malloc(sizeof(BKT) + npgs * mp->pagesize)) == NULL)
		return (NULL);
	bp->page = (char *)bp + sizeof(BKT);
	bp->npgs = npgs;
	bp->pinned = 0;
	++mp->curcache;
	return (bp);
}


/*
 * mpool_get
 *	Get a page.
 */
void *
__mpool_get(mp, pgno, npgs, flags)
	MPOOL *mp;
	pgno_t pgno;
	int npgs;
	u_int flags;				/* XXX not used? */
{
	struct _hqh *head;
	BKT *bp;
	off_t off;
	int nr, i;


	/* Check for attempt to retrieve a non-existent page. */
	if (0 && pgno >= mp->npages) {
		errno = EINVAL;
		return (NULL);
	}

	/* Check for a page that is cached. */
	if ((bp = mpool_look(mp, pgno)) != NULL) {
		if (bp->pinned) {
			(void)fprintf(stderr,
			    "mpool_get: page %d already pinned\n", bp->pgno);
			abort();
		}
		assert(bp->npgs == npgs);
		/*
		 * Move the page to the head of the hash chain and the tail
		 * of the lru chain.
		 */
		head = &mp->hqh[HASHKEY(bp->pgno)];
		CIRCLEQ_REMOVE(head, bp, hq);
		CIRCLEQ_INSERT_HEAD(head, bp, hq);
		CIRCLEQ_REMOVE(&mp->lqh, bp, q);
		CIRCLEQ_INSERT_TAIL(&mp->lqh, bp, q);

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
		if (lseek(mp->fd, off, SEEK_SET) != off)
			return (NULL);

		nr = read(mp->fd, bp->page, npgs * mp->pagesize);
		if (nr != npgs * mp->pagesize) {
			if (nr >= 0)
				errno = EFTYPE;
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
	CIRCLEQ_INSERT_HEAD(head, bp, hq);
	CIRCLEQ_INSERT_TAIL(&mp->lqh, bp, q);

	/* Run through the user's filter. */
	if (mp->pgin != NULL) {
		for (i = 0; i < npgs; i++)
			(mp->pgin)(mp->pgcookie, bp->pgno + i, bp->page + i * mp->pagesize);
	}

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
	struct _hqh *head;
	BKT *bp;
	loff_t blk;
	size_t order;

	for (order = 0; size > (1 << order); order++);
	
	blk = balloc(order);

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
	int order;

	for (order = 0; mp->pagesize > (1 << order); order++);

	*pgnoaddr = balloc(order) / mp->pagesize;
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
	struct _hqh *head;
	BKT *bp;
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
		(void)fprintf(stderr,
		    "mpool_put: page %d not pinned\n", bp->pgno);
		abort();
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

	bfree(bp->pgno * mp->pagesize, order);
}

/*
 * mpool_close
 *	Close the buffer pool.
 */
int
mpool_close(mp)
	MPOOL *mp;
{
	BKT *bp;

	/* Free up any space allocated to the lru pages. */
	while ((bp = mp->lqh.cqh_first) != (void *)&mp->lqh) {
		CIRCLEQ_REMOVE(&mp->lqh, mp->lqh.cqh_first, q);
		free(bp);
	}

	/* Free the MPOOL cookie. */
	free(mp);
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
	for (bp = mp->lqh.cqh_first;
	    bp != (void *)&mp->lqh; bp = bp->q.cqe_next)
		if (bp->flags & MPOOL_DIRTY &&
		    mpool_write(mp, bp) == RET_ERROR)
			return (RET_ERROR);

	/* Sync the file descriptor. */
	return (fsync(mp->fd) ? RET_ERROR : RET_SUCCESS);
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
	int i;
	off_t off;
	size_t bwrite;

#ifdef STATISTICS
	++mp->pagewrite;
#endif

	/* Run through the user's filter. */
	if (mp->pgout) {
		for (i = 0; i < bp->npgs; i++) 
			(mp->pgout)(mp->pgcookie, bp->pgno + i, 
					bp->page + mp->pagesize * i);
	}

	off = mp->pagesize * bp->pgno;
	if (lseek(mp->fd, off, SEEK_SET) != off)
		return (RET_ERROR);
	bwrite = write(mp->fd, bp->page, mp->pagesize * bp->npgs);
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
	struct _hqh *head;
	BKT *bp;

	head = &mp->hqh[HASHKEY(pgno)];
	for (bp = head->cqh_first; bp != (void *)head; bp = bp->hq.cqe_next)
		if (bp->pgno == pgno) {
#ifdef STATISTICS
			++mp->cachehit;
#endif
			return (bp);
		}
#ifdef STATISTICS
	++mp->cachemiss;
#endif
	return (NULL);
}
