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
// Null:          0x3f - 0b 0011 1110
// Boolean True:  0x2f - 0b 0010 1110
// Boolean False: 0x6f - 0b 0110 1110
// Character:     0x0f - 0b 0000 1110
// TODO ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

