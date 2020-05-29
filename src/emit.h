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
#include "outbuf.h"

// Primitive emitter prototypes
void emit_asm_expr (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_epilogue (struct outbuf *);
void emit_asm_prologue (struct outbuf *, const char *);
void emit_asm_imm (struct outbuf *, schptr_t);
void emit_asm_if (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_let (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_expr_seq (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxadd1 (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxsub1 (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxzerop (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_char_to_fixnum (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fixnum_to_char (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_nullp (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fixnump (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_booleanp (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_charp (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_not (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlognot (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxadd (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxsub (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxmul (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlogand (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlogor (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxeq (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxlt (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxle (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxgt (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_fxge (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_chareq (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_charlt (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_charle (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_chargt (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_prim_charge (struct outbuf *, schptr_t, size_t, env_t *);
void emit_asm_lambda (struct outbuf *, schptr_t, size_t, env_t *);
