#
# Makefile for the Linux minix filesystem routines.
#

obj-m += minix.o

minix-objs := bitmap.o itree_v1.o itree_v2.o namei.o inode.o file.o dir.o


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make -C /home/x/linux/code/linux-4.9/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
