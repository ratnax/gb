#
# Makefile for the linux ext2-filesystem routines.
#

obj-m := kbdb.o gbfs.o

gbfs-y :=	fs/bitmap.o \
			fs/namei.o \
			fs/inode.o \
			fs/file.o \
			fs/dir.o

kbdb-y := 	./db/build_kernel/kbdb_mod.o

kbdb-y += 	./db/src/os/os_abort.o 	\
 			./db/src/os/os_abs.o 		\
 			./db/src/os/os_alloc.o 	\
 			./db/src/os/os_clock.o 	\
 			./db/src/os/os_cpu.o 		\
 			./db/src/os/os_ctime.o 	\
 			./db/src/os/os_config.o 	\
 			./db/src/os/os_dir.o 		\
 			./db/src/os/os_errno.o 	\
 			./db/src/os/os_fid.o 		\
 			./db/src/os/os_flock.o 	\
 			./db/src/os/os_fsync.o 	\
 			./db/src/os/os_getenv.o 	\
 			./db/src/os/os_handle.o 	\
 			./db/src/os/os_map.o		\
 			./db/src/os/os_mkdir.o 	\
 			./db/src/os/os_open.o 	\
 			./db/src/os/os_path.o 	\
 			./db/src/os/os_pid.o 		\
 			./db/src/os/os_rename.o 	\
 			./db/src/os/os_rmdir.o 	\
 			./db/src/os/os_root.o 	\
 			./db/src/os/os_rpath.o 	\
 			./db/src/os/os_rw.o 		\
 			./db/src/os/os_seek.o 	\
 			./db/src/os/os_stack.o 	\
 			./db/src/os/os_stat.o 	\
 			./db/src/os/os_tmpdir.o 	\
 			./db/src/os/os_truncate.o \
 			./db/src/os/os_uid.o 		\
 			./db/src/os/os_unlink.o 	\
 			./db/src/os/os_yield.o


kbdb-y += 	./db/src/mutex/mut_tas.o		\
			./db/src/mutex/mut_pthread.o	\
			./db/src/mutex/mut_alloc.o 	\
			./db/src/mutex/mut_failchk.o 	\
			./db/src/mutex/mut_method.o 	\
			./db/src/mutex/mut_region.o 	\
			./db/src/mutex/mut_stat.o 	

kbdb-y +=	./db/src/btree/bt_compare.o 	\
			./db/src/btree/bt_compress.o 	\
			./db/src/btree/bt_conv.o 		\
			./db/src/btree/bt_curadj.o	\
			./db/src/btree/bt_cursor.o	\
			./db/src/btree/bt_delete.o	\
			./db/src/btree/bt_method.o	\
			./db/src/btree/bt_open.o		\
			./db/src/btree/bt_put.o		\
			./db/src/btree/bt_rec.o		\
			./db/src/btree/bt_reclaim.o	\
			./db/src/btree/bt_recno.o		\
			./db/src/btree/bt_rsearch.o	\
			./db/src/btree/bt_search.o	\
			./db/src/btree/bt_split.o		\
			./db/src/btree/bt_stat.o		\
			./db/src/btree/bt_compact.o	\
			./db/src/btree/bt_upgrade.o	\
			./db/src/btree/btree_auto.o	


kbdb-y += 	./db/src/hash/hash_stub.o 	\
 			./db/src/hash/hash_func.o 	

kbdb-y +=	./db/src/heap/heap_stub.o 	
kbdb-y +=	./db/src/qam/qam_stub.o
kbdb-y +=	./db/src/rep/rep_stub.o
kbdb-y +=	./db/src/repmgr/repmgr_stub.o
kbdb-y +=	./db/src/db/db_vrfy_stub.o
kbdb-y +=	./db/src/log/log_verify_stub.o

kbdb-y +=	./db/src/lock/lock.o 			\
			./db/src/lock/lock_deadlock.o \
			./db/src/lock/lock_failchk.o 	\
			./db/src/lock/lock_id.o 		\
			./db/src/lock/lock_list.o 	\
			./db/src/lock/lock_method.o 	\
			./db/src/lock/lock_region.o 	\
			./db/src/lock/lock_stat.o 	\
			./db/src/lock/lock_timer.o 	\
			./db/src/lock/lock_util.o 	

kbdb-y +=	./db/src/crypto/aes_method.o 					\
			./db/src/crypto/crypto.o 						\
			./db/src/crypto/mersenne/mt19937db.o 			\
			./db/src/crypto/rijndael/rijndael-alg-fst.o 	\
			./db/src/crypto/rijndael/rijndael-api-fst.o 	


kbdb-y +=	./db/src/blob/blob_fileops.o 	\
			./db/src/blob/blob_page.o 	\
			./db/src/blob/blob_stream.o 	\
			./db/src/blob/blob_util.o 	

kbdb-y +=	./db/src/common/clock.o			\
			./db/src/common/db_byteorder.o 	\
			./db/src/common/db_shash.o 		\
			./db/src/common/db_compint.o 		\
			./db/src/common/db_err.o 			\
			./db/src/common/db_getlong.o 		\
			./db/src/common/db_idspace.o 		\
			./db/src/common/db_log2.o 		\
			./db/src/common/dbt.o 			\
 			./db/src/common/mkpath.o			\
 			./db/src/common/openflags.o		\
 			./db/src/common/os_method.o		\
 			./db/src/common/zerofill.o


