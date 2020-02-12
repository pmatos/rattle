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

#include "common.h"

// Runtime entry point.
// The compiler generated code is linked here.
extern schptr_t scheme_entry (void);

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

void
runtime_startup (void)
{
  print_ptr (scheme_entry ());
}

int
main (void)
{
  runtime_startup ();
  return 0;
}
