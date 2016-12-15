#include <linux/slab.h>
#include <linux/types.h>
#include <linux/bitops.h>

#include <buddy.h>

#define TOTAL_SPACE (1024*1024*1024*4ULL)

#define MAX_UNT (1024*1024)
#define MIN_UNT	(256)

#define LEVELS	(ffs(MAX_UNIT) - ffs(MIN_UNIT))


typedef struct level_s {
	uint64_t *map;

	struct level_s *next;
	struct level_s *prev;

	int idx;
} level_t;


typedef struct buddy_s {
	int nlevels;
	uint64_t total_space;
	uint64_t *map;
	level_t *free[0];
} buddy_t;

buddy_t *b;

void 
buddy_free(block, order) 
	uint64_t block;
	size_t order;
{
	int i, idx;
	level_t *h = b->free[order - ffs(MIN_UNT) + 1]; 

	while (h) {
		if (h->idx == block / MAX_UNT) {
		
			idx = (block % MAX_UNT) / (1 << order);

			BUG_ON(!(h->map[idx >> 6] & (1ULL << (idx % 64))));

			h->map[idx >> 6] &= ~(1ULL << (idx % 64));

			for (i = 0; i < (MAX_UNT / (1 << order) + 63) >> 6; i++) {
				if (h->map[i]) 
					break;
			}
			if (i == (MAX_UNT / (1 << order) + 63) >> 6) {
				
				if (h->prev) 
					h->prev->next = h->next;
				else 
					b->free[order - ffs(MIN_UNT) + 1] = h->next;

				if (h->next) 
					h->next->prev = h->prev;

				BUG_ON(!(b->map[h->idx >> 6] & (1ULL << (h->idx % 64)))); 
				b->map[h->idx >> 6] &= ~(1ULL << (h->idx % 64)); 
				kfree(h);
			}
			return;
		}
		h = h->next;
	}
	BUG();
	return;
}


uint64_t 
buddy_alloc(order)
	size_t order;
{
	int i, idx, size;
	level_t *h = b->free[order - ffs(MIN_UNT) + 1]; 

repeat:
	while (h) {
		for (i = 0; i < (MAX_UNT / (1 << order) + 63) >> 6; i++) {
			if (h->map[i] != ~0ULL) {
				idx = __ffs(~h->map[i]);
				if (((i << 6) + idx) < MAX_UNT / (1 << order)) {
					h->map[i] |= 1ULL << idx;

					return (uint64_t) h->idx * MAX_UNT + (((i << 6) + idx) << order);
				}
			}
		}

		h = h->next;
	}

	for (i = 0; i < (b->total_space / MAX_UNT + 63) >> 6; i++) {
		if (b->map[i] != ~0ULL) {
			idx = __ffs(~b->map[i]);

			if (((i << 6) + idx) < (b->total_space / MAX_UNT)) {
				b->map[i] |= 1ULL << idx;

				size = (MAX_UNT / (1 << order) + 63) >> 6;
				h = kzalloc(sizeof(level_t) + size, GFP_KERNEL);

				h->map = (uint64_t *) (h + 1);
				h->idx = (i << 6) + idx;
				h->next = b->free[order - ffs(MIN_UNT) + 1];
				if (h->next)
					h->next->prev = h;
				b->free[order - ffs(MIN_UNT) + 1] = h;
				goto repeat;
			}
		}
	}
	return ~0ULL; 
}

#if 0

void test()
{
#define TEST_SIZE 140096
   	int i, sizes[TEST_SIZE];
	uint64_t blocks[TEST_SIZE];
	uint64_t total = 0;
	srandom(time(NULL));
	for (i = 0; i < TEST_SIZE; i++) {
		sizes[i] = (random() % b->nlevels) + ffs(MIN_UNT) - 1;
		blocks[i] = balloc(sizes[i]);
		if (blocks[i] == ~0ULL) {
			printf("ENOSPC at %ld %d, lost in frag %ld.\n", total, i, 
					b->total_space - total);
			break;
		} else {
			BUG_ON(!(blocks[i] < b->total_space));
			total += 1 << sizes[i];
		}
	}

	for (i = i - 1; i >= 0; i--) 
		bfree(blocks[i], sizes[i]);

	for (i = 0; i < b->nlevels; i++) 
		assert(b->free[i] == NULL);

	for (i = 0; i < (b->total_space / MAX_UNT + 63) >> 6; i++)
		assert(b->map[i] == 0);

	return;
}
#endif

int 
buddy_init(total_space)
	uint64_t total_space;
{
	int levels = ffs(MAX_UNT) - ffs(MIN_UNT) + 1;
	int bitmap_size;

	b = kzalloc(sizeof(buddy_t) + sizeof(level_t *) * levels, GFP_KERNEL);
	b->nlevels = levels;
	b->total_space = total_space ? : TOTAL_SPACE;


	bitmap_size	= (b->total_space / MAX_UNT + 63) >> 6;
	b->map = kzalloc(bitmap_size << 6, GFP_KERNEL);
	return 0;
}

#if 0
int main()
{
	buddy_init(TOTAL_SPACE);
	test();
}
#endif
