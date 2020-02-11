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
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

//
// Platform
//
static inline bool
arch_32_p (void)
{
  return sizeof (size_t) == 4;
}

static inline bool
arch_64_p (void)
{
  return sizeof (size_t) == 8;
}

// malloc returns pointers that are always alignof(max_align_t) aligned.
// Which means we can distinguish integers that are pointers from
// integers that are not and can represent immediates.
// In a 64 bit system, malloc returns pointers that are 16bits aligned (last 4 bits are zero).
// In a 32 bit system, malloc returns pointers that are 8bits aligned (last 3 bits are zero).
//
// If we assume 8bits the smallest alignment on any system nowadays,
// then we know all pointers will have the lowest 8 bits at zero.
// We just need a non-zero tag for all immediates that live on those
// bits.

/////////////////////////////////////////////////////
// Generated by ptrtags.rkt
// bitwidth: 64, tag bitwidth: 4, pointer mask: 0x7
#define PTR_TAG UINT64_C(0)
#define PTR_MASK UINT64_C(0x7)
#define PTR_SHIFT UINT8_C(0)
//
#define FX_TAG UINT64_C(0x1)
#define FX_MASK UINT64_C(0x1)
#define FX_SHIFT UINT8_C(1)
#define FX_MAX INT64_C(4611686018427387903)
#define FX_MIN INT64_C(-4611686018427387904)
//
#define CHAR_TAG UINT64_C(0x2)
#define CHAR_MASK UINT64_C(0x3)
#define CHAR_SHIFT UINT8_C(2)
//
#define BOOL_TAG UINT64_C(0x4)
#define BOOL_MASK UINT64_C(0xf)
#define BOOL_SHIFT UINT8_C(34)
//
#define NULL_CST UINT64_C(0xc)
#define TRUE_CST UINT64_C(0x400000004)
#define FALSE_CST UINT64_C(0x4)
//
/////////////////////////////////////////////////////

// This header contains common declaration to be used by both the
// compiler and the runtime.

typedef uintptr_t schptr_t;

//
// Pointers
//

static inline bool
sch_ptr_p (schptr_t sptr)
{
  return (sptr & PTR_MASK) == PTR_TAG;
}

//
// Immediates
//

static inline bool
sch_imm_p (schptr_t sptr)
{
  return !sch_ptr_p (sptr);
}

//
// Fixnum
//
static inline bool
sch_imm_fixnum_p (schptr_t sptr)
{
  return (sptr & FX_MASK) == FX_TAG;
}

static inline schptr_t
sch_encode_imm_fixnum (int64_t fx)
{
  assert (fx <= FX_MAX);
  assert (fx >= FX_MIN);

  return (((uint64_t)fx) << FX_SHIFT) | FX_TAG;
}

static inline int64_t
sch_decode_imm_fixnum (schptr_t sptr)
{
  // arithmetic right shift from hackers delight
  // gcc does signed shift but it's implementation dependent
  // so until we determine what the compiler uses we leave this implementation here
  const uint64_t t = -(sptr >> 63);
  return ((sptr ^ t) >> FX_SHIFT) ^ t;
}

//
// Null
//

static inline bool
sch_imm_null_p (schptr_t sptr)
{
  return sptr == NULL_CST;
}

static inline schptr_t
sch_encode_imm_null (void)
{
  return NULL_CST;
}


//
// Bool
//
static inline bool
sch_imm_bool_p (schptr_t sptr)
{
  return (sptr & BOOL_MASK) == BOOL_TAG;
}

static inline schptr_t
sch_encode_imm_bool (bool b)
{
  return ((schptr_t)b << BOOL_SHIFT) | BOOL_TAG;
}

static inline bool
sch_decode_imm_bool (schptr_t sptr)
{
  return sptr >> BOOL_SHIFT;
}

static inline bool
sch_imm_false_p (schptr_t sptr)
{
  return sptr == FALSE_CST;
}

static inline bool
sch_imm_true_p (schptr_t sptr)
{
  return sptr == TRUE_CST;
}

//
// Characters
//

static inline bool
sch_imm_char_p (schptr_t sptr)
{
  return (sptr & CHAR_MASK) == CHAR_TAG;
}

static inline schptr_t
sch_encode_imm_char (unsigned char c)
{
  return (c << CHAR_SHIFT) | CHAR_TAG;
}

static inline unsigned char
sch_decode_imm_char (schptr_t sptr)
{
  return sptr >> CHAR_SHIFT;
}
