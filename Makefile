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
	racket tests/script/test.rkt -c "./rattle -e --" tests/null.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/fixnum.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/boolean.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/char.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/primitives.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/if.tests
	./rattle -o fx1 -c tests/fx1.rl && test `./fx1` = "1"
	./rattle -o fxadd1 -c tests/fxadd1.rl && test `./fxadd1` = "190"

.PHONY: clean
clean:
	$(RM) rattle runtime.o rattle.o
