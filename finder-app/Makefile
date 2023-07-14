#command line
ifeq ($(strip $(CROSS_COMPILE)),)
	CC := gcc
else
	CC := $(CROSS_COMPILE)gcc
endif

#passed options
CFLAGS= -c -Wall -g

all: writer

writer: writer.o
	$(CC) writer.o -o writer

writer.o: writer.c
	$(CC) $(CFLAGS) writer.c

clean:
	rm -rf *.o writer *.elf *.map