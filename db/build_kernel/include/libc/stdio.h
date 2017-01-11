#ifndef _LIBC_STDIO_H_
#define _LIBC_STDIO_H_
#include <linux/fs.h>

#define EOF (-1)

typedef struct file FILE;
#define fclose(file) filp_close(file, 0)

extern FILE * fopen(const char *name, const char *mode);
extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp);
extern size_t fwrite(const void *ptr, size_t size, size_t nmemb,
			                      FILE *fp);
extern int fflush(FILE *fp);
extern int fgetc(FILE *fp);
extern char *fgets(char *str, int size, FILE *stream);

#define stdin	((FILE *) 1)	
#define stdout	((FILE *) 2)
#define stderr	((FILE *) 3)
extern int rename(const char *oldname, const char *newname);
#endif
