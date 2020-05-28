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
#pragma once

#include <stdlib.h>

struct section
{
  char *name;
  char **insns;
  size_t sz;
  size_t cap;
};
#define SECTION_INITIAL_CAP 128
#define SECTION_CAP_MULTIPLIER 1.2

struct section *make_section (char *);
void free_section (struct section *);
void section_grow (struct section *);
void emit_insn (char *, struct section *);
void write_section (FILE *, struct section *);
