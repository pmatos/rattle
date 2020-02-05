# Rattle Makefile
all: rattle runtime.o

# Flags
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

# Sources and Dependencies
SRCS = src/rattle.c
depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CC) $(CFLAGS) -MM $^ -MF ./.depend

include .depend

# Rules
rattle.o: $(SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $(SRCS) -o $@ $(LDFLAGS)
rattle: rattle.o
	$(CC) $^ -o $@ $(LDFLAGS)

runtime.o: src/runtime.c
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: tests
tests:

.PHONY: clean
clean:
	$(RM) rattle runtime.o
