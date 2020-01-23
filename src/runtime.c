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

// Runtime entry point.
// The compiler generated code is linked here.

/* all scheme values are of type ptrs */
typedef uint64_t ptr;

extern ptr scheme_entry (void);

/* define all scheme constants */
#define boolf    0x2f
#define boolt    0x6f
#define null     0x3f

#define ch_mask  0xff
#define ch_tag   0x0f
#define ch_shift 8

#define fx_mask  0x03
#define fx_tag   0x00
#define fx_shift 2

static void
print_char (uint64_t code) {
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
print_ptr(ptr x)
{
  if ((x & fx_mask) == fx_tag)
    printf ("%" PRIi64, (int64_t) ((uint64_t) x) >> fx_shift);
  else if ((x & ch_mask) == ch_tag)
    print_char (((uint64_t) x) >> ch_shift);
  else if (x == boolf)
    printf ("#f");
  else if (x == boolt)
    printf ("#t");
  else if (x == null)
    printf ("()");
  else
    printf ("#<unknown 0x%08lx>", x);
  printf ("\n");
}

void
runtime_startup (void)
{
  print_ptr (scheme_entry ());
}
