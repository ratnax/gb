#define PAGE_SHFT (12)
#ifdef __KERNEL__
#else
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "find_bit.c"
#define PAGE_SIZE (1UL << PAGE_SHFT)
#endif

#include <db.h>
#include <mpool.h>
#include "balloc.h"

uint64_t *maps[BLK_UNIT_MAX];
int fd;

int mpool_balloc(MPOOL *mp, int order, uint64_t *out_blk)
{
	unsigned long unit, pno, bit;
	blk_unit_page_t *p;
	blk_unit_t *bu;
	uint64_t *map = maps[order - MIN_UNIT_SHFT]; 
	
	unit = find_next_bit((unsigned long*) map, TOTAL_UNITS, 0);

	if (unit < TOTAL_UNITS) {

		pno = unit / 28;
   	
		p = (blk_unit_page_t *) mpool_get_pg(mp, pno, 0);
		if (!p)
			return -EIO;

		bu = &p->blk_units[unit % 28];

		bit = find_next_zero_bit((unsigned long *) bu->map, bu->nmax, 0); 
		if (--bu->nfree == 0)
			clear_bit(unit, (unsigned long *) map);
	} else {
		unit = find_next_bit((unsigned long *) maps[BLK_UNIT_4MB], 
							TOTAL_UNITS, 0);

		if (unit >= TOTAL_UNITS)
			return -ENOSPC;

		pno = unit / 28;

		p = mpool_get_pg(mp, pno, 0);
		if (!p) 
			return -EIO;
		
		bu = &p->blk_units[unit % 28];


		bu->type = order - MIN_UNIT_SHFT;
		bu->nmax = bu->nfree = 1UL << (MAX_UNIT_SHFT - order);
		bu->nfree--;

		set_bit(unit, (unsigned long *) map);
		clear_bit(unit, (unsigned long *) maps[BLK_UNIT_4MB]);
		bit = 0;
	}

	set_bit(bit, (unsigned long *)bu->map);

	mpool_put(mp, p, MPOOL_DIRTY);
	*out_blk = (unit << MAX_UNIT_SHFT) + (bit << order);
	return 0;
}

int mpool_bfree(MPOOL *mp, uint64_t blk)
{
	unsigned long unit = blk >> MAX_UNIT_SHFT;
	unsigned long pno = unit / 28;
	blk_unit_page_t *p;
	blk_unit_t *bu;

	p = (blk_unit_page_t *) mpool_get_pg(mp, pno, 0);
	if (!p)
		return -EIO;
	
   	bu = &p->blk_units[unit % 28];

	clear_bit((blk & MAX_UNIT_MASK) >> (bu->type + MIN_UNIT_SHFT), 
			(unsigned long *) bu->map);
	bu->nfree++;

	if (bu->nfree == bu->nmax) {
		clear_bit(unit, (unsigned long *) maps[bu->type]);
		bu->type = BLK_UNIT_4MB;
		bu->nfree = bu->nmax = 1;
		set_bit(unit, (unsigned long *) maps[bu->type]);
	}

	mpool_put(mp, (void *) p, MPOOL_DIRTY);
	return 0;
}

int balloc_init(MPOOL *mp)
{
	int i, j;
	blk_unit_page_t *p= NULL;

	p = kzlloc(sizeof(blk_unit_page_t), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	for (i = 0; i < BLK_UNIT_MAX; i++) {
		maps[i] = kzalloc(TOTAL_UNITS >> 3, GFP_KERNEL);
		if (!maps[i])
			return -ENOMEM;
	}

	for (i = 0; i < (TOTAL_UNITS + 27) / 28; i++) {
		
		p = mpool_get_pg(mp, 2 + i, 0);
		if (!p)
			return -EIO;

		for (j = 0; j < 28; j++) {
			if (p->blk_units[j].nfree > 0) 
				set_bit(i * 28 + j, maps[p->blk_units[j].type]);
		}
	} 

	return 0;
}


#if 0
#include <time.h>

void test(void)
{
#define TEST_SIZE 50000
   	int i, j, ret, k = 4;
	int sizes[TEST_SIZE];
	uint64_t blocks[TEST_SIZE];
	uint64_t total = 0;
	blk_unit_page_t p;

	srandom(time(NULL));
	while (k--) {
	for (i = 0; i < TEST_SIZE; i++) {
		sizes[i] = (random() % 10) + MIN_UNIT_SHFT;

		ret = balloc(sizes[i], &blocks[i]);
		if (ret) {
			printf("ENOSPC at %ld %d, lost in frag %llu.\n", total, i, 
					TOTAL_SPACE - total);
			sizes[i] = 0;
			if (TOTAL_SPACE - total == 0) 
				break;
		} else {
			total += 1 << sizes[i];
		}
	}

	for (i = i - 1; i >= 0; i--) 
		if (sizes[i])
			bfree(blocks[i]);

	for (i = 0; i < (TOTAL_UNITS + 27) / 28; i++) {
		
		assert(pread(fd, &p, PAGE_SIZE, i << PAGE_SHFT) == PAGE_SIZE);

		for (j = 0; j < 28; j++) {
			assert(p.blk_units[j].type == BLK_UNIT_4MB);
			assert(p.blk_units[j].nfree == 1);
			assert(p.blk_units[j].nmax == 1);
		}
	} 
	printf("allocated: %lu\n", total);
	total = 0;
	}

	return;
}

int main()
{
	int i, j;
	blk_unit_page_t *p= NULL;

	fd = open("/home/x/maps", O_RDWR|O_CREAT, 0755);
	if (fd <=0) {
		fprintf(stderr, "Open failed (%s)\n", strerror(errno));
		return -errno;
	}


	if (mkb()) {
		fprintf(stderr, "Create failed\n");
		return -EINVAL;
	}

	p = calloc(1, sizeof(blk_unit_page_t));
	if (!p)
		return -ENOMEM;

	for (i = 0; i < BLK_UNIT_MAX; i++) {
		maps[i] = calloc(1, TOTAL_UNITS >> 3);
		if (!maps[i])
			return -ENOMEM;
	}

	for (i = 0; i < (TOTAL_UNITS + 27) / 28; i++) {
		
		if (pread(fd, p, PAGE_SIZE, i << PAGE_SHFT) != PAGE_SIZE)
			return -EIO;

		for (j = 0; j < 28; j++) {
			if (p->blk_units[j].nfree > 0) 
				set_bit(i * 28 + j, maps[p->blk_units[j].type]);
		}
	} 

	test();
	return 0;
}
#endif
