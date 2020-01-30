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
inline bool
arch_32_p (void)
{
  return sizeof (size_t) == 4;
}

inline bool
arch_64_p (void)
{
  return sizeof (size_t) == 8;
}

// This header contains common declaration to be used by both the
// compiler and the runtime.

#define FIXNUM_MIN -2305843009213693952LL
#define FIXNUM_MAX  2305843009213693951LL

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
//
// For example:
// Non-Immediate: 0xY0 - 0b 0000 0000 for 64bit
//                     - 0b xxxx x000 for 32bit
//
// Fixnum:        0x01 - 0b 0000 0001
// Null:          0x3e - 0b 0011 1110
// Boolean True:  0x2e - 0b 0110 1110
// Boolean False: 0x6e - 0b 0010 1110
// Character:     0x0e - 0b 0000 1110

typedef uint64_t schptr_t;

//
// Predicates
//

inline bool
sch_imm_p (schptr_t sptr)
{
  return (sptr & 0x07) ? true : false;
}

inline bool
sch_imm_fixnum_p (schptr_t sptr)
{
  return (sptr & 0x01) ? true : false;
}

inline bool
sch_imm_null_p (schptr_t sptr)
{
  return (sptr & 0xff) == 0x3e;
}

inline bool
sch_imm_boolean_p (schptr_t sptr)
{
  return (sptr & 0x3f) == 0x2e;
}

inline bool
sch_imm_true_p (schptr_t sptr)
{
  return sch_imm_boolean_p (sptr) && (sptr >> 6);
}

inline bool
sch_imm_false_p (schptr_t sptr)
{
  return !sch_imm_true_p (sptr);
}

// Encoding

inline schptr_t
sch_encode_ptr (void *ptr)
{
  assert (arch_32_p () ?
          ((uintptr_t)ptr & 0x7) == 0 :
          ((uintptr_t)ptr & 0xf) == 0);
  return (schptr_t) ptr;
}

inline schptr_t
sch_encode_fixnum (int64_t fx)
{
  assert (fx < FIXNUM_MAX);
  assert (fx > FIXNUM_MIN);
  return (fx << 1) | 0x1;
}

inline schptr_t
sch_encode_boolean (int64_t fx)
{
  
