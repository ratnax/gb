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

#define PAGE_SHFT (12)
#define PAGE_SIZE (1UL << PAGE_SHFT)

#define MIN_UNIT_SHFT	(PAGE_SHFT)
#define MIN_UNIT_SIZE	(1UL << MIN_UNIT_SHFT)

#define MAX_UNIT_SHFT	(22)
#define MAX_UNIT_SIZE	(1UL << MAX_UNIT_SHFT)
#define MAX_UNIT_MASK	(MAX_UNIT_SIZE - 1)

#define TOTAL_SPACE (4*1024*1024*1024ULL)
#define TOTAL_UNITS (TOTAL_SPACE / MAX_UNIT_SIZE) 

typedef enum blk_unit_type_t {
	BLK_UNIT_4KB,
	BLK_UNIT_8KB,
	BLK_UNIT_16KB,
	BLK_UNIT_32KB,
	BLK_UNIT_64KB,
	BLK_UNIT_128KB,
	BLK_UNIT_256KB,
	BLK_UNIT_512KB,
	BLK_UNIT_1MB,
	BLK_UNIT_2MB,
	BLK_UNIT_4MB,
	BLK_UNIT_MAX
} blk_unit_type_t;

typedef struct blk_unit_t {
	uint32_t type;
	uint32_t nmax;
	uint32_t nfree;
	uint32_t rsvd;
	uint64_t map[1 << (MAX_UNIT_SHFT - MIN_UNIT_SHFT - 6)];
} blk_unit_t;

typedef union blk_unit_page_t {
	blk_unit_t blk_units[28];
	uint8_t page[PAGE_SIZE];
} blk_unit_page_t;

uint64_t *maps[BLK_UNIT_MAX];
int fd;

int balloc(int order, uint64_t *out_blk)
{
	unsigned long unit, pno, bit;
	blk_unit_page_t p;
	blk_unit_t *bu;
	uint64_t *map = maps[order - MIN_UNIT_SHFT]; 
	
	unit = find_next_bit(map, TOTAL_UNITS, 0);

	if (unit < TOTAL_UNITS) {

		pno = unit / 28;
   	
		bu = &p.blk_units[unit % 28];
	   	if (PAGE_SIZE != pread(fd, &p, PAGE_SIZE,  pno << PAGE_SHFT))
			return -EIO;

		assert(bu->type == (order - MIN_UNIT_SHFT)); 

		bit = find_next_zero_bit(bu->map, bu->nmax, 0); 
		assert(bit < bu->nmax);
		if (--bu->nfree == 0)
			clear_bit(unit, map);
	} else {
		unit = find_next_bit(maps[BLK_UNIT_4MB], TOTAL_UNITS, 0);

		if (unit >= TOTAL_UNITS)
			return -ENOSPC;

		pno = unit / 28;
		bu = &p.blk_units[unit % 28];

		if (PAGE_SIZE != pread(fd, &p, PAGE_SIZE, pno << PAGE_SHFT))
			return -EIO;

		assert(bu->type == BLK_UNIT_4MB);

		bu->type = order - MIN_UNIT_SHFT;
		assert(bu->type < BLK_UNIT_MAX);
		bu->nmax = bu->nfree = 1UL << (MAX_UNIT_SHFT - order);
		bu->nfree--;

		set_bit(unit, map);
		clear_bit(unit, maps[BLK_UNIT_4MB]);
		bit = 0;
	}

	set_bit(bit, bu->map);

	if (PAGE_SIZE != pwrite(fd, &p, PAGE_SIZE, pno << PAGE_SHFT))
		return -EIO;

	*out_blk = (unit << MAX_UNIT_SHFT) + (bit << order);
	return 0;
}

int bfree(uint64_t blk)
{
	unsigned long unit = blk >> MAX_UNIT_SHFT;
	unsigned long pno = unit / 28;
	blk_unit_page_t p;
	blk_unit_t *bu = &p.blk_units[unit % 28];

	if (PAGE_SIZE != pread(fd, &p, PAGE_SIZE, pno << PAGE_SHFT))
		return -EIO;
	
	assert(bu->type < BLK_UNIT_MAX);
	clear_bit((blk & MAX_UNIT_MASK)  >> (bu->type + MIN_UNIT_SHFT), bu->map);
	bu->nfree++;

	if (bu->nfree == bu->nmax) {
		clear_bit(unit, maps[bu->type]);
		bu->type = BLK_UNIT_4MB;
		set_bit(unit, maps[bu->type]);
	}

	if (PAGE_SIZE != pwrite(fd, &p, PAGE_SIZE, pno << PAGE_SHFT))
		return -EIO;
	return 0;
}


#include <string.h>

int mkb(void)
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
		if (PAGE_SIZE != pwrite(fd, &p, PAGE_SIZE, i << PAGE_SHFT))
			return -EIO;	
	}
	return 0;
}

#include <time.h>

void test(void)
{
#define TEST_SIZE 5000
   	int i, ret;
	int sizes[TEST_SIZE];
	uint64_t blocks[TEST_SIZE];
	uint64_t total = 0;

	srandom(time(NULL));
	for (i = 0; i < TEST_SIZE; i++) {
		sizes[i] = (random() % 10) + MIN_UNIT_SHFT;

		ret = balloc(sizes[i], &blocks[i]);
		if (ret) {
			printf("ENOSPC at %ld %d, lost in frag %llu.\n", total, i, 
					TOTAL_SPACE - total);
			break;
		} else {
			total += 1 << sizes[i];
		}
	}

	for (i = i - 1; i >= 0; i--) 
		bfree(blocks[i]);

	printf("allocated: %lu\n", total);
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
