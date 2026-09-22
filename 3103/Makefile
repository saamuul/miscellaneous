CC = gcc
CFLAGS = -g -pg -Wall
TARGETS = printFibonacci printAdd1K
SOURCES = printFibonacci.c printAdd1K.c mathlib.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGETS)

$(TARGETS): %: %.o mathlib.o
	$(CC) $(CFLAGS) -o $@ $^

%.o:%.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGETS) gmon.out 
