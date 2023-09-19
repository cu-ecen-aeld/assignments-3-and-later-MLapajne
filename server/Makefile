#passed options
CC = gcc
CFLAGS= -g -Wall -Wextra -Werror
TARGET = aesdsocket
OBJFILES = aesdsocket.o

ifdef CROSS_COMPILE
    CC = $(CROSS_COMPILE)gcc
endif



.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJFILES)