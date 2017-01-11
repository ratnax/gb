/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 2016 Oracle and/or its affiliates.  All rights reserved.
 *
 * $Id$
 */

#include "db_config.h"

#include "db_int.h"

#ifdef HAVE_SYSTEM_INCLUDE_FILES
#ifdef macintosh
#include <TFileSpec.h>
#endif
#endif

/*
 * __os_tmpdir --
 *	Set the temporary directory path.
 *
 * The order of items in the list structure and the order of checks in
 * the environment are documented.
 *
 * PUBLIC: int __os_tmpdir __P((ENV *, u_int32_t));
 */
int
__os_tmpdir(env, flags)
	ENV *env;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int isdir, ret;
	char *tdir, *tdir_buf = NULL;

	dbenv = env->dbenv;

	tdir_buf = malloc(DB_MAXPATHLEN);
	if (!tdir_buf)
		return -ENOMEM;

	/* Use the environment if it's permitted and initialized. */
	if (LF_ISSET(DB_USE_ENVIRON) ||
	    (LF_ISSET(DB_USE_ENVIRON_ROOT) && __os_isroot())) {
		/* POSIX: TMPDIR */
		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    env, "TMPDIR", &tdir, DB_MAXPATHLEN)) != 0)
			goto out;

		if (tdir != NULL && tdir[0] != '\0')
			goto found;

		/*
		 * Windows: TEMP, TMP
		 */
		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    env, "TEMP", &tdir, DB_MAXPATHLEN)) != 0)
			goto out;

		if (tdir != NULL && tdir[0] != '\0')
			goto found;

		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    env, "TMP", &tdir, DB_MAXPATHLEN)) != 0)
			goto out;

		if (tdir != NULL && tdir[0] != '\0')
			goto found;

		/* Macintosh */
		tdir = tdir_buf;
		if ((ret = __os_getenv(
		    env, "TempFolder", &tdir, DB_MAXPATHLEN)) != 0) {
			goto out;
		}

		if (tdir != NULL && tdir[0] != '\0') {
found:		
			ret = __os_strdup(env, tdir, &dbenv->db_tmp_dir);
			goto out;
		}
	}

#ifdef macintosh
	/* Get the path to the temporary folder. */
	{FSSpec spec;

		if (!Special2FSSpec(kTemporaryFolderType,
		    kOnSystemDisk, 0, &spec)) {
			ret = (__os_strdup(env,
			    FSp2FullPath(&spec), &dbenv->db_tmp_dir));
			goto out;
		}
	}
#endif
#ifdef DB_WIN32
	/* Get the path to the temporary directory. */
	{
		_TCHAR tpath[DB_MAXPATHLEN + 1];
		char *path, *eos;

		if (GetTempPath(DB_MAXPATHLEN, tpath) > 2) {
			FROM_TSTRING(env, tpath, path, ret);
			if (ret != 0)
				goto out;

			eos = path + strlen(path) - 1;
			if (*eos == '\\' || *eos == '/')
				*eos = '\0';
			if (__os_exists(env, path, &isdir) == 0 && isdir) {
				ret = __os_strdup(env,
				    path, &dbenv->db_tmp_dir);
				FREE_STRING(env, path);
				goto out;
			}
			FREE_STRING(env, path);
		}
	}
#endif

	/*
	 * Step through the static list looking for a possibility.
	 *
	 * We don't use the obvious data structure because some C compilers
	 * (and I use the phrase loosely) don't like static data arrays.
	 */
#define	DB_TEMP_DIRECTORY(n) {						\
	char *__p = n;							\
	if (__os_exists(env, __p, &isdir) == 0 && isdir != 0) {		\
		ret = (__os_strdup(env, __p, &dbenv->db_tmp_dir));	\
		goto out;	\
	}	\
	}
#ifdef DB_WIN32
	DB_TEMP_DIRECTORY("/temp");
	DB_TEMP_DIRECTORY("C:/temp");
	DB_TEMP_DIRECTORY("C:/tmp");
#else
	DB_TEMP_DIRECTORY("/var/tmp");
	DB_TEMP_DIRECTORY("/usr/tmp");
	DB_TEMP_DIRECTORY("/tmp");
#if defined(ANDROID) || defined(DB_ANDROID)
	DB_TEMP_DIRECTORY("/cache");
#endif
#endif

	/*
	 * If we don't have any other place to store temporary files, store
	 * them in the current directory.
	 */
	ret = __os_strdup(env, "", &dbenv->db_tmp_dir);
out:
	if (tdir_buf) 
		free(tdir_buf);
	return ret;
}
