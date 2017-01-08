#
# Makefile for the linux ext2-filesystem routines.
#

obj-m := kbdb.o


kbdb-y := 	kbdb_mod.o

kbdb-y += 	../src/os/os_abort.o 	\
 			../src/os/os_abs.o 		\
 			../src/os/os_alloc.o 	\
 			../src/os/os_clock.o 	\
 			../src/os/os_cpu.o 		\
 			../src/os/os_ctime.o 	\
 			../src/os/os_config.o 	\
 			../src/os/os_dir.o 		\
 			../src/os/os_errno.o 	\
 			../src/os/os_fid.o 		\
 			../src/os/os_flock.o 	\
 			../src/os/os_fsync.o 	\
 			../src/os/os_getenv.o 	\
 			../src/os/os_handle.o 	\
 			../src/os/os_map.o		\
 			../src/os/os_mkdir.o 	\
 			../src/os/os_open.o 	\
 			../src/os/os_path.o 	\
 			../src/os/os_pid.o 		\
 			../src/os/os_rename.o 	\
 			../src/os/os_rmdir.o 	\
 			../src/os/os_root.o 	\
 			../src/os/os_rpath.o 	\
 			../src/os/os_rw.o 		\
 			../src/os/os_seek.o 	\
 			../src/os/os_stack.o 	\
 			../src/os/os_stat.o 	\
 			../src/os/os_tmpdir.o 	\
 			../src/os/os_truncate.o \
 			../src/os/os_uid.o 		\
 			../src/os/os_unlink.o 	\
 			../src/os/os_yield.o


kbdb-y += 	../src/mutex/mut_tas.o		\
			../src/mutex/mut_pthread.o	\
			../src/mutex/mut_alloc.o 	\
			../src/mutex/mut_failchk.o 	\
			../src/mutex/mut_method.o 	\
			../src/mutex/mut_region.o 	\
			../src/mutex/mut_stat.o 	

kbdb-y +=	../src/btree/bt_compare.o 	\
			../src/btree/bt_compress.o 	\
			../src/btree/bt_conv.o 		\
			../src/btree/bt_curadj.o	\
			../src/btree/bt_cursor.o	\
			../src/btree/bt_delete.o	\
			../src/btree/bt_method.o	\
			../src/btree/bt_open.o		\
			../src/btree/bt_put.o		\
			../src/btree/bt_rec.o		\
			../src/btree/bt_reclaim.o	\
			../src/btree/bt_recno.o		\
			../src/btree/bt_rsearch.o	\
			../src/btree/bt_search.o	\
			../src/btree/bt_split.o		\
			../src/btree/bt_stat.o		\
			../src/btree/bt_compact.o	\
			../src/btree/bt_upgrade.o	\
			../src/btree/btree_auto.o	


kbdb-y += 	../src/hash/hash_stub.o 	\
 			../src/hash/hash_func.o 	

kbdb-y +=	../src/heap/heap_stub.o 	
kbdb-y +=	../src/qam/qam_stub.o
kbdb-y +=	../src/rep/rep_stub.o
kbdb-y +=	../src/repmgr/repmgr_stub.o
kbdb-y +=	../src/db/db_vrfy_stub.o
kbdb-y +=	../src/log/log_verify_stub.o

kbdb-y +=	../src/lock/lock.o 			\
			../src/lock/lock_deadlock.o \
			../src/lock/lock_failchk.o 	\
			../src/lock/lock_id.o 		\
			../src/lock/lock_list.o 	\
			../src/lock/lock_method.o 	\
			../src/lock/lock_region.o 	\
			../src/lock/lock_stat.o 	\
			../src/lock/lock_timer.o 	\
			../src/lock/lock_util.o 	

kbdb-y +=	../src/crypto/aes_method.o 					\
			../src/crypto/crypto.o 						\
			../src/crypto/mersenne/mt19937db.o 			\
			../src/crypto/rijndael/rijndael-alg-fst.o 	\
			../src/crypto/rijndael/rijndael-api-fst.o 	


kbdb-y +=	../src/blob/blob_fileops.o 	\
			../src/blob/blob_page.o 	\
			../src/blob/blob_stream.o 	\
			../src/blob/blob_util.o 	

kbdb-y +=	../src/common/clock.o			\
			../src/common/db_byteorder.o 	\
			../src/common/db_shash.o 		\
			../src/common/db_compint.o 		\
			../src/common/db_err.o 			\
			../src/common/db_getlong.o 		\
			../src/common/db_idspace.o 		\
			../src/common/db_log2.o 		\
			../src/common/dbt.o 			\
 			../src/common/mkpath.o			\
 			../src/common/openflags.o		\
 			../src/common/os_method.o		\
 			../src/common/zerofill.o