kbdb-y += 	./db/src/db/crdel_auto.o 			\
			./db/src/db/crdel_rec.o 			\
			./db/src/db/db.o 					\
			./db/src/db/db_am.o 				\
			./db/src/db/db_auto.o 			\
			./db/src/db/db_backup.o 			\
			./db/src/db/db_cam.o 				\
			./db/src/db/db_cds.o 				\
			./db/src/db/db_compact.o 			\
			./db/src/db/db_conv.o 			\
			./db/src/db/db_copy.o 			\
			./db/src/db/db_dispatch.o 		\
			./db/src/db/db_dup.o 				\
			./db/src/db/db_iface.o 			\
			./db/src/db/db_join.o 			\
			./db/src/db/db_meta.o 			\
			./db/src/db/db_method.o 			\
			./db/src/db/db_open.o 			\
			./db/src/db/db_overflow.o 		\
			./db/src/db/db_pr.o 				\
			./db/src/db/db_rec.o 				\
			./db/src/db/db_reclaim.o 			\
			./db/src/db/db_remove.o 			\
			./db/src/db/db_rename.o 			\
			./db/src/db/db_ret.o 				\
			./db/src/db/db_setid.o 			\
			./db/src/db/db_setlsn.o 			\
			./db/src/db/db_slice.o 			\
			./db/src/db/db_sort_multiple.o 	\
			./db/src/db/db_stati.o 			\
			./db/src/db/db_truncate.o 		\
			./db/src/db/db_upg.o 				\
			./db/src/db/db_upg_opd.o 			\
 			./db/src/db/partition.o

kbdb-y += 	./db/src/dbreg/dbreg.o 		\
			./db/src/dbreg/dbreg_stat.o 	\
			./db/src/dbreg/dbreg_auto.o 	\
			./db/src/dbreg/dbreg_rec.o 	\
			./db/src/dbreg/dbreg_util.o 	

kbdb-y += 	./db/src/env/env_alloc.o 		\
			./db/src/env/env_config.o 	\
			./db/src/env/env_backup.o 	\
			./db/src/env/env_failchk.o 	\
			./db/src/env/env_file.o 		\
			./db/src/env/env_globals.o 	\
			./db/src/env/env_open.o 		\
			./db/src/env/env_method.o 	\
			./db/src/env/env_name.o 		\
			./db/src/env/env_recover.o 	\
			./db/src/env/env_region.o 	\
			./db/src/env/env_register.o 	\
			./db/src/env/env_sig.o 		\
			./db/src/env/env_slice.o 		\
			./db/src/env/env_stat.o 

kbdb-y += 	./db/src/fileops/fileops_auto.o 	\
 			./db/src/fileops/fop_basic.o 		\
 			./db/src/fileops/fop_rec.o 		\
 			./db/src/fileops/fop_util.o 		

kbdb-y += 	./db/src/hmac/hmac.o	\
 			./db/src/hmac/sha1.o

kbdb-y += 	./db/src/log/log.o 			\
 			./db/src/log/log_archive.o	\
 			./db/src/log/log_compare.o 	\
 			./db/src/log/log_debug.o 		\
 			./db/src/log/log_get.o 		\
 			./db/src/log/log_method.o 	\
 			./db/src/log/log_print.o 		\
 			./db/src/log/log_put.o 		\
 			./db/src/log/log_stat.o


kbdb-y +=	./db/src/mp/mp_alloc.o 	\
 			./db/src/mp/mp_backup.o 	\
 			./db/src/mp/mp_bh.o 		\
 			./db/src/mp/mp_fget.o 	\
 			./db/src/mp/mp_fmethod.o 	\
 			./db/src/mp/mp_fopen.o 	\
 			./db/src/mp/mp_fput.o 	\
 			./db/src/mp/mp_fset.o 	\
 			./db/src/mp/mp_method.o 	\
 			./db/src/mp/mp_mvcc.o 	\
 			./db/src/mp/mp_region.o 	\
 			./db/src/mp/mp_register.o \
 			./db/src/mp/mp_resize.o 	\
 			./db/src/mp/mp_stat.o 	\
 			./db/src/mp/mp_sync.o 	\
 			./db/src/mp/mp_trickle.o

kbdb-y += 	./db/src/sequence/seq_stat.o 	\
 			./db/src/sequence/sequence.o

kbdb-y +=	./db/src/clib/snprintf.o \
			./db/src/clib/atol.o \
			./db/src/clib/atoi.o \
			./db/src/clib/strtoul.o \
			./db/src/clib/printf.o \
			./db/src/clib/strsep.o \
			./db/src/clib/isprint.o \
			./db/src/clib/strncmp.o \
			./db/src/clib/isspace.o \
			./db/src/clib/strtol.o \
			./db/src/clib/isdigit.o

kbdb-y +=	./db/src/txn/txn.o 			\
 			./db/src/txn/txn_auto.o 		\
 			./db/src/txn/txn_chkpt.o 		\
 			./db/src/txn/txn_failchk.o 	\
 			./db/src/txn/txn_method.o 	\
 			./db/src/txn/txn_rec.o 		\
 			./db/src/txn/txn_recover.o 	\
 			./db/src/txn/txn_region.o 	\
 			./db/src/txn/txn_stat.o 		\
 			./db/src/txn/txn_util.o


kbdb-y += 	./db/src/xa/xa.o 		\
 			./db/src/xa/xa_map.o

kbdb-y +=	./db/build_kernel/libk.o \
			./db/build_kernel/syscalls.o

ccflags-y := -Wfatal-errors -I$(src)/db/build_kernel/include/ \
				-I$(src)/db/build_kernel/include/libc/ \
				-I$(src)/db/src/  -D_GNU_SOURCE -O3

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make -C /home/x/linux/code/linux-4.9/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	@find . \( -name '*.[oa]' -o -name '*.ko' -o -name '.*.cmd' \) -type f -print | xargs rm -f

