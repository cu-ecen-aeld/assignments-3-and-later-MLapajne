#passed options
CC ?= gcc
CFLAGS ?= -g -Wall -Wextra -Werror
LDFLAGS ?= -lpthread
TARGET ?= aesdsocket
OBJFILES ?= aesdsocket.o

ifdef CROSS_COMPILE
    CC ?= $(CROSS_COMPILE)gcc
endif



.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJFILES)