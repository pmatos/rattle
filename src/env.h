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

#include "structs.h"

///////////////////////////////////////////////////////////////////////
//
//  Environment manipulation
//
//
///////////////////////////////////////////////////////////////////////

typedef struct env
{
  schid_t *id;
  size_t si;
  struct env *next;
} env_t;

env_t *make_env ();
env_t *env_add (schid_t *, size_t, env_t *);
env_t *env_append (env_t *, env_t *);
bool env_ref (schid_t *, env_t *, size_t *);



