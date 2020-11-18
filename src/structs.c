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
#include "structs.h"

#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "memory.h"

void
free_identifier (schid_t *id)
{
  free (id->name);
  free (id);
}

void
free_if (schif_t *e)
{
  free_expression (e->condition);
  free_expression (e->thenv);
  free_expression (e->elsev);
  free (e);
}

void
free_let (schlet_t *e)
{
  free_binding_spec_list (e->bindings);
  free_expression (e->body);
  free (e);
}

void
free_expression_list (expression_list_t *e)
{
  expression_list_t *tmp = NULL;
  while (e)
    {
      tmp = e->next;
      free_expression (e->expr);
      free (e);
      e = tmp;
    }
}

void
free_expr_seq (schexprseq_t *e)
{
  free_expression_list (e->seq);
  free (e);
}

void
free_prim_eval1 (schprim_eval1_t *e)
{
  free_expression (e->arg1);
  free (e);
}

void
free_prim_eval2 (schprim_eval2_t *e)
{
  free_expression (e->arg1);
  free_expression (e->arg2);
  free (e);
}

#define SCHTYPE(e) (((schtype_t *)e)->type)

void
free_expression (schptr_t e)
{
  if (sch_imm_p (e))
    return;

  switch (SCHTYPE (e))
    {
    case SCH_PRIM:
      // these are statically allocated and don't need to be freed
      break;

    case SCH_IF:
      free_if ((schif_t *)e);
      break;

    case SCH_ID:
      free_identifier ((schid_t *)e);
      break;

    case SCH_LET:
      free_let ((schlet_t *)e);
      break;

    case SCH_EXPR_SEQ:
      free_expr_seq ((schexprseq_t *)e);
      break;

    case SCH_PRIM_EVAL1:
      free_prim_eval1 ((schprim_eval1_t *)e);
      break;

    case SCH_PRIM_EVAL2:
      free_prim_eval2 ((schprim_eval2_t *)e);
      break;

    default:
      err_unreachable ("unknown type");
    }
}

void
free_binding_spec_list (binding_spec_list_t *bs)
{
  binding_spec_list_t *tmp = NULL;
  while (bs)
    {
      tmp = bs->next;
      free_identifier (bs->id);
      free_expression (bs->expr);
      free (bs);
      bs = tmp;
    }
}

schid_t *
clone_schid (const schid_t *id)
{
  schid_t *clone = alloc (sizeof *clone);
  clone->name = strdup (id->name);
  return clone;
}
