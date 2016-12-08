#	@(#)Makefile	8.9 (Berkeley) 7/14/94

LIBDB=	libdb.a

OBJ1=
OBJ2=	bt_close.o bt_conv.o bt_debug.o bt_delete.o bt_get.o bt_open.o \
	bt_overflow.o bt_page.o bt_put.o bt_search.o bt_seq.o bt_split.o \
	bt_utils.o
OBJ3=	db.o
OBJ4=	mpool.o
OBJ5=

#MISC=	snprintf.o

${LIBDB}: ${OBJ1} ${OBJ2} ${OBJ3} ${OBJ4} ${OBJ5} ${MISC}
	rm -f $@
	ar cq $@ \
	    `lorder ${OBJ1} ${OBJ2} ${OBJ3} ${OBJ4} ${OBJ5} ${MISC} | tsort`
	ranlib $@

clean:
	rm -f ${LIBDB} ${OBJ1} ${OBJ2} ${OBJ3} ${OBJ4} ${OBJ5} ${MISC}

OORG=	-O
CL=	${CC} -c -D__DBINTERFACE_PRIVATE ${OORG} -I. -Iinclude

bt_close.o: btree/bt_close.c
	${CL} -Ibtree btree/bt_close.c
bt_conv.o: btree/bt_conv.c
	${CL} -Ibtree btree/bt_conv.c
bt_debug.o: btree/bt_debug.c
	${CL} -Ibtree btree/bt_debug.c
bt_delete.o: btree/bt_delete.c
	${CL} -Ibtree btree/bt_delete.c
bt_get.o: btree/bt_get.c
	${CL} -Ibtree btree/bt_get.c
bt_open.o: btree/bt_open.c
	${CL} -Ibtree btree/bt_open.c
bt_overflow.o: btree/bt_overflow.c
	${CL} -Ibtree btree/bt_overflow.c
bt_page.o: btree/bt_page.c
	${CL} -Ibtree btree/bt_page.c
bt_put.o: btree/bt_put.c
	${CL} -Ibtree btree/bt_put.c
bt_search.o: btree/bt_search.c
	${CL} -Ibtree btree/bt_search.c
bt_seq.o: btree/bt_seq.c
	${CL} -Ibtree btree/bt_seq.c
bt_split.o: btree/bt_split.c
	${CL} -Ibtree btree/bt_split.c
bt_stack.o: btree/bt_stack.c
	${CL} -Ibtree btree/bt_stack.c
bt_utils.o: btree/bt_utils.c
	${CL} -Ibtree btree/bt_utils.c

db.o: db/db.c
	${CL} db/db.c

mpool.o: mpool/mpool.c
	${CL} -Impool mpool/mpool.c

memmove.o:
	${CC} -DMEMMOVE -c -O -I. -Iinclude clib/memmove.c
mktemp.o:
	${CC} -c -O -I. -Iinclude clib/mktemp.c
#snprintf.o:
#	${CC} -c -O -I. -Iinclude clib/snprintf.c
