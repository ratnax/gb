#ifndef _DB_H_
#define	_DB_H_

#include<linux/types.h>

#define	RET_ERROR	-1		/* Return values. */
#define	RET_SUCCESS	 0
#define	RET_SPECIAL	 1

#ifndef	__BIT_TYPES_DEFINED__
#define	__BIT_TYPES_DEFINED__
typedef	__signed char		   int8_t;
typedef	unsigned char		 u_int8_t;
typedef	short			  int16_t;
typedef	unsigned short		u_int16_t;
typedef	int			  int32_t;
typedef	unsigned int		u_int32_t;
#ifdef WE_DONT_NEED_QUADS
typedef	long long		  int64_t;
typedef	unsigned long long	u_int64_t;
#endif
#endif

#define	MAX_PAGE_NUMBER	0xffffffff	/* >= # of pages in a file */
typedef u_int32_t	pgno_t;
#define	MAX_PAGE_OFFSET	65535		/* >= # of bytes in a page */
typedef u_int16_t	indx_t;
#define	MAX_REC_NUMBER	0xffffffff	/* >= # of records in a tree */
typedef u_int32_t	recno_t;

/* Key/data structure -- a Data-Base Thang. */
typedef struct {
	void	*data;			/* data */
	size_t	 size;			/* data length */
} DBT;

typedef enum { DB_BTREE} DBTYPE;


/* Access method description structure. */
typedef struct __db {
	DBTYPE type;			/* Underlying db type. */
	int (*close)	(struct __db *);
	int (*del)	(const struct __db *, const DBT *, u_int);
	int (*get)	(const struct __db *, const DBT *, DBT *, u_int);
	int (*put)	(const struct __db *, DBT *, const DBT *, u_int);
	int (*sync)	(const struct __db *, u_int);
	void *internal;			/* Access method private. */
} DB;

#define	BTREEMAGIC	0x053162
#define	BTREEVERSION	3

/* Structure used to pass parameters to the btree routines. */
typedef struct {
#define	R_DUP		0x01	/* duplicate keys */
	u_long	flags;
	u_int	cachesize;	/* bytes to cache */
	int	maxkeypage;	/* maximum keys per page */
	int	minkeypage;	/* minimum keys per page */
	u_int	psize;		/* page size */
	int	(*compare)	/* comparison function */
	    (const DBT *, const DBT *);
	size_t	(*prefix)	/* prefix function */
	    (const DBT *, const DBT *);
} BTREEINFO;

#define	HASHMAGIC	0x061561
#define	HASHVERSION	2

/* Structure used to pass parameters to the hashing routines. */
typedef struct {
	u_int	bsize;		/* bucket size */
	u_int	ffactor;	/* fill factor */
	u_int	nelem;		/* number of elements */
	u_int	cachesize;	/* bytes to cache */
	u_int32_t		/* hash function */
		(*hash) (const void *, size_t);
	int	lorder;		/* byte order */
} HASHINFO;

/* Structure used to pass parameters to the record routines. */
typedef struct {
#define	R_FIXEDLEN	0x01	/* fixed-length records */
#define	R_NOKEY		0x02	/* key not required */
#define	R_SNAPSHOT	0x04	/* snapshot the input */
	u_long	flags;
	u_int	cachesize;	/* bytes to cache */
	u_int	psize;		/* page size */
	int	lorder;		/* byte order */
	size_t	reclen;		/* record length (fixed-length records) */
	u_char	bval;		/* delimiting byte (variable-length records */
	char	*bfname;	/* btree file name */ 
} RECNOINFO;

DB	*dbopen (const char *, int, int, DBTYPE, const void *);
DB	*__bt_open (const char *, int, int, const BTREEINFO *, int);
#endif /* !_DB_H_ */
