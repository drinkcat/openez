CC       = gcc
CFLAGS   = -g -fpic -w -Wall
LD       = ld
OBJ     := $(filter-out test.o, $(patsubst %.c,%.o,$(wildcard *.c)))

MAKEXE   = make
LIBNAME  = ezusb.so

all: unix

clean:
	rm -f *.o $(LIBNAME) core

osx: $(OBJ) Makefile
	$(CC) -dynamiclib $(OBJ) -o $(LIBNAME)

unix: $(OBJ) Makefile
	$(LD) -shared $(OBJ) -lusb-1.0 -o $(LIBNAME)

$(patsubst %.c,%.o,$(wildcard *.c)) : %.o : %.c Makefile
	$(CC) $(CFLAGS) -c $<

