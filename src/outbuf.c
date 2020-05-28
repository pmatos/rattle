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

#include "outbuf.h"

#include "err.h"
#include "memory.h"

#include <assert.h>
#include <string.h>

// make_output_buffer (void)
// Creates an output buffer and returns pointer to it
struct outbuf *
make_output_buffer (void)
{
  return alloc (sizeof (struct outbuf));
}

void
free_output_buffer (struct outbuf *ob)
{
  if (!ob)
    return;

  for (size_t i = 0; i < ob->sz; i++)
    free_section (ob->scs[i]);
  free (ob);
}

void
grow_output_buffer (struct outbuf *ob)
{
  ob->cap *= OUTBUF_INITIAL_CAP;
  ob->scs = (struct section **) grow (ob->scs, ob->cap);
}

bool
output_buffer_add_section (struct section *s, struct outbuf *ob)
{
  // see where to add the section
  struct section **scs = ob->scs;
  size_t i = 0;
  while (strcmp(scs[i]->name, s->name) < 0)
    i++;
  const size_t found = i;
  
  if (!strcmp (scs[i]->name, s->name))
    return false;

  // We are adding a section, check for capacity
  if (ob->sz == ob->cap - 1)
    grow_output_buffer (ob);

  // shift all elements
  for (;i < ob->sz; i++)
    {
      struct section *saved = scs[i+1];
      struct section *to_move = scs[i];
      scs[i+1] = to_move;
      to_move = saved;
    }

  scs[found] = s;
  ob->sz++;
  return true;
}

struct section *
find_section (const char *n, struct outbuf *ob)
{
  // given sections are alphabetically sorted, we can do
  // a binary search over the length of the array
  size_t max = ob->sz;
  size_t min = 0;
  size_t middle = (max + min) / 2;
  
  while (min <= max)
    {
      int v = strcmp(n, ob->scs[middle]->name);
      if (v < 0)
        min = middle + 1;
      else if (v == 0)
        return ob->scs[middle];
      else
        max = middle - 1;

      middle = (max + min) / 2;
    }

  return NULL;
}

void
write_output_buffer (FILE *f, struct outbuf *ob)
{
  assert (ftell(f) != -1);

  for (size_t i = 0; i < ob->sz; i++)
    write_section(f, ob->scs[i]);
}

void
write_output_buffer_to_path (const char *p, struct outbuf *ob)
{
  FILE *f = fopen (p, "w");
  if (!f)
    err_open ("cannot write output buffer");

  write_output_buffer (f, ob);
}
