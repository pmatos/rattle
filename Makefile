# Rattle Makefile
all: rattle runtime.o

EXTRA_CFLAGS =
LDFLAGS = -ldl

ifdef DEBUG
CFLAGS := $(CFLAGS) -O0 -g -fsanitize=undefined
else ifdef COVERAGE	
CFLAGS := $(CFLAGS) -O0 -g
EXTRA_CFLAGS := --coverage
else
CFLAGS := $(CFLAGS) -O3
endif

CFLAGS := $(CFLAGS) -Werror -Wall -Wextra

rattle: src/rattle.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $^ -o $@ $(LDFLAGS)

runtime.o: src/runtime.c
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: tests
tests:

.PHONY: clean
clean:
	$(RM) rattle runtime.o
