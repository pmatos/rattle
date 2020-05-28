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

#define GET_TYPE(x) ((schtype_t *)x)->type

// Immediates are just represented by a single 64bit value
typedef uint64_t sch_imm;

// Primitives
struct schprim;
struct env;
typedef void (*prim_emmiter) (FILE *, schptr_t, size_t, struct env *);

typedef enum { SCH_PRIM, SCH_IF, SCH_ID, SCH_LET, SCH_EXPR_SEQ, SCH_PRIM_EVAL1, SCH_PRIM_EVAL2, SCH_LAMBDA } sch_type;
typedef enum { LET, LETS, LETREC } let_t;

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

typedef struct identifier_list
{
  schid_t *id;
  struct identifier_list *next;
} identifier_list_t;

typedef struct binding_spec_list
{
  schid_t *id;
  schptr_t expr;
  struct binding_spec_list *next;
} binding_spec_list_t;

typedef struct schlet
{
  sch_type type;
  let_t let;
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

typedef enum { FORMALS_NORMAL, FORMALS_REST, FORMALS_LIST } lambda_formals_type;
typedef struct lambda_formals_base
{
  lambda_formals_type type;
} lambda_formals_t;

typedef struct lambda_formals_normal
{
  struct lambda_formals_base;
  identifier_list_t *args;
} lambda_formals_normal_t;

typedef struct lambda_formals_rest
{
  struct lambda_formals_base;
  identifier_list_t *args;
  schid_t *rest;
} lambda_formals_rest_t;

typedef struct lambda_formals_list
{
  struct lambda_formals_base;
  schid_t *listid;
} lambda_formals_list_t;

typedef struct schlambda
{
  sch_type type;
  char *label;
  lambda_formals_t *formals;
  schptr_t body;
} schlambda_t;

void free_expression (schptr_t);
void free_expression_list (expression_list_t *);
void free_identifier (schid_t *);
void free_identifier_list (identifier_list_t *);
void free_binding_spec_list (binding_spec_list_t *);
void free_lambda (schlambda_t *);
void free_lambda_formals (lambda_formals_t *);
schid_t *clone_schid (const schid_t *);

//
// Lambda
//
static inline bool
sch_lambda_p (schptr_t sptr)
{
  return sch_ptr_p (sptr) && GET_TYPE(sptr) == SCH_LAMBDA;
}
