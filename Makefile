#
# Makefile for the linux ext2-filesystem routines.
#

obj-m := kbdb.o gbfs.o

gbfs-y :=	fs/bitmap.o \
			fs/namei.o \
			fs/inode.o \
			fs/file.o \
			fs/dir.o

kbdb-y := 	./db-6.2.23/build_kernel/kbdb_mod.o

kbdb-y += 	./db-6.2.23/src/os/os_abort.o 	\
 			./db-6.2.23/src/os/os_abs.o 		\
 			./db-6.2.23/src/os/os_alloc.o 	\
 			./db-6.2.23/src/os/os_clock.o 	\
 			./db-6.2.23/src/os/os_cpu.o 		\
 			./db-6.2.23/src/os/os_ctime.o 	\
 			./db-6.2.23/src/os/os_config.o 	\
 			./db-6.2.23/src/os/os_dir.o 		\
 			./db-6.2.23/src/os/os_errno.o 	\
 			./db-6.2.23/src/os/os_fid.o 		\
 			./db-6.2.23/src/os/os_flock.o 	\
 			./db-6.2.23/src/os/os_fsync.o 	\
 			./db-6.2.23/src/os/os_getenv.o 	\
 			./db-6.2.23/src/os/os_handle.o 	\
 			./db-6.2.23/src/os/os_map.o		\
 			./db-6.2.23/src/os/os_mkdir.o 	\
 			./db-6.2.23/src/os/os_open.o 	\
 			./db-6.2.23/src/os/os_path.o 	\
 			./db-6.2.23/src/os/os_pid.o 		\
 			./db-6.2.23/src/os/os_rename.o 	\
 			./db-6.2.23/src/os/os_rmdir.o 	\
 			./db-6.2.23/src/os/os_root.o 	\
 			./db-6.2.23/src/os/os_rpath.o 	\
 			./db-6.2.23/src/os/os_rw.o 		\
 			./db-6.2.23/src/os/os_seek.o 	\
 			./db-6.2.23/src/os/os_stack.o 	\
 			./db-6.2.23/src/os/os_stat.o 	\
 			./db-6.2.23/src/os/os_tmpdir.o 	\
 			./db-6.2.23/src/os/os_truncate.o \
 			./db-6.2.23/src/os/os_uid.o 		\
 			./db-6.2.23/src/os/os_unlink.o 	\
 			./db-6.2.23/src/os/os_yield.o


kbdb-y += 	./db-6.2.23/src/mutex/mut_tas.o		\
			./db-6.2.23/src/mutex/mut_pthread.o	\
			./db-6.2.23/src/mutex/mut_alloc.o 	\
			./db-6.2.23/src/mutex/mut_failchk.o 	\
			./db-6.2.23/src/mutex/mut_method.o 	\
			./db-6.2.23/src/mutex/mut_region.o 	\
			./db-6.2.23/src/mutex/mut_stat.o 	

kbdb-y +=	./db-6.2.23/src/btree/bt_compare.o 	\
			./db-6.2.23/src/btree/bt_compress.o 	\
			./db-6.2.23/src/btree/bt_conv.o 		\
			./db-6.2.23/src/btree/bt_curadj.o	\
			./db-6.2.23/src/btree/bt_cursor.o	\
			./db-6.2.23/src/btree/bt_delete.o	\
			./db-6.2.23/src/btree/bt_method.o	\
			./db-6.2.23/src/btree/bt_open.o		\
			./db-6.2.23/src/btree/bt_put.o		\
			./db-6.2.23/src/btree/bt_rec.o		\
			./db-6.2.23/src/btree/bt_reclaim.o	\
			./db-6.2.23/src/btree/bt_recno.o		\
			./db-6.2.23/src/btree/bt_rsearch.o	\
			./db-6.2.23/src/btree/bt_search.o	\
			./db-6.2.23/src/btree/bt_split.o		\
			./db-6.2.23/src/btree/bt_stat.o		\
			./db-6.2.23/src/btree/bt_compact.o	\
			./db-6.2.23/src/btree/bt_upgrade.o	\
			./db-6.2.23/src/btree/btree_auto.o	


kbdb-y += 	./db-6.2.23/src/hash/hash_stub.o 	\
 			./db-6.2.23/src/hash/hash_func.o 	

kbdb-y +=	./db-6.2.23/src/heap/heap_stub.o 	
kbdb-y +=	./db-6.2.23/src/qam/qam_stub.o
kbdb-y +=	./db-6.2.23/src/rep/rep_stub.o
kbdb-y +=	./db-6.2.23/src/repmgr/repmgr_stub.o
kbdb-y +=	./db-6.2.23/src/db/db_vrfy_stub.o
kbdb-y +=	./db-6.2.23/src/log/log_verify_stub.o

kbdb-y +=	./db-6.2.23/src/lock/lock.o 			\
			./db-6.2.23/src/lock/lock_deadlock.o \
			./db-6.2.23/src/lock/lock_failchk.o 	\
			./db-6.2.23/src/lock/lock_id.o 		\
			./db-6.2.23/src/lock/lock_list.o 	\
			./db-6.2.23/src/lock/lock_method.o 	\
			./db-6.2.23/src/lock/lock_region.o 	\
			./db-6.2.23/src/lock/lock_stat.o 	\
			./db-6.2.23/src/lock/lock_timer.o 	\
			./db-6.2.23/src/lock/lock_util.o 	

kbdb-y +=	./db-6.2.23/src/crypto/aes_method.o 					\
			./db-6.2.23/src/crypto/crypto.o 						\
			./db-6.2.23/src/crypto/mersenne/mt19937db.o 			\
			./db-6.2.23/src/crypto/rijndael/rijndael-alg-fst.o 	\
			./db-6.2.23/src/crypto/rijndael/rijndael-api-fst.o 	


kbdb-y +=	./db-6.2.23/src/blob/blob_fileops.o 	\
			./db-6.2.23/src/blob/blob_page.o 	\
			./db-6.2.23/src/blob/blob_stream.o 	\
			./db-6.2.23/src/blob/blob_util.o 	

kbdb-y +=	./db-6.2.23/src/common/clock.o			\
			./db-6.2.23/src/common/db_byteorder.o 	\
			./db-6.2.23/src/common/db_shash.o 		\
			./db-6.2.23/src/common/db_compint.o 		\
			./db-6.2.23/src/common/db_err.o 			\
			./db-6.2.23/src/common/db_getlong.o 		\
			./db-6.2.23/src/common/db_idspace.o 		\
			./db-6.2.23/src/common/db_log2.o 		\
			./db-6.2.23/src/common/dbt.o 			\
 			./db-6.2.23/src/common/mkpath.o			\
 			./db-6.2.23/src/common/openflags.o		\
 			./db-6.2.23/src/common/os_method.o		\
 			./db-6.2.23/src/common/zerofill.o


kbdb-y += 	./db-6.2.23/src/db/crdel_auto.o 			\
			./db-6.2.23/src/db/crdel_rec.o 			\
			./db-6.2.23/src/db/db.o 					\
			./db-6.2.23/src/db/db_am.o 				\
			./db-6.2.23/src/db/db_auto.o 			\
			./db-6.2.23/src/db/db_backup.o 			\
			./db-6.2.23/src/db/db_cam.o 				\
			./db-6.2.23/src/db/db_cds.o 				\
			./db-6.2.23/src/db/db_compact.o 			\
			./db-6.2.23/src/db/db_conv.o 			\
			./db-6.2.23/src/db/db_copy.o 			\
			./db-6.2.23/src/db/db_dispatch.o 		\
			./db-6.2.23/src/db/db_dup.o 				\
			./db-6.2.23/src/db/db_iface.o 			\
			./db-6.2.23/src/db/db_join.o 			\
			./db-6.2.23/src/db/db_meta.o 			\
			./db-6.2.23/src/db/db_method.o 			\
			./db-6.2.23/src/db/db_open.o 			\
			./db-6.2.23/src/db/db_overflow.o 		\
			./db-6.2.23/src/db/db_pr.o 				\
			./db-6.2.23/src/db/db_rec.o 				\
			./db-6.2.23/src/db/db_reclaim.o 			\
			./db-6.2.23/src/db/db_remove.o 			\
			./db-6.2.23/src/db/db_rename.o 			\
			./db-6.2.23/src/db/db_ret.o 				\
			./db-6.2.23/src/db/db_setid.o 			\
			./db-6.2.23/src/db/db_setlsn.o 			\
			./db-6.2.23/src/db/db_slice.o 			\
			./db-6.2.23/src/db/db_sort_multiple.o 	\
			./db-6.2.23/src/db/db_stati.o 			\
			./db-6.2.23/src/db/db_truncate.o 		\
			./db-6.2.23/src/db/db_upg.o 				\
			./db-6.2.23/src/db/db_upg_opd.o 			\
 			./db-6.2.23/src/db/partition.o

kbdb-y += 	./db-6.2.23/src/dbreg/dbreg.o 		\
			./db-6.2.23/src/dbreg/dbreg_stat.o 	\
			./db-6.2.23/src/dbreg/dbreg_auto.o 	\
			./db-6.2.23/src/dbreg/dbreg_rec.o 	\
			./db-6.2.23/src/dbreg/dbreg_util.o 	

