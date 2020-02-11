# Rattle Makefile
all: rattle runtime.o

# Flags
CFLAGS = -std=gnu11
EXTRA_CFLAGS =
LDFLAGS = -ldl

ifdef DEBUG
CFLAGS := $(CFLAGS) -O0 -g
LDFLAGS := $(LDFLAGS)

# UBSAN only works when DEBUG=1
ifdef UBSAN
CFLAGS := $(CFLAGS) -fsanitize=undefined
LDFLAGS := $(LDFLAGS) -fsanitize=undefined
endif

else ifdef COVERAGE	
CFLAGS := $(CFLAGS) -O0 -g
LDFLAGS := $(LDFLAGS) --coverage 
EXTRA_CFLAGS := --coverage
else
CPPFLAGS := -DNDEBUG
CFLAGS := $(CFLAGS) -O3 -flto -march=native
LDFLAGS := $(LDFLAGS) $(CFLAGS)
endif

CFLAGS := $(CFLAGS) -Werror -Wall -Wextra -Wshadow

# Sources and Dependencies
# TODO: make runtime.c also depend on headers

SRCS = src/rattle.c
depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CC) $(CFLAGS) -MM $^ -MF ./.depend

include .depend

# Rules
rattle.o: $(SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -c $(SRCS) -o $@
rattle: rattle.o
	$(CC) $^ -o $@ $(LDFLAGS)

runtime.o: src/runtime.c
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: tests
tests:

.PHONY: clean
clean:
	$(RM) rattle runtime.o rattle.o
