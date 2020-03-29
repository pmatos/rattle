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

#include "emit.h"

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include "err.h"

///////////////////////////////////////////////////////////////////////
//
//  ASM utilities
//
//
///////////////////////////////////////////////////////////////////////
#define LABEL_MAX 64

void
gen_new_temp_label (char *str)
{
  static size_t lcount = 0;
  const char prefix[] = ".LTrattle";

  sprintf (str, "%s%zu", prefix, lcount++);
}

///////////////////////////////////////////////////////////////////////
//
// Section EMIT_ASM_
//
// The functions in this section are used to emit assembly to a FILE *
//
//
///////////////////////////////////////////////////////////////////////

// Emit assembly for function decorations - prologue and epilogue
void
emit_asm_prologue (FILE *f, const char *name)
{
#if defined(__APPLE__) || defined(__MACH__)
  fprintf (f, "    .section	__TEXT,__text,regular,pure_instructions\n");
  fprintf (f, "    .globl " ASM_SYMBOL_PREFIX "%s\n", name);
  fprintf (f, "    .p2align 4, 0x90\n");
  fprintf (f, ASM_SYMBOL_PREFIX "%s:\n", name);
#elif defined(__linux__)
  fprintf (f, "    .text\n");
  fprintf (f, "    .globl " ASM_SYMBOL_PREFIX "%s\n", name);
  fprintf (f, "    .type " ASM_SYMBOL_PREFIX "%s, @function\n", name);
  fprintf (f, ASM_SYMBOL_PREFIX "%s:\n", name);
#endif
}

void
emit_asm_epilogue (FILE *f)
{
  fprintf (f, "    ret\n");
}

void
emit_asm_identifier (FILE *f, schptr_t sptr, env_t *env)
{
  schid_t *id = (schid_t *) sptr;
  assert (id->type == SCH_ID);

  size_t si;

  // get the stack offset where the value of id is.
  // issue a load from that stack location to obtain it's value and put it in rax
  if (env_ref (id, env, &si))
    fprintf (f, "    movq   -%zu(%%rsp), %%rax\n", si);
  else
    {
      fprintf (stderr, "undefined variable: %s\n", id->name);
      exit (EXIT_FAILURE);
    }
}

// EMIT_ASM_IMM
// Emit assembly for immediates
void
emit_asm_imm (FILE *f, schptr_t imm)
{
  if (imm > 4294967295)
    fprintf (f, "    movabsq $%" PRIu64 ", %%rax\n", (uint64_t)imm);
  else
    fprintf (f, "    movl $%" PRIu64 ", %%eax\n", (uint64_t)imm);
}

void
emit_asm_prim (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval_t *pe = (schprim_eval_t *)sptr;
  pe->prim->emitter (f, sptr, si, env);
}

void
emit_asm_expr (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  if (sch_imm_p (sptr))
    {
      emit_asm_imm (f, sptr);
      return;
    }

  assert (sch_ptr_p (sptr));

  sch_type type = *((sch_type *)sptr);
  switch (type)
    {
    case SCH_PRIM:
      fprintf (stderr, "cannot emit singleton primitive types\n");
      exit (EXIT_FAILURE);
      break;
    case SCH_PRIM_EVAL1:
    case SCH_PRIM_EVAL2:
      emit_asm_prim (f, sptr, si, env);
      break;
    case SCH_IF:
      emit_asm_if (f, sptr, si, env);
      break;
    case SCH_ID:
      emit_asm_identifier (f, sptr, env);
      break;
    case SCH_LET:
      emit_asm_let (f, sptr, si, env);
      break;
    case SCH_EXPR_SEQ:
      emit_asm_expr_seq (f, sptr, si, env);
      break;
    default:
      fprintf (stderr, "unknown type 0x%08x\n", type);
      err_unreachable ("unknown type");
      break;
    }
}

// Primitives Emitter
void
emit_asm_prim_fxadd1 (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  const uint64_t cst = UINT64_C(1) << FX_SHIFT;
  fprintf (f, "    addq $%" PRIu64", %%rax\n", cst);
}

void
emit_asm_prim_fxsub1 (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  const uint64_t cst = UINT64_C(1) << FX_SHIFT;
  fprintf (f, "    subq $%" PRIu64 ", %%rax\n", cst);
}

void
emit_asm_prim_fxzerop (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  fprintf (f, "    movl   $%" PRIu64 ", %%edx\n", FALSE_CST);
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", FX_TAG);
  fprintf (f, "    movabsq $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne %%rdx, %%rax\n");
}

