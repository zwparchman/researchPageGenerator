CC = gcc
F77 = f77

CFLAGS=-g --std=gnu89 -D_BSD_SOURCE
FFLAGS=-Og -g 

OBJECTS = system.o setup.o signal_handlers.o checkpoint.o \
          dlist.o include.o recover.o jmalloc.o mult.o

HDRS = libckpt.h structs.h externs.h dlist.h local.h

all: c fortran fort

c: testprog libckpt.a cmain.o coalesce

fortran: testprog libckpt.a fmain.o coalesce

local.h: configure.c
	$(CC) $(CFLAGS) -o configure configure.c
	./configure

testprog: testprog.c
	$(CC) $(CFLAGS) -o testprog testprog.c

coalesce: coalesce.c
	$(CC) $(CFLAGS) -o coalesce coalesce.c
	/bin/cp -f coalesce $(HOME)/bin

fmain.o: fmain.f
	$(F77) -c $(FFLAGS) fmain.f

fort: fmain.o libckpt.a
	$(F77) $(FFLAGS) fmain.o libckpt.a -o fort

libckpt.a: local.h $(OBJECTS)
	/bin/rm -f libckpt.a
	ar crv libckpt.a $(OBJECTS)
	
.c.o: $(HDRS)
	$(CC) -c $(CFLAGS) $*.c

example: mult.c libckpt.a cmain.o
	$(CC) $(CFLAGS) -o mult mult.c cmain.o libckpt.a

clean:
	/bin/rm -f *.o libckpt.a mult core configure local.h coalesce testprog \
~/bin/coalesce coalesce
