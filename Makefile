# Rattle Makefile
all: rattle runtime.o

# Flags
CFLAGS = -std=gnu11 -fms-extensions
EXTRA_CFLAGS =
LDFLAGS = -ldl

# If clang we need to add a warning disable
CCNAME = $(findstring clang,$(shell $(CC) --version))
ifeq ($(CCNAME),clang)
CFLAGS := $(CFLAGS) -Wno-microsoft-anon-tag
endif

ifdef DEBUG
CFLAGS := $(CFLAGS) -O0 -g
LDFLAGS := $(LDFLAGS)

# UBSAN only works when DEBUG=1
ifdef UBSAN
CFLAGS := $(CFLAGS) -fsanitize=undefined
LDFLAGS := $(LDFLAGS) -fsanitize=undefined
UBSANLIB :="-lubsan"
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
	$(CC) $(CFLAGS) -MM $^ -I. -MF ./.depend

include .depend

# Rules
rattle.o: $(SRCS) config.h
	$(CC) $(CPPFLAGS) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c $(SRCS) -o $@
rattle: rattle.o
	$(CC) $^ -o $@ $(LDFLAGS)

runtime.o: src/runtime.c
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -c $< -o $@

config.h:
	echo '#pragma once' > $@
ifdef UBSAN
	echo "#define UBSANLIB \"$(UBSANLIB)\"" >> $@
else
	echo "/* #define UBSANLIB */" >> $@
endif

.PHONY: tests
tests:
	racket tests/script/test.rkt -c "./rattle -e --" tests/null.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/fixnum.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/boolean.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/char.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/primitives.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/if.tests
	racket tests/script/test.rkt -c "./rattle -e --" tests/let.tests
	./rattle -o fx1 -c tests/fx1.rl && test `./fx1` = "1"
	./rattle -o fxadd1 -c tests/fxadd1.rl && test `./fxadd1` = "190"
	./rattle -o primitives-1 -c tests/primitives-1.rl && test `./primitives-1` = "#f"

.PHONY: clean
clean:
	$(RM) rattle runtime.o rattle.o config.h