void
emit_asm_prim_char_to_fixnum (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fixnum_to_char (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", CHAR_TAG);
}

void
emit_asm_prim_fixnump (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    andq   $%" PRIu64 ", %%rax\n", FX_MASK);
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", FX_TAG);
  fprintf (f, "    sete   %%al\n");
  fprintf (f, "    movzbl %%al, %%eax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", BOOL_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", BOOL_TAG);
}

void
emit_asm_prim_booleanp (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    andq   $%" PRIu64 ", %%rax\n", BOOL_MASK);
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", BOOL_TAG);
  fprintf (f, "    sete   %%al\n");
  fprintf (f, "    salq   $%" PRIu8", %%rax\n", BOOL_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", BOOL_TAG);
}

// Not will return #t for #f and #f for anything else,
// because all values evaluate to #t
// section 6.3 of r7rs:
//     "Of all the Scheme values, only#fcounts as false
//      in condi-tional expressions.  All other Scheme
//      values, including#t,count as true."
void
emit_asm_prim_not (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // I *don't* think this one can be optimized by fixing the values
  fprintf (f, "    movq    $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    cmpq    $%" PRIu64 ", %%rax\n", FALSE_CST);
  fprintf (f, "    movabsq $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne  %%rdx, %%rax\n");
}

void
emit_asm_prim_charp (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    andq   $%" PRIu64 ", %%rax\n", CHAR_MASK);
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", CHAR_TAG);
  fprintf (f, "    sete   %%al\n");
  fprintf (f, "    movzbl %%al, %%eax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", BOOL_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", BOOL_TAG);
}
void
emit_asm_prim_nullp (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // I *don't* think this one can be optimized by fixing the values
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", NULL_CST);
  fprintf (f, "    sete   %%al\n");
  fprintf (f, "    movzbl %%al, %%eax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", BOOL_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", BOOL_TAG);
}

void
emit_asm_prim_fxlognot (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si, env);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    notq   %%rax\n");
  fprintf (f, "    andq   $%" PRIu64 ", %%rax\n", ~FX_MASK);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fxadd (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    xorq   $%" PRIu64 ", %%rax\n", FX_MASK);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    addq   -%zu(%%rsp), %%rax\n", si);
}

void
emit_asm_prim_fxsub (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    movq   %%rax, %%r8\n");
  fprintf (f, "    movq   -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    subq   %%r8, %%rax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fxmul (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    imulq  -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fxlogand (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    andq   -%zu(%%rsp), %%rax\n", si);
}

void
emit_asm_prim_fxlogor (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    orq   -%zu(%%rsp), %%rax\n", si);
}

void
emit_asm_prim_fxeq (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne    %%rdx, %%rax\n");
}

void
emit_asm_prim_fxlt (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovle    %%rdx, %%rax\n");
}

void
emit_asm_prim_fxle (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovl     %%rdx, %%rax\n");
}

void
emit_asm_prim_fxgt (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovge    %%rdx, %%rax\n");
}

void
emit_asm_prim_fxge (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovg     %%rdx, %%rax\n");
}

void
emit_asm_prim_chareq (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne    %%rdx, %%rax\n");
}

void
emit_asm_prim_charlt (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", CHAR_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovle    %%rdx, %%rax\n");
}

void
emit_asm_prim_charle (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", CHAR_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovl     %%rdx, %%rax\n");
}

void
emit_asm_prim_chargt (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", CHAR_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovge    %%rdx, %%rax\n");
}

void
emit_asm_prim_charge (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si, env);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES, env);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", CHAR_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovg     %%rdx, %%rax\n");
}

void
emit_asm_label (FILE *f, char *label)
{
  fprintf (f, "%s:\n", label);
}

// Emitting asm for conditional
void
emit_asm_if (FILE *f, schptr_t p, size_t si, env_t *env)
{
  schif_t *pif = (schif_t *) p;
  assert (pif->type == SCH_IF);

  char elsel[LABEL_MAX];
  gen_new_temp_label (elsel);

  char endl[LABEL_MAX];
  gen_new_temp_label (endl);


  emit_asm_expr (f, pif->condition, si, env);

  // Check if boolean value is true of false and jump accordingly
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", FALSE_CST);
  fprintf (f, "    je     %s\n", elsel);
  emit_asm_expr (f, pif->thenv, si, env);
  fprintf (f, "    jmp    %s\n", endl);
  emit_asm_label (f, elsel);
  emit_asm_expr (f, pif->elsev, si, env);
  emit_asm_label (f, endl);
}

void
emit_asm_let (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  // We have to evaluate all let bindings right hand sides.
  // Add definitions for all of them into an environment and
  // emit code for the body of the let with the environment properly
  // filled.
  schlet_t *let = (schlet_t *) sptr;
  assert (let->type == SCH_LET);

  // Evaluate each of the bindings in the bindings list
  env_t *nenv = env;
  size_t freesi = si; // currently free si
  for (binding_spec_list_t *bs = let->bindings; bs != NULL; bs = bs->next)
    {
      emit_asm_expr (f, bs->expr, freesi, let->star_p ? nenv : env);

      fprintf (f, "    movq %%rax, -%zu(%%rsp)\n", freesi);

      nenv = env_add (bs->id, freesi, nenv);
      freesi += WORD_BYTES;
    }

  emit_asm_expr (f, let->body, freesi, nenv);
  free_env_partial (nenv, env, /*shallow=*/true);
}

void
emit_asm_expr_seq (FILE *f, schptr_t sptr, size_t si, env_t *env)
{
  schexprseq_t *seq = (schexprseq_t *) sptr;
  assert (seq->type == SCH_EXPR_SEQ);

  expression_list_t *s = seq->seq;

  for (; s; s = s->next)
    emit_asm_expr (f, s->expr, si, env);
}
