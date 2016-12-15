#include <linux/fcntl.h>
#include <db.h>

DB *
dbopen(fname, flags, mode, type, openinfo)
	const char *fname;
	int flags, mode;
	DBTYPE type;
	const void *openinfo;
{

#define	USE_OPEN_FLAGS							\
	(O_CREAT | O_EXCL | O_NONBLOCK | O_RDONLY |		\
	 O_RDWR | O_TRUNC)

	if ((flags & ~(USE_OPEN_FLAGS)) == 0)
		return (__bt_open(fname, flags & USE_OPEN_FLAGS,
			    mode, openinfo, 0));
	//errno = EINVAL; TODO
	return (NULL);
}
