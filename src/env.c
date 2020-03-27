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

#include "env.h"

#include "memory.h"
#include "string.h"

env_t *make_env ()
{
  return NULL;
}

env_t *
env_add (schid_t *id, size_t si, env_t *env)
{
  env_t *e = alloc (sizeof (*e));
  e->id = id;
  e->si = si;
  e->next = env;
  return e;
}

env_t *
env_append (env_t *env1, env_t *env2)
{
  if (!env1)
    return env2;

  env_t *ptr = env1;
  for (; ptr->next != NULL; ptr = ptr->next);
  ptr->next = env2;
  return env1;
}

bool
env_ref (schid_t *id, env_t *env, size_t *si)
{
  for (env_t *e = env; e; e = e->next)
    {
      if (!strcmp (id->name, e->id->name))
        {
          *si = e->si;
          return true;
        }
    }
  return false;
}
