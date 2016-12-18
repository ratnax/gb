#
# Makefile for the linux ext2-filesystem routines.
#

obj-m := gbfs.o

gbfs-y :=	fs/minix/bitmap.o \
			fs/minix/itree_v1.o \
			fs/minix/itree_v2.o \
			fs/minix/namei.o \
			fs/minix/inode.o \
			fs/minix/file.o \
			fs/minix/dir.o

gbfs-y +=	btree/bt_close.o \
			btree/bt_delete.o \
			btree/bt_get.o \
			btree/bt_open.o \
			btree/bt_overflow.o \
			btree/bt_page.o \
			btree/bt_put.o \
			btree/bt_search.o \
			btree/bt_split.o \
			btree/bt_utils.o

gbfs-y +=	db/db.o
gbfs-y +=	mpool/mpool.o mpool/buddy.o

ccflags-y := -Wfatal-errors -I$(src)/include

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make -C /home/x/linux/code/linux-4.9/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
