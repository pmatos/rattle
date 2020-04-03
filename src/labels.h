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

#if defined(__APPLE__) || defined(__MACH__)
#  define ASM_SYMBOL_PREFIX "_"
#elif defined(__linux__)
#  define ASM_SYMBOL_PREFIX ""
#else
#  error "Unsupported platform"
#endif

#define LABEL_MAX 64

void gen_new_temp_label (char *);
char *gen_new_label (void) __attribute__((malloc));
