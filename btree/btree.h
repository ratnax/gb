#ifndef _BTREE_H_
#define _BTREE_H_
/* Macros to set/clear/test flags. */
#define	F_SET(p, f)	(p)->flags |= (f)
#define	F_CLR(p, f)	(p)->flags &= ~(f)
#define	F_ISSET(p, f)	((p)->flags & (f))

#include <mpool.h>

#define	DEFMINKEYPAGE	(2)		/* Minimum keys per page */
#define	MINCACHE	(5)		/* Minimum cached pages */
#define	MINPSIZE	(512)		/* Minimum page size */

/*
 * Page 0 of a btree file contains a copy of the meta-data.  This page is also
 * used as an out-of-band page, i.e. page pointers that point to nowhere point
 * to page 0.  Page 1 is the root of the btree.
 */
#define	P_INVALID	 0		/* Invalid tree page number. */
#define	P_META		 40		/* Tree metadata page number. */
#define	P_ROOT		 41		/* Tree root page number. */

/*
 * There are five page layouts in the btree: btree internal pages (BINTERNAL),
 * btree leaf pages (BLEAF), recno internal pages (RINTERNAL), recno leaf pages
 * (RLEAF) and overflow pages.  All five page types have a page header (PAGE).
 * This implementation requires that values within structures NOT be padded.
 * (ANSI C permits random padding.)  If your compiler pads randomly you'll have
 * to do some work to get this package to run.
 */
typedef struct _page {
	pgno_t	pgno;			/* this page's page number */

#define	P_BINTERNAL	0x01		/* btree internal page */
#define	P_BLEAF		0x02		/* leaf page */
#define P_TYPE		0x03		/* type mask */
	u_int32_t flags;

	indx_t	lower;			/* lower bound of free space on page */
	indx_t	upper;			/* upper bound of free space on page */
	indx_t	linp[1];		/* indx_t-aligned VAR. LENGTH DATA */
} PAGE;

/* First and next index. */
#define	BTDATAOFF							\
	(sizeof(pgno_t) + 						\
	    sizeof(u_int32_t) + sizeof(indx_t) + sizeof(indx_t))
#define	NEXTINDEX(p)	(((p)->lower - BTDATAOFF) / sizeof(indx_t))

/*
 * For pages other than overflow pages, there is an array of offsets into the
 * rest of the page immediately following the page header.  Each offset is to
 * an item which is unique to the type of page.  The h_lower offset is just
 * past the last filled-in index.  The h_upper offset is the first item on the
 * page.  Offsets are from the beginning of the page.
 *
 * If an item is too big to store on a single page, a flag is set and the item
 * is a { page, size } pair such that the page is the first page of an overflow
 * chain with size bytes of item.  Overflow pages are simply bytes without any
 * external structure.
 *
 * The page number and size fields in the items are pgno_t-aligned so they can
 * be manipulated without copying.  (This presumes that 32 bit items can be
 * manipulated on this system.)
 */
#define	LALIGN(n)	(((n) + sizeof(pgno_t) - 1) & ~(sizeof(pgno_t) - 1))
#define	NOVFLSIZE	(sizeof(pgno_t) + sizeof(u_int32_t))

/*
 * For the btree internal pages, the item is a key.  BINTERNALs are {key, pgno}
 * pairs, such that the key compares less than or equal to all of the records
 * on that page.  For a tree without duplicate keys, an internal page with two
 * consecutive keys, a and b, will have all records greater than or equal to a
 * and less than b stored on the page associated with a.  Duplicate keys are
 * somewhat special and can cause duplicate internal and leaf page records and
 * some minor modifications of the above rule.
 */
typedef struct _binternal {
	u_int32_t ksize;		/* key size */
	pgno_t	pgno;			/* page number stored on */
#define	P_BIGDATA	0x01		/* overflow data */
	u_char	flags;
	char	bytes[1];		/* data */
} BINTERNAL;

/* Get the page's BINTERNAL structure at index indx. */
#define	GETBINTERNAL(pg, indx)						\
	((BINTERNAL *)((char *)(pg) + (pg)->linp[indx]))

/* Get the number of bytes in the entry. */
#define NBINTERNAL(len)							\
	LALIGN(sizeof(u_int32_t) + sizeof(pgno_t) + sizeof(u_char) + (len))

/* Copy a BINTERNAL entry to the page. */
#define	WR_BINTERNAL(p, size, pgno, flags) {				\
	*(u_int32_t *)p = size;						\
	p += sizeof(u_int32_t);						\
	*(pgno_t *)p = pgno;						\
	p += sizeof(pgno_t);						\
	*(u_char *)p = flags;						\
	p += sizeof(u_char);						\
}

