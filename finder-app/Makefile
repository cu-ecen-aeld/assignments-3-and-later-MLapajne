#command line
ifeq ($(strip $(CROSS_COMPILE)),)
	CC := gcc
else
	CC := $(CROSS_COMPILE)gcc
endif

#passed options
CFLAGS= -c -Wall -g
TARGET = writer
OBJFILES = writer.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJFILES)