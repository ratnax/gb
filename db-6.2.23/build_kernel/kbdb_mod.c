#include <linux/module.h>

static int __init init_kbdb_fs(void)
{
	return 0; 
}

static void __exit exit_kbdb_fs(void)
{
	return;
}

module_init(init_kbdb_fs)
module_exit(exit_kbdb_fs)
MODULE_LICENSE("GPL");