kbdb-y += 	./db-6.2.23/src/env/env_alloc.o 		\
			./db-6.2.23/src/env/env_config.o 	\
			./db-6.2.23/src/env/env_backup.o 	\
			./db-6.2.23/src/env/env_failchk.o 	\
			./db-6.2.23/src/env/env_file.o 		\
			./db-6.2.23/src/env/env_globals.o 	\
			./db-6.2.23/src/env/env_open.o 		\
			./db-6.2.23/src/env/env_method.o 	\
			./db-6.2.23/src/env/env_name.o 		\
			./db-6.2.23/src/env/env_recover.o 	\
			./db-6.2.23/src/env/env_region.o 	\
			./db-6.2.23/src/env/env_register.o 	\
			./db-6.2.23/src/env/env_sig.o 		\
			./db-6.2.23/src/env/env_slice.o 		\
			./db-6.2.23/src/env/env_stat.o 

kbdb-y += 	./db-6.2.23/src/fileops/fileops_auto.o 	\
 			./db-6.2.23/src/fileops/fop_basic.o 		\
 			./db-6.2.23/src/fileops/fop_rec.o 		\
 			./db-6.2.23/src/fileops/fop_util.o 		

kbdb-y += 	./db-6.2.23/src/hmac/hmac.o	\
 			./db-6.2.23/src/hmac/sha1.o

kbdb-y += 	./db-6.2.23/src/log/log.o 			\
 			./db-6.2.23/src/log/log_archive.o	\
 			./db-6.2.23/src/log/log_compare.o 	\
 			./db-6.2.23/src/log/log_debug.o 		\
 			./db-6.2.23/src/log/log_get.o 		\
 			./db-6.2.23/src/log/log_method.o 	\
 			./db-6.2.23/src/log/log_print.o 		\
 			./db-6.2.23/src/log/log_put.o 		\
 			./db-6.2.23/src/log/log_stat.o


kbdb-y +=	./db-6.2.23/src/mp/mp_alloc.o 	\
 			./db-6.2.23/src/mp/mp_backup.o 	\
 			./db-6.2.23/src/mp/mp_bh.o 		\
 			./db-6.2.23/src/mp/mp_fget.o 	\
 			./db-6.2.23/src/mp/mp_fmethod.o 	\
 			./db-6.2.23/src/mp/mp_fopen.o 	\
 			./db-6.2.23/src/mp/mp_fput.o 	\
 			./db-6.2.23/src/mp/mp_fset.o 	\
 			./db-6.2.23/src/mp/mp_method.o 	\
 			./db-6.2.23/src/mp/mp_mvcc.o 	\
 			./db-6.2.23/src/mp/mp_region.o 	\
 			./db-6.2.23/src/mp/mp_register.o \
 			./db-6.2.23/src/mp/mp_resize.o 	\
 			./db-6.2.23/src/mp/mp_stat.o 	\
 			./db-6.2.23/src/mp/mp_sync.o 	\
 			./db-6.2.23/src/mp/mp_trickle.o

kbdb-y += 	./db-6.2.23/src/sequence/seq_stat.o 	\
 			./db-6.2.23/src/sequence/sequence.o

kbdb-y +=	./db-6.2.23/src/clib/snprintf.o \
			./db-6.2.23/src/clib/atol.o \
			./db-6.2.23/src/clib/atoi.o \
			./db-6.2.23/src/clib/strtoul.o \
			./db-6.2.23/src/clib/printf.o \
			./db-6.2.23/src/clib/strsep.o \
			./db-6.2.23/src/clib/isprint.o \
			./db-6.2.23/src/clib/strncmp.o \
			./db-6.2.23/src/clib/isspace.o \
			./db-6.2.23/src/clib/strtol.o \
			./db-6.2.23/src/clib/isdigit.o

kbdb-y +=	./db-6.2.23/src/txn/txn.o 			\
 			./db-6.2.23/src/txn/txn_auto.o 		\
 			./db-6.2.23/src/txn/txn_chkpt.o 		\
 			./db-6.2.23/src/txn/txn_failchk.o 	\
 			./db-6.2.23/src/txn/txn_method.o 	\
 			./db-6.2.23/src/txn/txn_rec.o 		\
 			./db-6.2.23/src/txn/txn_recover.o 	\
 			./db-6.2.23/src/txn/txn_region.o 	\
 			./db-6.2.23/src/txn/txn_stat.o 		\
 			./db-6.2.23/src/txn/txn_util.o


kbdb-y += 	./db-6.2.23/src/xa/xa.o 		\
 			./db-6.2.23/src/xa/xa_map.o

kbdb-y +=	./db-6.2.23/build_kernel/libk.o \
			./db-6.2.23/build_kernel/syscalls.o

ccflags-y := -Wfatal-errors -I$(src)/db-6.2.23/build_kernel/include/ \
				-I$(src)/db-6.2.23/build_kernel/include/libc/ \
				-I$(src)/db-6.2.23/src/  -D_GNU_SOURCE -O3

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make -C /home/x/linux/code/linux-4.9/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	@find . \( -name '*.[oa]' -o -name '*.ko' -o -name '.*.cmd' \) -type f -print | xargs rm -f

