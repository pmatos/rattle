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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>

#include "common.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

static const char *CC = "/usr/bin/cc";

// Compiler main entry point file
void __attribute__((noreturn))
usage (const char *prog)  {
  fprintf(stderr, "rattle version %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
  fprintf(stderr, "Usage: %s [-hec] [expression ...]\n", prog);
  exit(EXIT_FAILURE);
}

void __attribute__((noreturn))
help (const char *prog) {
  usage(prog);
}

// Prototypes
void evaluate(const char *);
void compile(const char *, const char *);
void compile_expression (const char *);

// TODO find correct posix value
#define FILE_PATH_MAX 1024

// Option globals
static bool dump_p = false;
static bool save_temps_p = false;

int
main (int argc, char *argv[]) {

  bool compile_p = false;
  bool evaluate_p = false;
  char input[FILE_PATH_MAX];
  char output[FILE_PATH_MAX];

  int opt;
  while ((opt = getopt(argc, argv, "hdsec:o:")) != -1)
    {
      switch (opt)
        {
        case 'h':
          help (argv[0]);
          break;
        case 'd':
          dump_p = true;
          break;
        case 's':
          save_temps_p = true;
          break;
        case 'c':
          compile_p = true;
          strcpy (input, optarg);
          break;
        case 'o':
          strcpy (output, optarg);
          break;
        case 'e':
          evaluate_p = true;
          break;
        case ':':
          fprintf (stderr, "flag missing operand\n");
          usage (argv[0]);
          break;
        case '?':
          fprintf (stderr, "unrecognized option: '-%c'\n", optopt);
          usage (argv[0]);
          break;
        default:
          usage (argv[0]);
          break;
        }
    }

  if (evaluate_p && compile_p)
    {
      fprintf (stderr, "cannot specify both -e and -c\n");
      usage (argv[0]);
    }

  if (evaluate_p)
    {
      int ncommands = argc - optind;
      if (ncommands > 1)
        {
          fprintf (stderr, "too many arguments\n");
          exit (EXIT_FAILURE);
        }

      const char *cmd = argv[optind];
      evaluate (cmd);
    }

  if (compile_p)
    compile (input, output);

  return 0;
}

///////////////////////////////////////////////////////////////////////
//
// Section Error Management
//
// Error management - this should be the only section causing
// the program to exit
//
///////////////////////////////////////////////////////////////////////

void
err_oom (void)
{
  fprintf (stderr, "out of memory\n");
  exit (EXIT_FAILURE);
}

void
err_parse (const char *s)
{
  fprintf (stderr, "error: cannot parse `%s'\n", s);
  exit (EXIT_FAILURE);
}

///////////////////////////////////////////////////////////////////////
//
// Section Memory Management
//
// Memory management - this should be the only section causing the
// program to allocate memory
//
///////////////////////////////////////////////////////////////////////

// alloc: allocates n bytes of memory and returns a pointer to it
void *
alloc (size_t n)
{
  void *mem = malloc (n);
  if (!mem)
    err_oom ();

  return mem;
}

// grow: grows region pointed to by ptr to n bytes
void *
grow (void *ptr, size_t n)
{
  void *mem = realloc (ptr, n);
  if (!mem)
    err_oom ();

  return mem;
}

///////////////////////////////////////////////////////////////////////
//
// Section Scheme Values
//
//
///////////////////////////////////////////////////////////////////////

// Immediates are just represented by a single 64bit value
typedef uint64_t sch_imm;

// Primitives
struct schprim;
typedef void (*prim_emmiter) (FILE *, schptr_t, size_t);

typedef enum { SCH_PRIM, SCH_IF, SCH_PRIM_EVAL1, SCH_PRIM_EVAL2 } sch_type;

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
  schprim_t *prim;
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

// Primitive emitter prototypes
void emit_asm_prim_fxadd1 (FILE *, schptr_t, size_t);
void emit_asm_prim_fxsub1 (FILE *, schptr_t, size_t);
void emit_asm_prim_fxzerop (FILE *, schptr_t, size_t);
void emit_asm_prim_char_to_fixnum (FILE *, schptr_t, size_t);
void emit_asm_prim_fixnum_to_char (FILE *, schptr_t, size_t);
void emit_asm_prim_nullp (FILE *, schptr_t, size_t);
void emit_asm_prim_fixnump (FILE *, schptr_t, size_t);
void emit_asm_prim_booleanp (FILE *, schptr_t, size_t);
void emit_asm_prim_charp (FILE *, schptr_t, size_t);
void emit_asm_prim_not (FILE *, schptr_t, size_t);
void emit_asm_prim_fxlognot (FILE *, schptr_t, size_t);
void emit_asm_prim_fxadd (FILE *, schptr_t, size_t);
void emit_asm_prim_fxsub (FILE *, schptr_t, size_t);
void emit_asm_prim_fxmul (FILE *, schptr_t, size_t);
void emit_asm_prim_fxlogand (FILE *, schptr_t, size_t);
void emit_asm_prim_fxlogor (FILE *, schptr_t, size_t);
void emit_asm_prim_fxeq (FILE *, schptr_t, size_t);
void emit_asm_prim_fxlt (FILE *, schptr_t, size_t);
void emit_asm_prim_fxle (FILE *, schptr_t, size_t);
void emit_asm_prim_fxgt (FILE *, schptr_t, size_t);
void emit_asm_prim_fxge (FILE *, schptr_t, size_t);
void emit_asm_prim_chareq (FILE *, schptr_t, size_t);
void emit_asm_prim_charlt (FILE *, schptr_t, size_t);
void emit_asm_prim_charle (FILE *, schptr_t, size_t);
void emit_asm_prim_chargt (FILE *, schptr_t, size_t);
void emit_asm_prim_charge (FILE *, schptr_t, size_t);

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
//  Section Parsing
//
//
///////////////////////////////////////////////////////////////////////

bool parse_prim (const char **, schptr_t *);
bool parse_prim1 (const char **, schptr_t *);
bool parse_prim2 (const char **, schptr_t *);
bool parse_expr (const char **, schptr_t *);
bool parse_imm (const char **, schptr_t *);
bool parse_imm_bool (const char **, schptr_t *);
bool parse_imm_null (const char **, schptr_t *);
bool parse_imm_fixnum (const char **, schptr_t *);
bool parse_imm_char (const char **, schptr_t *);
bool parse_if (const char **, schptr_t *);

bool parse_char (const char **, char);
bool parse_lparen (const char **);
bool parse_rparen (const char **);
bool parse_whitespace (const char **);

bool
parse_whitespace (const char **input)
{
  const char *tmp = *input;
  while (isspace (**input))
    (*input)++;
  return *input != tmp; // return true if whitespace was skipped
}

bool
parse_if (const char **input, schptr_t *sptr)
{
  const char *ptr = *input;
  // Parses an expression as follows:
  // (if <expr> <expr> <expr>)
  if (!parse_lparen (&ptr))
    return false;

  // skip possible whilespace between lparen and if identifier
  (void) parse_whitespace (&ptr);

  if (ptr[0] != 'i' || ptr[1] != 'f')
    return false;
  ptr += 2;

  // skip whitespace between if identifier and condition
  (void) parse_whitespace (&ptr);

  schptr_t condition;
  schptr_t thenv;
  schptr_t elsev;

  if (!parse_expr (&ptr, &condition))
    return false;

  // skip whitespace between condition and then-value
  (void) parse_whitespace (&ptr);

  if (!parse_expr (&ptr, &thenv))
      return false;

  // skip whitespace between then-value and else-value
  (void) parse_whitespace (&ptr);

  if (!parse_expr (&ptr, &elsev))
    return false;

  // skip whitespace between else-value and right paren
  (void) parse_whitespace (&ptr);

  if (! parse_rparen (&ptr))
    return false;

  // We parsed all we wanted now, so prepare return value
  *input = ptr;
  schif_t *ifv = (schif_t *) alloc (sizeof *ifv);
  if (!ifv)
    err_oom ();

  ifv->type = SCH_IF;
  ifv->condition = condition;
  ifv->thenv = thenv;
  ifv->elsev = elsev;

  *sptr = (schptr_t) ifv;
  return true;
}

bool
parse_imm_bool (const char **input, schptr_t *imm)
{
  // TODO: Support #true and #false
  if ((*input)[0] == '#')
    {
      if ((*input)[1] == 't' || (*input)[1] == 'f')
        {
          *imm = sch_encode_imm_bool((*input)[1] == 't');
          *input += 2;
          return true;
        }
    }

  return false;
}

bool
parse_imm_fixnum (const char **input, schptr_t *imm)
{
  const char *sign  = NULL;
  bool seen_num = false;
  const char *ptr = *input;

  if (**input == '+' || **input == '-')
    {
      sign = *input;
      ptr++;
    }
  uint64_t v = 0;
  for (; isdigit(*ptr); ptr++)
    {
      seen_num = true;
      v = (v * 10) + (*ptr - '0');
    }

  if (seen_num)
    {
      int64_t fx =  (int64_t) v;
      if (sign && *sign == '-')
        fx = -v;

      if (fx >= FX_MIN && fx <= FX_MAX)
        {
          *imm = sch_encode_imm_fixnum (fx);
          *input = ptr;
          return true;
        }
    }

  return false;
}

bool
parse_imm_char (const char **input, schptr_t *imm)
{
  unsigned char c = 0;
  const char *ptr = *input;

  if (ptr[0] == '#' && ptr[1] == '\\')
    {
      ptr += 2;
      if (!strncmp (ptr, "alarm", 5))
        {
          c = 0x7;
          *input += 7;
        }
      else if (!strncmp (ptr, "backspace", 9))
        {
          c = 0x8;
          *input += 11;
        }
      else if (!strncmp (ptr, "delete", 6))
        {
          c = 0x7f;
          *input += 8;
        }
      else if (!strncmp (ptr, "escape", 6))
        {
          c = 0x1b;
          *input += 8;
        }
      else if (!strncmp (ptr, "newline", 7))
        {
          c = 0xa;
          *input += 9;
        }
      else if (!strncmp (ptr, "null", 4))
        {
          c = 0x0;
          *input += 6;
        }
      else if (!strncmp (ptr, "return", 6))
        {
          c = 0xd;
          *input += 8;
        }
      else if (!strncmp (ptr, "space", 5))
        {
          c = (uint64_t)' ';
          *input += 7;
        }
      else if (!strncmp (ptr, "tab", 3))
        {
          c = 0x9;
          *input += 5;
        }
      else if (isascii(ptr[2])) // Simple case: #\X where X is ascii
        {
          c = (*input)[2];
          *input += 3;
        }
      else
        {
          fprintf (stderr, "failed to parse `%s'", *input);
          exit (EXIT_FAILURE);
        }

      *imm = sch_encode_imm_char (c);
      return true;
    }

  return false;
}

bool
parse_imm_null (const char **input, schptr_t *imm)
{
  if ((*input)[0] == '(' &&
      (*input)[1] == ')')
    {
      *input += 2;
      *imm = sch_encode_imm_null ();
      return true;
    }

  return false;
}

bool
parse_imm (const char **input, schptr_t *imm)
{
  return
  parse_imm_fixnum (input, imm) ||
    parse_imm_bool (input, imm) ||
    parse_imm_null (input, imm) ||
    parse_imm_char (input, imm);
}

bool
parse_char (const char **input, char c)
{
  if (**input == c)
    {
      (*input)++;
      return true;
    }
  return false;
}

bool
parse_lparen (const char **input)
{
  return parse_char (input, '(');
}

bool
parse_rparen (const char **input)
{
  return parse_char (input, ')');
}

bool
parse_expr (const char **input, schptr_t *sptr)
{
  // An expression is:
  //   * an immediate,
  //   * a primitive,
  //   * an if conditional
  // or a parenthesized expression

  if (parse_imm (input, sptr) ||
      parse_prim (input, sptr) ||
      parse_if (input, sptr) ||
      parse_prim1 (input, sptr) ||
      parse_prim2 (input, sptr))
    return true;

  return false;
}

bool
parse_prim (const char **input, schptr_t *sptr)
{
  for (size_t i = 0; i < primitives_count; i++)
    {
      schprim_t prim = primitives[i];
      if (!strncmp (prim.name, *input, strlen (prim.name)))
        {
          *sptr = (schptr_t)(&(primitives[i]));
          (*input) += strlen (prim.name);
          return true;
        }
    }
  return false;
}

bool
parse_prim_generic (const char **input, size_t nargs, schptr_t *sptr)
{
#define PRIM_MAX_ARGS 2
  assert (nargs <= PRIM_MAX_ARGS);
  const char *ptr = *input;

  if (!parse_lparen (&ptr))
    return false;

  (void) parse_whitespace (&ptr);

  schptr_t prim;
  // infer: initialization here shuts it up
  schptr_t args[PRIM_MAX_ARGS] = {0};

  if (!parse_prim (&ptr, &prim))
    return false;

  for (size_t i = 0; i < nargs; i++)
    {
      (void) parse_whitespace (&ptr);

      if (!parse_expr (&ptr, &args[i]))
        return false;
    }

  if (!parse_rparen (&ptr))
    return false;

  // All parsed correctly
  *input = ptr;

  switch (nargs)
    {
    case 1:
      {
        schprim_eval1_t *pe = (schprim_eval1_t *)alloc (sizeof *pe);
        pe->type = SCH_PRIM_EVAL1;
        pe->prim = (schprim_t *) prim;
        pe->arg1 = args[0];
        *sptr = (schptr_t)pe;
      }
      break;
    case 2:
      {
        schprim_eval2_t *pe = (schprim_eval2_t *)alloc (sizeof *pe);
        pe->type = SCH_PRIM_EVAL2;
        pe->prim = (schprim_t *) prim;
        pe->arg1 = args[0];
        pe->arg2 = args[1];
        *sptr = (schptr_t)pe;
      }
      break;
    default:
      assert (false); // unreachable
      break;
    }

  return true;
#undef PRIM_MAX_ARGS
}

bool
parse_prim1 (const char **input, schptr_t *sptr)
{
  return parse_prim_generic (input, 1, sptr);
}

bool
parse_prim2 (const char **input, schptr_t *sptr)
{
  return parse_prim_generic (input, 2, sptr);
}

///////////////////////////////////////////////////////////////////////
//
// Section EMIT_ASM_
//
// The functions in this section are used to emit assembly to a FILE *
//
//
///////////////////////////////////////////////////////////////////////

#if defined(__APPLE__) || defined(__MACH__)
#  define ASM_SYMBOL_PREFIX "_"
#elif defined(__linux__)
#  define ASM_SYMBOL_PREFIX ""
#else
#  error "Unsupported platform"
#endif

void emit_asm_epilogue (FILE *);
void emit_asm_prologue (FILE *, const char *);
void emit_asm_imm (FILE *, schptr_t);
void emit_asm_if (FILE *, schptr_t, size_t);

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
emit_asm_prim (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval_t *pe = (schprim_eval_t *)sptr;
  pe->prim->emitter (f, sptr, si);
}

void
emit_asm_expr (FILE *f, schptr_t sptr, size_t si)
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
      emit_asm_prim (f, sptr, si);
      break;
    case SCH_IF:
      emit_asm_if (f, sptr, si);
      break;
    default:
      fprintf (stderr, "unknown type 0x%08x\n", type);
      assert (false); // unreachable
      break;
    }
}

