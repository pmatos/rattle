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

#include <stdint.h>

#include "common.h"

///////////////////////////////////////////////////////////////////////
//
// Section Scheme Values
//
//
///////////////////////////////////////////////////////////////////////

// Immediates are just represented by a single 64bit value
typedef uint64_t sch_imm;

// Primitives
struct schprim;
struct env;
typedef void (*prim_emmiter) (FILE *, schptr_t, size_t, struct env *);

typedef enum { SCH_PRIM, SCH_IF, SCH_ID, SCH_LET, SCH_EXPR_SEQ, SCH_PRIM_EVAL1, SCH_PRIM_EVAL2 } sch_type;

typedef struct schtype
{
  sch_type type;
} schtype_t;

typedef struct schprim
{
  sch_type type;         // Type (always SCH_PRIM)
  char *name;            // Primitive name
  unsigned int argcount; // Number of arguments for the primitive
  prim_emmiter emitter;  // Primitive function emmiter
} schprim_t;

typedef struct schprim_eval
{
  sch_type type;
  const schprim_t *prim;
} schprim_eval_t;

typedef struct schprim_eval1
{
  struct schprim_eval;
  schptr_t arg1;
} schprim_eval1_t;

typedef struct schprim_eval2
{
  struct schprim_eval;
  schptr_t arg1;
  schptr_t arg2;
} schprim_eval2_t;

typedef struct schif
{
  sch_type type;
  schptr_t condition; // if conditional
  schptr_t thenv;     // then value
  schptr_t elsev;     // else value
} schif_t;

typedef struct schid
{
  sch_type type;
  char *name;
} schid_t;

typedef struct binding_spec_list
{
  schid_t *id;
  schptr_t expr;
  struct binding_spec_list *next;
} binding_spec_list_t;

typedef struct schlet
{
  sch_type type;
  binding_spec_list_t *bindings;
  schptr_t body;
} schlet_t;

typedef struct expression_list
{
  schptr_t expr;
  struct expression_list *next;
} expression_list_t;

typedef struct schexprseq
{
  sch_type type;
  expression_list_t *seq;
} schexprseq_t;

void free_expression (schptr_t);
void free_expression_list (expression_list_t *);
void free_binding_spec_list (binding_spec_list_t *);
