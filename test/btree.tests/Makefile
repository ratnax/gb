#	@(#)Makefile	8.15 (Berkeley) 7/28/94

PROG=	btest
OBJS=	btest.o strerror.o

# Uncomment the STAT line get hash and btree statistical use info.  This
# also forces ld to load the btree debug functions for use by gdb, which
# is useful.  The db library has to be compiled with -DSTATISTICS as well.
INC=	-I../../include -I../../btree/
OORG=	-g
#STAT=	-DSTATISTICS
CFLAGS=	-D__DBINTERFACE_PRIVATE -DDEBUG ${STAT} ${OORG} ${INC}

btest: ${OBJS} ../../libdb.a
	${CC} -o $@ ${OBJS} ../../libdb.a

strerror.o: ../../clib/strerror.c
	${CC} -c ../../clib/strerror.c

clean:
	rm -f ${OBJS} ${PROG} 

${OBJS}: Makefile
