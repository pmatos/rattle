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

#include "section.h"

#include "memory.h"

#include <assert.h>

// Section functions

// make_section (char *n)
//
// Creates a section with name `n`.
// _Note_: After the call to `make_section`, the memory allocated to by n
// belongs to the management functions of the newly created section.
struct section *
make_section (char *n)
{
  struct section *s = alloc (sizeof *s);
  s->name = n;
  s->insns = NULL;
  s->sz = 0;
  s->cap = SECTION_INITIAL_CAP;

  return s;
}

// free_section (struct section *s)
// Frees the memory allocated to `s`.
void
free_section (struct section *s)
{
  if (!s)
    return;

  free (s->name);
  for (size_t i = 0; i < s->sz; i++)
    free (s->insn[i]);
  free (s);
}

// section_grow (struct section *s)
// Grows capacity of section `s`.
void
section_grow (struct section *s)
{
  s->cap *= SECTION_CAP_MULTIPLIER;
  s->insns = (char **) grow (s->insns, s->cap);
}

// emit_insn (char *insn, struct section *s)
//
// Emits an instruction `insn` into section `s`, appending it to the existing
// instructions in the section.
// _Note_: After the call to `emit_insn`, the memory allocated to by `insn`
// belongs to the management functions of section `s`.
void
emit_insn (char *insn, struct section *s)
{
  if (s->sz == s->cap - 1)
    section_grow (s);

  s->insns[s->sz++] = insn;
}

// write_section (FILE *f, struct section *s)
//
// Writes section `s` to file `f`.
// _Note_: Expects an open file.
void
write_section (FILE *f, struct section *s)
{
  assert (ftell (f) != -1);

  for (size_t i = 0; i < s->sz; i++)
    fprintf (f, "%s\n", s->insns[i]);
}


