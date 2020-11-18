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

#include "err.h"

#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////////////////////////////////////////
//
// Section Error Management
//
// Error management - this should be the only section causing
// the program to exit
//
///////////////////////////////////////////////////////////////////////

void
err_oom (void)
{
  fprintf (stderr, "out of memory\n");
  exit (EXIT_FAILURE);
}

void
err_parse (const char *s)
{
  fprintf (stderr, "error: cannot parse `%s'\n", s);
  exit (EXIT_FAILURE);
}

__attribute__ ((noreturn)) void
err_unreachable (const char *s)
{
  fprintf (stderr, "error: unreachable - `%s'\n", s);
  exit (EXIT_FAILURE);
}
