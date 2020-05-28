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

#include "section.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Implements an output buffer
// All of the generated assembly is emitted to the buffer and in the end
// the buffer is written to disk

struct outbuf
{
  struct section **scs;  // sections
  size_t sz;             // size (number of sections in buffer)
  size_t cap;            // capacity (once sz == cap, we need to reallocate it)
};
#define OUTBUF_INITIAL_CAP 256

struct outbuf *make_output_buffer (void);
void free_output_buffer (void);
bool output_buffer_add_section (char *, struct outbuf *);
struct section *find_section (char *, struct outbuf *);
void write_output_buffer (FILE *);
