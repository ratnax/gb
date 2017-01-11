#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_
#include <linux/bsearch.h>

extern void *malloc(size_t size);
extern void free(void *ptr);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);

extern int rand(void);
extern void srand(unsigned int seed);
extern char *getenv(const char *name);      
extern void abort(void);
extern void exit(int status);
extern void qsort(void *base, size_t nmemb, size_t size,
				int (*compar)(const void *, const void *));

#endif
