#ifndef _MPOOL_H_
#define _MPOOL_H_
#ifdef __KERNEL__
#include <linux/fs.h>
#else
#include <list.h>
#endif

/*
 * The memory pool scheme is a simple one.  Each in-memory page is referenced
 * by a bucket which is threaded in up to two of three ways.  All active pages
 * are threaded on a hash chain (hashed by page number) and an lru chain.
 * Inactive pages are threaded on a free chain.  Each reference to a memory
 * pool is handed an opaque MPOOL cookie which stores all of this information.
 */
#define	HASHSIZE	128
#define	HASHKEY(pgno)	((pgno - 1) % HASHSIZE)

/* The BKT structures are the elements of the queues. */
typedef struct _bkt {
	struct hlist_node hq;		/* hash queue */
	struct list_head q;		/* lru queue */
	void    *page;			/* page */
	pgno_t   pgno;			/* page number */
	size_t	npgs;

	u_int32_t pinned;
#define	MPOOL_DIRTY	0x01		/* page needs to be written */
#define	MPOOL_NEW 	0x02		/* page needs to be written */
	u_int8_t flags;			/* flags */
} BKT;

typedef struct MPOOL {
	struct list_head lqh;	/* lru queue head */
					/* hash queue array */
	struct hlist_head hqh[HASHSIZE];
	pgno_t	curcache;		/* current number of cached pages */
	pgno_t	maxcache;		/* max number of cached pages */
	u_long	pagesize;		/* file page size */
#ifdef __KERNEL__
	struct file *file;			/* file descriptor */
#else
	int file;
#endif
					/* page in conversion routine */
} MPOOL;

#ifdef __KERNEL__
extern MPOOL	*mpool_open (const char *, struct file *, pgno_t, pgno_t);
#else
extern MPOOL	*mpool_open (const char *, int, pgno_t, pgno_t);
#endif
extern void	 mpool_filter (MPOOL *, void (*)(void *, pgno_t, void *),
	    void (*)(void *, pgno_t, void *), void *);
extern void	*mpool_new (MPOOL *, pgno_t *, off_t *, size_t);
extern void	*mpool_new_pg (MPOOL *, pgno_t *);
extern void	*mpool_get (MPOOL *, loff_t, size_t, pgno_t *, off_t *, u_int);
extern void	*mpool_get_pg (MPOOL *, pgno_t , u_int);

extern int	mpool_put (MPOOL *, void *, u_int);
extern int	mpool_free_pg (MPOOL *, void *);
extern int	mpool_sync (MPOOL *);
extern int	mpool_close (MPOOL *);
#endif
