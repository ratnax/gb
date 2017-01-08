#include <linux/module.h>

#include <db.h>
void test(void)
{
	int ret, ret_c;
	u_int32_t db_flags, env_flags;
	DB *dbp;
	DB_ENV *envp;
	const char *db_home_dir = "/media/x/";
	const char *file_name = "mydb.db";

	dbp = NULL;
	envp = NULL;
	/* Open the environment */
	ret = db_env_create(&envp, 0);
	if (ret != 0) {
		printk(KERN_ERR "Error creating environment handle: %s\n",
				db_strerror(ret));
		return;
	}
	env_flags = DB_CREATE | /* Create the environment if it does 
							   Library Version 12.1.6.2 Enabling Transactions
							   3/28/2016 Using Transactions with DB Page 15
							 * not already exist. */
		DB_PRIVATE |
		DB_INIT_TXN | /* Initialize transactions */
		DB_INIT_LOCK | /* Initialize locking. */
		DB_INIT_LOG | /* Initialize logging */
		DB_INIT_MPOOL; /* Initialize the in-memory cache. */
	ret = envp->open(envp, db_home_dir, env_flags, 0);
	if (ret != 0) {
		printk(KERN_ERR "Error opening environment: %s\n",
				db_strerror(ret));
		goto err;
	}
	/* Initialize the DB handle */
	ret = db_create(&dbp, envp, 0);
	if (ret != 0) {
		envp->err(envp, ret, "Database creation failed");
		goto err;
	}
	db_flags = DB_CREATE | DB_AUTO_COMMIT;
	ret = dbp->open(dbp, /* Pointer to the database */
			NULL, /* Txn pointer */
			file_name, /* File name */
			NULL, /* Logical db name */
			DB_BTREE, /* Database type (using btree) */
			db_flags, /* Open flags */
			0); /* File mode. Using defaults */
	if (ret != 0) {
		envp->err(envp, ret, "Database '%s' open failed",
				file_name);
		goto err;
	}
err:
	/* Close the database */
	if (dbp != NULL) {
		ret_c = dbp->close(dbp, 0);
		if (ret_c != 0) {
			envp->err(envp, ret_c, "Database close failed.");
			ret = ret_c;
		}
	}
	/* Close the environment */
	if (envp != NULL) {
		ret_c = envp->close(envp, 0);
		if (ret_c != 0) {
			printk(KERN_ERR "environment close failed: %s\n",
					db_strerror(ret_c));
			ret = ret_c;
		}
	}
	printk(KERN_ERR "Test:%d", ret);
}

static int __init init_kbdb_fs(void)
{
	test();
	return 0; 
}

static void __exit exit_kbdb_fs(void)
{
	return;
}

module_init(init_kbdb_fs)
module_exit(exit_kbdb_fs)
MODULE_LICENSE("GPL");

