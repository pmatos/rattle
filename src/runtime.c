/*
 * Copyright 2020 Paulo Matos
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <limits.h>

#include "common.h"

// Runtime entry point.
// The compiler generated code is linked here.
extern schptr_t scheme_entry (uint8_t *);

static void
print_char (char code) {
  switch (code)
    {
    case 0x7:
      printf ("#\\alarm");
      break;
    case 0x8:
      printf ("#\\backspace");
      break;
    case 0x7f:
      printf ("#\\delete");
      break;
    case 0x1b:
      printf ("#\\escape");
      break;
    case 0xa:
      printf ("#\\newline");
      break;
    case 0x0:
      printf ("#\\null");
      break;
    case 0xd:
      printf ("#\\return");
      break;
    case ' ':
      printf ("#\\space");
      break;
    case 0x9:
      printf ("#\\tab");
      break;
    default:
      printf ("#\\%c", (char) code);
      break;
    }
}

static void
print_ptr(schptr_t x)
{
  if (sch_imm_fixnum_p (x))
    printf ("%" PRIi64, sch_decode_imm_fixnum (x));
  else if (sch_imm_char_p (x))
    print_char (sch_decode_imm_char (x));
  else if (sch_imm_false_p (x))
    printf ("#f");
  else if (sch_imm_true_p (x))
    printf ("#t");
  else if (sch_imm_null_p (x))
    printf ("()");
  else
    printf ("#<unknown 0x%08" PRIxPTR ">", x);
  printf ("\n");
}


// Stack size in words (enough for 16K words)
#define WORD_STACK_SIZE (16 * 1024)

/*
  | ...                      |
  +--------------------------+
  | Protected Page           |
  +--------------------------+
  |                          |
  |                          |
  |                          |
  |                          |
  +--------------------------+
  |                          |
  +--------------------------+ /.\
  |                          |  |
  +--------------------------+  | stack grows up
  |                          |  |
  +--------------------------+
  |                          | - Stack bottom
  +--------------------------+
  | Protected Page           |
  +--------------------------+
  | ...                      |

    We allocate for use a stack with size bytes of size, aligned to a page size.
    However, in reality we allocate 2 more pages, and we protect them so that we
    get a segmentation fault if we try to access them by mistake.
 */
static uint8_t *
allocate_protected_space (size_t size)
{
  int pagesize = sysconf (_SC_PAGESIZE);
  int status = 0;

  // this is the requested size aligned to a page
  size_t aligned_size = ((size + pagesize - 1) / pagesize) * pagesize;

  // next (aligned_size + 2 * pagesize) adds the two pages we want to use for protection
  uint8_t *p = mmap (NULL, aligned_size + (2 * pagesize),
                     PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE,
                     0, 0);
  if (p == MAP_FAILED)
    {
      fprintf (stderr, "failed to allocate stack space of size `%zu'\n", size);
      exit (EXIT_FAILURE);
    }

  // Protect bottom page
  status = mprotect (p, pagesize, PROT_NONE);
  if (status != 0)
    {
      fprintf (stderr, "failed to protect stack space of size `%zu'\n", size);
      exit (EXIT_FAILURE);
    }

  // Protect top page
  status = mprotect (p + pagesize + aligned_size, pagesize, PROT_NONE);
  if (status != 0)
    {
      fprintf (stderr, "failed to protect stack space of size `%zu'\n", size);
      exit (EXIT_FAILURE);
    }

  // Return for use the bottom usable page for the stack
  return p + pagesize;
}

static void
deallocate_protected_space (uint8_t *p, size_t size)
{
  int page = sysconf (_SC_PAGESIZE);
  int aligned_size = ((size + page - 1) / page) * page;
  int status = munmap (p - page, aligned_size + (2 * page));
  if (status)
    fprintf (stderr, "warning: failed to deallocate stack space of size `%zu'\n", size);
}

void
runtime_startup (void)
{
  size_t stack_size = (WORD_STACK_SIZE * WORD_BYTES); // 16K words of space in stack
  uint8_t *stack_top = allocate_protected_space (stack_size);
  uint8_t *stack_base = stack_top + stack_size;
  print_ptr (scheme_entry (stack_base));
  deallocate_protected_space (stack_top, stack_size);
}

int
main (void)
{
  runtime_startup ();
  return 0;
}
