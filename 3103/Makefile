CC = gcc
CFLAGS_COMMON = -Wall

TARGETS = printFibonacci printAdd1K
COMMON  = mathlib
HEADERS = mathlib.h

TARGETS_ALL     = $(TARGETS)
TARGETS_DEBUG   = $(addprefix d_,$(TARGETS))
TARGETS_PROFILE = $(addprefix p_,$(TARGETS))

.PHONY: all clean debug profile

all: CFLAGS := $(CFLAGS_COMMON)
all: $(TARGETS_ALL)

debug: CFLAGS := $(CFLAGS_COMMON) -g
debug: $(TARGETS_DEBUG)

profile: CFLAGS := $(CFLAGS_COMMON) -pg
profile: $(TARGETS_PROFILE)

$(TARGETS_ALL): %: %a.o $(COMMON)a.o
	$(CC) $(CFLAGS) -o $@ $^

$(TARGETS_DEBUG): d_%: %d.o $(COMMON)d.o
	$(CC) $(CFLAGS) -o $@ $^

$(TARGETS_PROFILE): p_%: %p.o $(COMMON)p.o
	$(CC) $(CFLAGS) -o $@ $^

%a.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%d.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%p.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *a.o *d.o *p.o $(TARGETS_ALL) $(TARGETS_DEBUG) $(TARGETS_PROFILE) gmon.out
