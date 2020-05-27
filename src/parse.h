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

#include "structs.h"

///////////////////////////////////////////////////////////////////////
//
//  Section Parsing
//
//
///////////////////////////////////////////////////////////////////////

// Main parsing procedures

bool parse_program (const char **, schptr_t *);
bool parse_identifier (const char **, schptr_t *);
bool parse_prim (const char **, schptr_t *);
bool parse_prim1 (const char **, schptr_t *);
bool parse_prim2 (const char **, schptr_t *);
bool parse_expression (const char **, schptr_t *);
bool parse_imm (const char **, schptr_t *);
bool parse_imm_bool (const char **, schptr_t *);
bool parse_imm_null (const char **, schptr_t *);
bool parse_imm_fixnum (const char **, schptr_t *);
bool parse_imm_char (const char **, schptr_t *);
bool parse_if (const char **, schptr_t *);
bool parse_procedure_call (const char **, schptr_t *);
bool parse_lambda_expression (const char **, schptr_t *);

// Helper parsing procedures

bool parse_initial (const char **);
bool parse_letter (const char **);
bool parse_special_initial (const char **);
bool parse_subsequent (const char **);
bool parse_digit (const char **);

bool parse_char (const char **, char);
bool parse_char_sequence (const char **, const char *);
bool parse_lparen (const char **);
bool parse_rparen (const char **);
bool parse_whitespace (const char **);