// Primitives Emitter
void
emit_asm_prim_fxadd1 (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  const uint64_t cst = UINT64_C(1) << FX_SHIFT;
  fprintf (f, "    addq $%" PRIu64", %%rax\n", cst);
}

void
emit_asm_prim_fxsub1 (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  const uint64_t cst = UINT64_C(1) << FX_SHIFT;
  fprintf (f, "    subq $%" PRIu64 ", %%rax\n", cst);
}

void
emit_asm_prim_fxzerop (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  fprintf (f, "    movl   $%" PRIu64 ", %%edx\n", FALSE_CST);
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", FX_TAG);
  fprintf (f, "    movabsq $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne %%rdx, %%rax\n");
}

void
emit_asm_prim_char_to_fixnum (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fixnum_to_char (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", CHAR_TAG);
}

void
emit_asm_prim_fixnump (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    andq   $%" PRIu64 ", %%rax\n", FX_MASK);
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", FX_TAG);
  fprintf (f, "    sete   %%al\n");
  fprintf (f, "    movzbl %%al, %%eax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", BOOL_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", BOOL_TAG);
}

void
emit_asm_prim_booleanp (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

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
emit_asm_prim_not (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  // I *don't* think this one can be optimized by fixing the values
  fprintf (f, "    movq    $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    cmpq    $%" PRIu64 ", %%rax\n", FALSE_CST);
  fprintf (f, "    movabsq $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne  %%rdx, %%rax\n");
}

void
emit_asm_prim_charp (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    andq   $%" PRIu64 ", %%rax\n", CHAR_MASK);
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", CHAR_TAG);
  fprintf (f, "    sete   %%al\n");
  fprintf (f, "    movzbl %%al, %%eax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", BOOL_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", BOOL_TAG);
}
void
emit_asm_prim_nullp (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  // I *don't* think this one can be optimized by fixing the values
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", NULL_CST);
  fprintf (f, "    sete   %%al\n");
  fprintf (f, "    movzbl %%al, %%eax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", BOOL_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", BOOL_TAG);
}

void
emit_asm_prim_fxlognot (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval1_t *pe = (schprim_eval1_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL1);
  emit_asm_expr (f, pe->arg1, si);

  // This can be improved if we set the tags, masks and shifts in stone
  fprintf (f, "    notq   %%rax\n");
  fprintf (f, "    andq   $%" PRIu64 ", %%rax\n", ~FX_MASK);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fxadd (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    xorq   $%" PRIu64 ", %%rax\n", FX_MASK);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    addq   -%zu(%%rsp), %%rax\n", si);
}

void
emit_asm_prim_fxsub (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    movq   %%rax, %%r8\n");
  fprintf (f, "    movq   -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    subq   %%r8, %%rax\n");
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fxmul (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    imulq  -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    salq   $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    orq    $%" PRIu64 ", %%rax\n", FX_TAG);
}

void
emit_asm_prim_fxlogand (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    andq   -%zu(%%rsp), %%rax\n", si);
}

void
emit_asm_prim_fxlogor (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    orq   -%zu(%%rsp), %%rax\n", si);
}

void
emit_asm_prim_fxeq (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne    %%rdx, %%rax\n");
}

void
emit_asm_prim_fxlt (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovle    %%rdx, %%rax\n");
}

void
emit_asm_prim_fxle (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovl     %%rdx, %%rax\n");
}

void
emit_asm_prim_fxgt (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovge    %%rdx, %%rax\n");
}

void
emit_asm_prim_fxge (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", FX_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", FX_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovg     %%rdx, %%rax\n");
}

void
emit_asm_prim_chareq (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovne    %%rdx, %%rax\n");
}

void
emit_asm_prim_charlt (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", CHAR_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovle    %%rdx, %%rax\n");
}

void
emit_asm_prim_charle (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", CHAR_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovl     %%rdx, %%rax\n");
}

void
emit_asm_prim_chargt (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
  fprintf (f, "    sarq      $%" PRIu8 ", -%zu(%%rsp)\n", CHAR_SHIFT, si);
  fprintf (f, "    sarq      $%" PRIu8 ", %%rax\n", CHAR_SHIFT);
  fprintf (f, "    cmpq      -%zu(%%rsp), %%rax\n", si);
  fprintf (f, "    movq      $%" PRIu64 ", %%rdx\n", FALSE_CST);
  fprintf (f, "    movabsq   $%" PRIu64 ", %%rax\n", TRUE_CST);
  fprintf (f, "    cmovge    %%rdx, %%rax\n");
}

void
emit_asm_prim_charge (FILE *f, schptr_t sptr, size_t si)
{
  schprim_eval2_t *pe = (schprim_eval2_t *) sptr;
  assert (pe->type == SCH_PRIM_EVAL2);

  schptr_t arg1 = pe->arg1;
  emit_asm_expr (f, arg1, si);
  fprintf (f, "    movq   %%rax, -%zu(%%rsp)\n", si);

  schptr_t arg2 = pe->arg2;
  emit_asm_expr (f, arg2, si + WORD_BYTES);
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
emit_asm_if (FILE *f, schptr_t p, size_t si)
{
  schif_t *pif = (schif_t *) p;

  char elsel[LABEL_MAX];
  gen_new_temp_label (elsel);

  char endl[LABEL_MAX];
  gen_new_temp_label (endl);


  emit_asm_expr (f, pif->condition, si);

  // Check if boolean value is true of false and jump accordingly
  fprintf (f, "    cmpq   $%" PRIu64 ", %%rax\n", FALSE_CST);
  fprintf (f, "    je     %s\n", elsel);
  emit_asm_expr (f, pif->thenv, si);
  fprintf (f, "    jmp    %s\n", endl);
  emit_asm_label (f, elsel);
  emit_asm_expr (f, pif->elsev, si);
  emit_asm_label (f, endl);
}

///////////////////////////////////////////////////////////////////////
//
// Section Compilation and Evaluation
//
// Top-level compilation and evaluation functions.
//
//
///////////////////////////////////////////////////////////////////////

// Prototypes
const char *find_system_tmpdir (void);

// Evaluation
void
evaluate(const char *cmd)
{
  compile_expression(cmd);
}

char *
output_asm (schptr_t sptr)
{
  const char *tmpdir = find_system_tmpdir ();
  char itemplate[1024];
  strcpy (itemplate, tmpdir);
  strcat (itemplate, "/rattleXXXXXX.s");

  int ifd = mkstemps (itemplate, 2);
  if (ifd == -1)
    {
      fprintf (stderr, "error creating temporary files for compilation\n");
      exit (EXIT_FAILURE);
    }

  FILE *i = fdopen (ifd, "w");
  if (!i)
    {
      fprintf (stderr, "cannot open file descriptor for `%s'\n", itemplate);
      exit (EXIT_FAILURE);
    }

  // write asm file
  emit_asm_prologue (i, "L_scheme_entry");
  emit_asm_expr (i, sptr, WORD_BYTES);
  emit_asm_epilogue (i);

  // scheme entry received one argument in %rdi,
  // which is the stack top pointer.
  emit_asm_prologue (i, "scheme_entry");
  fprintf (i, "    movq %%rsp, %%rcx\n");
  fprintf (i, "    leaq -4(%%rdi), %%rsp\n");
  fprintf (i, "    call %sL_scheme_entry\n", ASM_SYMBOL_PREFIX);
  fprintf (i, "    movq %%rcx, %%rsp\n");
  emit_asm_epilogue (i);

  // close file
  fclose (i);

  char *ipath = strdup (itemplate);
  if (!ipath)
    err_oom ();

  return ipath;
}

char *
read_file_to_mem (const char *path)
{
  FILE *f = fopen (path, "r");
  if (!f)
    {
      fprintf (stderr, "cannot open `%s' for reading", path);
      exit (EXIT_FAILURE);
    }

  // read file contents to memory
  const size_t blocksize = 1024;
  size_t ssize = blocksize;
  char *s = (char *) alloc (sizeof (*s) * ssize);
  size_t bytes_read = 0;
  int c;
  while ((c = fgetc (f)) != EOF)
    {
      s[bytes_read++] = c;

      // maybe grow s?
      if (bytes_read == ssize)
        {
          ssize += blocksize;
          s = grow (s, ssize);
        }
    }

  // close file
  fclose (f);
  return s;
}

void
dump_asm_if_needed (const char *path)
{
  if (dump_p)
    {
      char *asmdump = read_file_to_mem (path);
      printf ("Assembly dump:\n");
      for (size_t i = 0; i < strlen (asmdump); i++)
        fputc (asmdump[i], stdout);
      printf ("End of Assembly dump\n");
      free (asmdump);
    }
}

void
compile (const char *input, const char *output)
{
  char *s = read_file_to_mem (input);
  schptr_t sptr = 0;
  const char *cs = s;
  if (!parse_expr (&cs, &sptr))
    err_parse (cs);

  // free parsed string
  free (s);

  char *asmtmp = output_asm (sptr);
  dump_asm_if_needed (asmtmp);

  // Now compile file and link with runtime
  {
    int child = fork ();
    if (child == 0)
      {
        // inside child
        execl (CC, CC, "-o", output, asmtmp, "runtime.o", (char *) NULL);

        // unreachable
        assert (false);
      }

    // wait for child to complete compilation
    waitpid (child, NULL, 0);
  }

  // free path to tmp file
  if (save_temps_p)
    printf ("Temporary asm source kept at `%s'\n", asmtmp);
  else
    unlink (asmtmp);
  free (asmtmp);
}

const char *
find_system_tmpdir (void)
{
  static const char *default_tmpdir = "/tmp/";

  char *tmpdir = NULL;

  if ((tmpdir = getenv ("TMPDIR")))
    return tmpdir;
  else if ((tmpdir = getenv ("TMP")))
    return tmpdir;
  else if ((tmpdir = getenv ("TEMPFILE")))
    return tmpdir;
  else if ((tmpdir = getenv ("TEMP")))
    return tmpdir;

  if (tmpdir)
    return tmpdir;
  else
    return default_tmpdir;
}

void
compile_expression (const char *e)
{
  schptr_t sptr = 0;
  if (!parse_expr (&e, &sptr))
    err_parse (e);

  const char *tmpdir = find_system_tmpdir ();
  char otemplate[1024];
  strcpy (otemplate, tmpdir);
  strcat (otemplate, "/librattleXXXXXX.so");

  int ofd = mkstemps (otemplate, 3);
  if (ofd == -1)
    {
      fprintf (stderr, "error creating temporary files for compilation\n");
      exit (EXIT_FAILURE);
    }

  char *asmtmp = output_asm (sptr);
  dump_asm_if_needed (asmtmp);

  // close ofd so gcc can write to it
  close (ofd);

  // Now compile file and link with runtime
  {
    int child = fork ();
    if (child == 0)
      {
        // inside child
        execl (CC, CC, "-shared", "-fPIC", "-o", otemplate, asmtmp, "runtime.o", (char *) NULL);

        // unreachable
        assert (false);
      }

    // wait for child to complete compilation
    waitpid (child, NULL, 0);
  }

  // remove input file and close port
    if (save_temps_p)
    printf ("Temporary asm source kept at `%s'\n", asmtmp);
  else
    unlink (asmtmp);
  free (asmtmp);

  // We have compiled the linked library so we are ready to
  // dynamically load the library
  {
    void *handle = NULL;
    void (*fn) (void);
    handle = dlopen (otemplate, RTLD_NOW | RTLD_GLOBAL);

    if (!handle)
      {
        fprintf (stderr, "%s\n", dlerror());
        exit (EXIT_FAILURE);
      }

    fn = dlsym (handle, "runtime_startup");
    if (!fn)
      {
        fprintf (stderr, "%s\n", dlerror ());
        dlclose (handle);
        exit (EXIT_FAILURE);
      }

    fn ();
    dlclose (handle);
  }

  // delete output file
  if (save_temps_p)
    printf ("Temporary shared object kept at `%s'\n", otemplate);
  else
    unlink (otemplate);
}

