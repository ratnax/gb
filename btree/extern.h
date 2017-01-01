#ifndef _EXTERN_H_
#define _EXTERN_H_
DB * __bt_open(const char *, int, int, const BTREEINFO *, int);
int	 __bt_close (DB *);
int	 __bt_cmp (BTREE *, const DBT *, EPG *);
int  __bt_isleftmost (BTREE *);
int  __bt_isrightmost(BTREE *);
int	 __bt_defcmp (const DBT *, const DBT *);
size_t	 __bt_defpfx (const DBT *, const DBT *);
int	 __bt_delete (const DB *, const DBT *, u_int);
int	 __bt_dleaf (BTREE *, const DBT *, PAGE *, u_int);
int	 __bt_fd (const DB *);
int	 __bt_free (BTREE *, PAGE *);
int	 __bt_get (const DB *, const DBT *, DBT *, u_int);
PAGE	*__bt_new (BTREE *, pgno_t *);
void 	*__bt_data_new (BTREE *, pgno_t *, size_t);
int	__bt_data_free(BTREE *, void *);
void	 __bt_pgin (void *, pgno_t, void *);
void	 __bt_pgout (void *, pgno_t, void *);
int	 __bt_push (BTREE *, pgno_t, int);
int	 __bt_put (const DB *dbp, DBT *, const DBT *, u_int);
int	 __bt_ret (BTREE *, EPG *, DBT *, DBT *, DBT *, DBT *, int);
EPG	*__bt_search (BTREE *, const DBT *, int *);
int	 __bt_split (BTREE *, PAGE *,
	    const DBT *, const DBT *, int, size_t, u_int32_t);
int	 __bt_sync (const DB *, u_int);

int	 __ovfl_delete (BTREE *, void *);
int	 __ovfl_get (BTREE *, void *, size_t *, void **, size_t *);
int	 __ovfl_put (BTREE *, const DBT *, pgno_t *);
#endif
