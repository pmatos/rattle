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
#include "memory.h"

#include <stdlib.h>

#include "err.h"

///////////////////////////////////////////////////////////////////////
//
// Section Memory Management
//
// Memory management - this should be the only section causing the
// program to allocate memory
//
///////////////////////////////////////////////////////////////////////

// alloc: allocates n bytes of memory and returns a pointer to it
void *
alloc (size_t n)
{
  void *mem = malloc (n);
  if (!mem)
    err_oom ();

  return mem;
}

// grow: grows region pointed to by ptr to n bytes
void *
grow (void *ptr, size_t n)
{
  void *mem = realloc (ptr, n);
  if (!mem)
    err_oom ();

  return mem;
}
