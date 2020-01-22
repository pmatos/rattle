# Rattle Makefile
all: rattle runtime.o

EXTRA_CFLAGS =

ifdef DEBUG
CFLAGS := $(CFLAGS) -O0 -g
else ifdef COVERAGE	
CFLAGS := $(CFLAGS) -O0 -g
EXTRA_CFLAGS := --coverage
else
CFLAGS := $(CFLAGS) -O3
endif

CFLAGS := $(CFLAGS) -Werror -Wall -Wextra

rattle: src/rattle.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) $^ -o $@

runtime.o: src/runtime.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: tests
tests:

.PHONY: clean
clean:
	$(RM) rattle runtime.o