kbdb-y += 	../src/db/crdel_auto.o 			\
			../src/db/crdel_rec.o 			\
			../src/db/db.o 					\
			../src/db/db_am.o 				\
			../src/db/db_auto.o 			\
			../src/db/db_backup.o 			\
			../src/db/db_cam.o 				\
			../src/db/db_cds.o 				\
			../src/db/db_compact.o 			\
			../src/db/db_conv.o 			\
			../src/db/db_copy.o 			\
			../src/db/db_dispatch.o 		\
			../src/db/db_dup.o 				\
			../src/db/db_iface.o 			\
			../src/db/db_join.o 			\
			../src/db/db_meta.o 			\
			../src/db/db_method.o 			\
			../src/db/db_open.o 			\
			../src/db/db_overflow.o 		\
			../src/db/db_pr.o 				\
			../src/db/db_rec.o 				\
			../src/db/db_reclaim.o 			\
			../src/db/db_remove.o 			\
			../src/db/db_rename.o 			\
			../src/db/db_ret.o 				\
			../src/db/db_setid.o 			\
			../src/db/db_setlsn.o 			\
			../src/db/db_slice.o 			\
			../src/db/db_sort_multiple.o 	\
			../src/db/db_stati.o 			\
			../src/db/db_truncate.o 		\
			../src/db/db_upg.o 				\
			../src/db/db_upg_opd.o 			\
 			../src/db/partition.o

kbdb-y += 	../src/dbreg/dbreg.o 		\
			../src/dbreg/dbreg_stat.o 	\
			../src/dbreg/dbreg_auto.o 	\
			../src/dbreg/dbreg_rec.o 	\
			../src/dbreg/dbreg_util.o 	

kbdb-y += 	../src/env/env_alloc.o 		\
			../src/env/env_config.o 	\
			../src/env/env_backup.o 	\
			../src/env/env_failchk.o 	\
			../src/env/env_file.o 		\
			../src/env/env_globals.o 	\
			../src/env/env_open.o 		\
			../src/env/env_method.o 	\
			../src/env/env_name.o 		\
			../src/env/env_recover.o 	\
			../src/env/env_region.o 	\
			../src/env/env_register.o 	\
			../src/env/env_sig.o 		\
			../src/env/env_slice.o 		\
			../src/env/env_stat.o 

kbdb-y += 	../src/fileops/fileops_auto.o 	\
 			../src/fileops/fop_basic.o 		\
 			../src/fileops/fop_rec.o 		\
 			../src/fileops/fop_util.o 		

kbdb-y += 	../src/hmac/hmac.o	\
 			../src/hmac/sha1.o

kbdb-y += 	../src/log/log.o 			\
 			../src/log/log_archive.o	\
 			../src/log/log_compare.o 	\
 			../src/log/log_debug.o 		\
 			../src/log/log_get.o 		\
 			../src/log/log_method.o 	\
 			../src/log/log_print.o 		\
 			../src/log/log_put.o 		\
 			../src/log/log_stat.o


kbdb-y +=	../src/mp/mp_alloc.o 	\
 			../src/mp/mp_backup.o 	\
 			../src/mp/mp_bh.o 		\
 			../src/mp/mp_fget.o 	\
 			../src/mp/mp_fmethod.o 	\
 			../src/mp/mp_fopen.o 	\
 			../src/mp/mp_fput.o 	\
 			../src/mp/mp_fset.o 	\
 			../src/mp/mp_method.o 	\
 			../src/mp/mp_mvcc.o 	\
 			../src/mp/mp_region.o 	\
 			../src/mp/mp_register.o \
 			../src/mp/mp_resize.o 	\
 			../src/mp/mp_stat.o 	\
 			../src/mp/mp_sync.o 	\
 			../src/mp/mp_trickle.o

kbdb-y += 	../src/sequence/seq_stat.o 	\
 			../src/sequence/sequence.o

kbdb-y +=	../src/clib/snprintf.o \
			../src/clib/atol.o \
			../src/clib/atoi.o \
			../src/clib/strtoul.o \
			../src/clib/printf.o \
			../src/clib/strsep.o \
			../src/clib/isprint.o \
			../src/clib/strncmp.o \
			../src/clib/isspace.o \
			../src/clib/strtol.o \
			../src/clib/isdigit.o

kbdb-y +=	../src/txn/txn.o 			\
 			../src/txn/txn_auto.o 		\
 			../src/txn/txn_chkpt.o 		\
 			../src/txn/txn_failchk.o 	\
 			../src/txn/txn_method.o 	\
 			../src/txn/txn_rec.o 		\
 			../src/txn/txn_recover.o 	\
 			../src/txn/txn_region.o 	\
 			../src/txn/txn_stat.o 		\
 			../src/txn/txn_util.o


kbdb-y += 	../src/xa/xa.o 		\
 			../src/xa/xa_map.o

kbdb-y +=	libk.o \
			syscalls.o

CLEAN_FILES += kbdb-y

ccflags-y := -Wfatal-errors -I$(src)/include -I$(src)/include/libc -I$(src)/../src -D_GNU_SOUCE -O3

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make -C /home/x/linux/code/linux-4.9/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	@find ../src/ \
		\( -name '*.[oa]' -o -name '*.ko' -o -name '.*.cmd' \) -type f -print | xargs rm -f

