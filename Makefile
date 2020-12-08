# Rattle Makefile
.PHONY: all
all: rattle runtime.o

CFLAGS := $(CFLAGS)

# Flags
CFLAGS += -std=gnu11 -fms-extensions
CPPFLAGS := -D_DEFAULT_SOURCE
EXTRA_CFLAGS :=
LDFLAGS := -ldl

# If clang we need to add a warning disable
CCNAME := $(findstring clang,$(shell $(CC) --version))
ifeq ($(CCNAME),clang)
CFLAGS += -Wno-microsoft-anon-tag
endif

ifdef DEBUG
CFLAGS += $(CFLAGS) -O0 -g

# UBSAN only works when DEBUG=1
ifdef UBSAN
CFLAGS +=  -fsanitize=undefined
LDFLAGS += -fsanitize=undefined
UBSANLIB := "-lubsan"
endif

ifdef ASAN
CFLAGS +=  -fsanitize=address
LDFLAGS += -fsanitize=address -lasan
UBSANLIB := "-lasan"
endif

else ifdef COVERAGE	
CFLAGS += -O0 -g
LDFLAGS += --coverage
EXTRA_CFLAGS := --coverage

else
CPPFLAGS += -DNDEBUG
CFLAGS += -O3 -flto -march=native
LDFLAGS += $(CFLAGS)
endif

CFLAGS += -Werror -Wall -Wextra -Wshadow

# Sources and Dependencies
# TODO: make runtime.c also depend on headers

SRCS := $(wildcard src/*.c)
HDRS := $(wildcard src/*.h)
OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)

rattle: $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Rules
src/%.o: src/%.c
	$(CC) $(CPPFLAGS) -I. $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@

runtime.o: src/runtime/runtime.c
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -c $< -o $@

config.h:
	echo '#pragma once' > $@
ifdef UBSAN
	echo "#define UBSANLIB \"$(UBSANLIB)\"" >> $@
else
	echo "/* #define UBSANLIB */" >> $@
endif

src/%.d: src/%.c config.h
	set -e; \
         rm -f $@; \
         $(CC) -MM $(CPPFLAGS) -I. $< > $@.$$$$; sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
         rm -f $@.$$$$
include $(DEPS)

.PHONY: test btests afltests itests btest
test: btest afltest itest

btest: btestimm btestcomp
btestimm:
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/null.tests
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/fixnum.tests
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/boolean.tests
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/char.tests

btestcomp:
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/primitives.tests
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/if.tests
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/let.tests
	racket tests/script/test.rkt -c "$(TEST_PREFIX) ./rattle -e --" tests/lets.tests

# AFL crash tests
afltest:
	for t in tests/afl/*.rl; do $(TEST_PREFIX) ./rattle -o /dev/null -c $$t; [ "$$?" = "1" ] || true ; done

# Integration tests
itest:
	$(TEST_PREFIX) ./rattle -o fx1 -c tests/fx1.rl && test `./fx1` = "1"
	$(TEST_PREFIX) ./rattle -o fxadd1 -c tests/fxadd1.rl && test `./fxadd1` = "190"
	$(TEST_PREFIX) ./rattle -o primitives-1 -c tests/primitives-1.rl && test `./primitives-1` = "#f"

.PHONY: compile_commands.json
compile_commands.json:
	rm -f $@
	$(MAKE) clean
	bear $(MAKE)

.PHONY: clean
clean:
	$(RM) rattle $(OBJS) config.h $(DEPS)

.PHONY: check-format
check-format:
	$(foreach file,$(SRCS) $(HDRS),clang-format --Werror -n $(file);)
