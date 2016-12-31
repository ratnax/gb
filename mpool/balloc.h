#ifndef _BALLOC_H_
#define _BALLOC_H_

#include <mpool.h>

#define PAGE_SHFT (12)
#ifdef __KERNEL__
#else
#define PAGE_SIZE (1UL << PAGE_SHFT)
#endif

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

extern int mpool_balloc(MPOOL *mp, int order, uint64_t *out_blkno);
extern int mpool_bfree(MPOOL *mp, uint64_t blkno);
extern int mpool_alloced(MPOOL *mp, uint64_t blk);
extern int balloc_init(MPOOL *mp);
#endif
