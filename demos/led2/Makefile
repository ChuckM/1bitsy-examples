OBJS= ../util/clock.o ../util/console.o ../util/retarget.o time.o

BINARY= main

LDLIBS= -lqrcode

LDSCRIPT = ../1bitsy.ld

CFLAGS+= -I../../libqrcode
LDFLAGS+= -L../../libqrcode

include ../../Makefile.include
