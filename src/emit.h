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

#include <stdio.h>

#include "env.h"

#if defined(__APPLE__) || defined(__MACH__)
#define ASM_SYMBOL_PREFIX "_"
#elif defined(__linux__)
#define ASM_SYMBOL_PREFIX ""
#else
#error "Unsupported platform"
#endif

// Primitive emitter prototypes
void emit_asm_expr (FILE *, schptr_t, size_t, env_t *);
void emit_asm_epilogue (FILE *);
void emit_asm_prologue (FILE *, const char *);
void emit_asm_imm (FILE *, schptr_t);
void emit_asm_if (FILE *, schptr_t, size_t, env_t *);
void emit_asm_let (FILE *, schptr_t, size_t, env_t *);
void emit_asm_expr_seq (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxadd1 (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxsub1 (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxzerop (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_char_to_fixnum (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fixnum_to_char (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_nullp (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fixnump (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_booleanp (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_charp (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_not (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlognot (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxadd (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxsub (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxmul (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlogand (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlogor (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxeq (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlt (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxle (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxgt (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxge (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_chareq (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_charlt (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_charle (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_chargt (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_charge (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_cons (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_pairp (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_car (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_cdr (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_set_car (FILE *, schptr_t, size_t, env_t *);
void emit_asm_prim_set_cdr (FILE *, schptr_t, size_t, env_t *);