/* For the btree leaf pages, the item is a key and data pair. */
typedef struct _bleaf {
	u_int32_t	ksize;		/* size of key */
	u_int32_t	dsize;		/* size of data */
	u_char	flags;			/* P_BIGDATA, P_BIGKEY */
	char	bytes[1];		/* data */
} BLEAF;

/* Get the page's BLEAF structure at index indx. */
#define	GETBLEAF(pg, indx)						\
	((BLEAF *)((char *)(pg) + (pg)->linp[indx]))

/* Get the number of bytes in the entry. */
#define NBLEAF(p)	NBLEAFDBT((p)->ksize, (p)->dsize)

/* Get the number of bytes in the user's key/data pair. */
#define NBLEAFDBT(ksize, dsize)						\
	LALIGN(sizeof(u_int32_t) + sizeof(u_int32_t) + sizeof(u_char) +	\
	    (ksize) + (dsize))

/* Copy a BLEAF entry to the page. */
#define	WR_BLEAF(p, key, data, flags) {					\
	*(u_int32_t *)p = key->size;					\
	p += sizeof(u_int32_t);						\
	*(u_int32_t *)p = data->size;					\
	p += sizeof(u_int32_t);						\
	*(u_char *)p = flags;						\
	p += sizeof(u_char);						\
	memmove(p, key->data, key->size);				\
	p += key->size;							\
	memmove(p, data->data, data->size);				\
}

/*
 * A record in the tree is either a pointer to a page and an index in the page
 * or a page number and an index.  These structures are used as a cursor, stack
 * entry and search returns as well as to pass records to other routines.
 *
 * One comment about searches.  Internal page searches must find the largest
 * record less than key in the tree so that descents work.  Leaf page searches
 * must find the smallest record greater than key so that the returned index
 * is the record's correct position for insertion.
 */
typedef struct _epgno {
	pgno_t	pgno;			/* the page number */
	indx_t	index;			/* the index on the page */
#define P_LMOST		0x1
#define P_RMOST		0x2
	u_int8_t flags;
} EPGNO;

typedef struct _epg {
	PAGE	*page;			/* the (pinned) page */
	indx_t	 index;			/* the index on the page */
} EPG;

/*
 * The metadata of the tree.  The nrecs field is used only by the RECNO code.
 * This is because the btree doesn't really need it and it requires that every
 * put or delete call modify the metadata.
 */
typedef struct _btmeta {
	u_int32_t	magic;		/* magic number */
	u_int32_t	version;	/* version */
	u_int32_t	psize;		/* page size */
} BTMETA;

/* The in-memory btree/recno data structure. */
typedef struct _btree {
	MPOOL	 *bt_mp;		/* memory pool cookie */

	DB	 *bt_dbp;		/* pointer to enclosing DB */

	EPG	  bt_cur;		/* current (pinned) page */
	PAGE	 *bt_pinned;		/* page pinned across calls */
#ifdef __KERNEL__
   	struct mutex mutex;
#else
	pthread_mutex_t mutex;
#endif

#define	BT_TOP(t)	(t->bt_sp == t->bt_stack ? NULL : (t->bt_sp - 1))
#define	BT_PUSH(t, p, i, flags) {				\
	t->bt_sp->pgno = p; 						\
	t->bt_sp->index = i; 						\
	t->bt_sp->flags = flags; 					\
	++t->bt_sp;									\
}
#define	BT_POP(t)	(t->bt_sp == t->bt_stack ? NULL : --t->bt_sp)
#define	BT_CLR(t)	(t->bt_sp = t->bt_stack)
	EPGNO	  bt_stack[50];		/* stack of parent pages */
	EPGNO	 *bt_sp;		/* current stack pointer */

	DBT	  bt_rkey;		/* returned key */
	DBT	  bt_rdata;		/* returned data */

#ifdef __KERNEL__
	struct file *bt_file;		/* tree file descriptor */
#else
	int bt_file;
#endif

	u_int32_t bt_psize;		/* page size */
	indx_t	  bt_ovflsize;		/* cut-off for key/data overflow */

					/* B: key comparison function */
	int	(*bt_cmp) (const DBT *, const DBT *);
					/* B: prefix comparison function */
	size_t  (*bt_pfx) (const DBT *, const DBT *);

/*
 * NB:
 * B_NODUPS and R_RECNO are stored on disk, and may not be changed.
 */
#define	B_METADIRTY	0x00002		/* need to write metadata */
#define	B_MODIFIED	0x00004		/* tree modified */
#define	B_RDONLY	0x00010		/* read-only tree */

#define	R_CLOSEFP	0x00040		/* opened a file pointer */
#define	R_EOF		0x00100		/* end of input file reached. */
#define	R_MEMMAPPED	0x00200		/* memory mapped file. */
#define	R_INMEM		0x00400		/* in-memory file */
#define	R_MODIFIED	0x00800		/* modified file */

	u_int32_t flags;
} BTREE;
#include "extern.h"
#endif
