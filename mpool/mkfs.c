#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "find_bit.c"

#define PAGE_SHFT (12)
#define PAGE_SIZE (1UL << PAGE_SHFT)


#include "balloc.h"

int mkballoc(int fd)
{
	int i;
	blk_unit_page_t p;

	for (i = 0; i < 28; i++) {
		p.blk_units[i].type = BLK_UNIT_4MB;
		p.blk_units[i].nmax = p.blk_units[i].nfree = 1;
		memset(p.blk_units[i].map, 0, sizeof(p.blk_units[i].map));
	}

	printf("total units: %llu\n", TOTAL_UNITS);
	for (i = 0; i < (TOTAL_UNITS + 27) / 28; i++) {
		if (PAGE_SIZE != pwrite(fd, &p, PAGE_SIZE, (i + 2) << PAGE_SHFT))
			return -EIO;	
	}

	p.blk_units[0].type = BLK_UNIT_4KB;
	p.blk_units[0].nfree = MAX_UNIT_SIZE / 4096;
	p.blk_units[0].nmax = p.blk_units[0].nfree;

	for (i = 0; i < 40; i++) {
		set_bit(i, (unsigned long *) p.blk_units[0].map);
		p.blk_units[0].nfree--;
	}
		
	if (PAGE_SIZE != pwrite(fd, &p, PAGE_SIZE, 2 << PAGE_SHFT)) 
		return -EIO;

	if (fsync(fd)) return -EIO;
	return 0;
}

int main(int argc, char **argv)
{
	int fd, ret;

	fd = open(argv[1], O_RDWR|O_CREAT, 0755);
	if (fd <=0) {
		fprintf(stderr, "Open failed (%s)\n", strerror(errno));
		return -errno;
	}

	ret = mkballoc(fd);
	close(fd);
	return ret;
}
