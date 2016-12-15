#ifndef _BUDDY_H_
#define _BUDDY_H_
uint64_t buddy_alloc (size_t);
void buddy_free (uint64_t, size_t);
int	buddy_init (uint64_t);
#endif
