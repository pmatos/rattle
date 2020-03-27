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

#include "emit.h"

// Order matter
static const schprim_t primitives[] =
  { { SCH_PRIM, "fxadd1", 1, emit_asm_prim_fxadd1 },
    { SCH_PRIM, "fxsub1", 1, emit_asm_prim_fxsub1 },
    { SCH_PRIM, "fxzero?", 1, emit_asm_prim_fxzerop },
    { SCH_PRIM, "char->fixnum", 1, emit_asm_prim_char_to_fixnum },
    { SCH_PRIM, "fixnum->char", 1, emit_asm_prim_fixnum_to_char },
    { SCH_PRIM, "null?", 1, emit_asm_prim_nullp },
    { SCH_PRIM, "not", 1, emit_asm_prim_not },
    { SCH_PRIM, "fixnum?", 1, emit_asm_prim_fixnump },
    { SCH_PRIM, "boolean?", 1, emit_asm_prim_booleanp },
    { SCH_PRIM, "char?", 1, emit_asm_prim_charp },
    { SCH_PRIM, "fxlognot", 1, emit_asm_prim_fxlognot },
    { SCH_PRIM, "fx+", 2, emit_asm_prim_fxadd },
    { SCH_PRIM, "fx-", 2, emit_asm_prim_fxsub },
    { SCH_PRIM, "fx*", 2, emit_asm_prim_fxmul },
    { SCH_PRIM, "fxlogand", 2, emit_asm_prim_fxlogand },
    { SCH_PRIM, "fxlogor", 2, emit_asm_prim_fxlogor },
    { SCH_PRIM, "fx=", 2, emit_asm_prim_fxeq },
    { SCH_PRIM, "fx<=", 2, emit_asm_prim_fxle },
    { SCH_PRIM, "fx<", 2, emit_asm_prim_fxlt },
    { SCH_PRIM, "fx>=", 2, emit_asm_prim_fxge },
    { SCH_PRIM, "fx>", 2, emit_asm_prim_fxgt },
    { SCH_PRIM, "char=", 2, emit_asm_prim_chareq },
    { SCH_PRIM, "char<=", 2, emit_asm_prim_charle },
    { SCH_PRIM, "char<", 2, emit_asm_prim_charlt },
    { SCH_PRIM, "char>=", 2, emit_asm_prim_charge },
    { SCH_PRIM, "char>", 2, emit_asm_prim_chargt }
  };
static const size_t primitives_count = sizeof(primitives)/sizeof(primitives[0]);
