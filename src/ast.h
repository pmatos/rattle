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

typedef enum
{
  SCH_PRIM,
  SCH_IF,
  SCH_ID,
  SCH_LET,
  SCH_EXPR_SEQ,
  SCH_PRIM_EVAL1,
  SCH_PRIM_EVAL2
} sch_type;

struct schtype
{
  sch_type type;
};

struct schprim
{
  sch_type type;         // Type (always SCH_PRIM)
  char *name;            // Primitive name
  unsigned int argcount; // Number of arguments for the primitive
  prim_emmiter emitter;  // Primitive function emmiter
};

struct schprim_eval
{
  sch_type type;
  const struct schprim *prim;
};

struct schprim_eval1
{
  struct schprim_eval;
  schptr_t arg1;
};

struct schprim_eval2
{
  struct schprim_eval;
  schptr_t arg1;
  schptr_t arg2;
};

struct schif
{
  sch_type type;
  schptr_t condition; // if conditional
  schptr_t thenv;     // then value
  schptr_t elsev;     // else value
};

struct schid
{
  sch_type type;
  char *name;
};

struct binding_spec_list
{
  struct schid *id;
  schptr_t expr;
  struct binding_spec_list *next;
};

struct schlet
{
  sch_type type;
  bool star_p;
  struct binding_spec_list *bindings;
  schptr_t body;
};

struct expression_list
{
  schptr_t expr;
  struct expression_list *next;
};

struct ast_schexprseq
{
  sch_type type;
  struct expression_list *seq;
};

void free_ast_expression (schptr_t);
void free_ast_expression_list (struct expression_list *);
void free_ast_identifier (struct schid *);
void free_ast_binding_spec_list (struct binding_spec_list *);

struct schid *clone_ast_schid (const struct schid *);
