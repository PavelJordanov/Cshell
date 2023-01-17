IDIR=include
DEPS=$(shell find $(IDIR) -name '*.h')
LIBS=-lm

CC=g++
CFLAGS=-I$(IDIR)

all: init main

init: 
	@mkdir -p obj

main: obj/cshell.o
#	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)
	$(CC) -o cshell $^ $(CFLAGS) $(LIBS)

obj/cshell.o: src/cshell.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
 
.PHONY: clean

clean:
	rm -fR obj/*.o obj cshell