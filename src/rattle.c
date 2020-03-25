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
#include "config.h"

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
          strncpy (input, optarg, FILE_PATH_MAX);
          input[FILE_PATH_MAX - 1] = '\0';
          break;
        case 'o':
          strncpy (output, optarg, FILE_PATH_MAX);
          output[FILE_PATH_MAX - 1] = '\0';
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

__attribute__((noreturn)) void
err_unreachable (const char *s)
{
  fprintf (stderr, "error: unreachable - `%s'\n", s);
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
struct env;
typedef void (*prim_emmiter) (FILE *, schptr_t, size_t, struct env *);

typedef enum { SCH_PRIM, SCH_IF, SCH_ID, SCH_LET, SCH_EXPR_SEQ, SCH_PRIM_EVAL1, SCH_PRIM_EVAL2 } sch_type;

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

typedef struct schid
{
  sch_type type;
  char *name;
} schid_t;

typedef struct binding_spec_list
{
  schid_t *id;
  schptr_t expr;
  struct binding_spec_list *next;
} binding_spec_list_t;

typedef struct schlet
{
  sch_type type;
  binding_spec_list_t *bindings;
  schptr_t body;
} schlet_t;

typedef struct expression_list
{
  schptr_t expr;
  struct expression_list *next;
} expression_list_t;

typedef struct schexprseq
{
  sch_type type;
  expression_list_t *seq;
} schexprseq_t;


///////////////////////////////////////////////////////////////////////
//
//  Environment manipulation
//
//
///////////////////////////////////////////////////////////////////////

typedef struct env
{
  schid_t *id;
  size_t si;
  struct env *next;
} env_t;

env_t *make_env ()
{
  return NULL;
}

env_t *
env_add (schid_t *id, size_t si, env_t *env)
{
  env_t *e = alloc (sizeof (*e));
  e->id = id;
  e->si = si;
  e->next = env;
  return e;
}

env_t *
env_append (env_t *env1, env_t *env2)
{
  if (!env1)
    return env2;

  env_t *ptr = env1;
  for (; ptr->next != NULL; ptr = ptr->next);
  ptr->next = env2;
  return env1;
}

bool
env_ref (schid_t *id, env_t *env, size_t *si)
{
  for (env_t *e = env; e; e = e->next)
    {
      if (!strcmp (id->name, e->id->name))
        {
          *si = e->si;
          return true;
        }
    }
  return false;
}

// Primitive emitter prototypes
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

// Main parsing procedures

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

bool
parse_whitespace (const char **input)
{
  const char *tmp = *input;
  while (isspace (**input))
    (*input)++;
  return *input != tmp; // return true if whitespace was skipped
}

bool
parse_letter (const char **input)
{
  char i = **input;

  if ((i >= 'a' && i <= 'z') ||
      (i >= 'A' && i <= 'Z'))
    {
      (*input)++;
      return true;
    }

  return false;
}

bool
parse_special_initial (const char **input)
{
  char i = **input;

  switch (i)
    {
    case '!':
    case '$':
    case '%':
    case '&':
    case '*':
    case '/':
    case ':':
    case '<':
    case '=':
    case '>':
    case '?':
    case '^':
    case '_':
    case '~':
      (*input)++;
      return true;
    }

  return false;
}

bool
parse_initial (const char **input)
{
  // Parses an expression as follows:
  //   <letter>
  // | <special initial>
  return parse_letter (input) || parse_special_initial (input);
}

bool
parse_explicit_sign (const char **input)
{
  // Parses an expression as follows:
  // + | -
  return parse_char (input, '+') || parse_char (input, '-');
}

bool
parse_special_subsequent (const char **input)
{
  // Parses an expression as follows:
  //   <explicit sign>
  // | .
  // | @
  return parse_explicit_sign (input)
    || parse_char (input, '.')
    || parse_char (input, '@');
}

bool
parse_digit (const char **input)
{
  switch (**input)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      (*input)++;
      return true;
    }
  return false;
}

bool
parse_subsequent (const char **input)
{
  // Parses an expression as follows:
  //    <initial>
  // |  <digit>
  // |  <special subsequent>
  return parse_initial (input)
    || parse_digit (input)
    || parse_special_subsequent (input);
}

bool
parse_vertical_line (const char **input)
{
  return parse_char (input, '|');
}

bool
parse_char_sequence (const char **input, const char *seq)
{
  if (!strncmp (*input, seq, strlen (seq)))
    {
      (*input) += strlen (seq);
      return true;
    }
  return false;
}

bool
parse_hex_digit (const char **input)
{
  switch (**input)
    {
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
      (*input)++;
      return true;
    }

  return parse_digit (input);
}

bool
parse_hex_scalar_value (const char **input)
{
  // Parses an expression as follows:
  // <hex digit> +
  if (!parse_hex_digit (input))
    return false;

  while (parse_hex_digit (input));
  return true;
}

bool
parse_inline_hex_escape (const char **input)
{
  // Parses an expression as follows:
  // \x <hex scalar value>
  return parse_char_sequence (input, "\\x") && parse_hex_scalar_value(input);
}

bool
parse_mnemonic_escape (const char **input)
{
  return parse_char_sequence (input, "\\a")
    || parse_char_sequence (input, "\\b")
    || parse_char_sequence (input, "\\t")
    || parse_char_sequence (input, "\\n")
    || parse_char_sequence (input, "\\r");
}

bool
parse_symbol_element (const char **input)
{
  // Parses an expression as follows:
  // <any char other than <vertical line> or \>
  // | <inline hex escape>
  // | <mnemonic excape>
  // | \|

  if (**input != '|' && **input != '\\')
    {
      (*input)++;
      return true;
    }
  else if ((*input)[0] == '\\' && (*input)[1] == '|')
    {
      (*input) += 2;
      return true;
    }
  else
    return parse_inline_hex_escape (input)
      || parse_mnemonic_escape (input);
}

bool
parse_sign_subsequent (const char **input)
{
  // Parses an expression as follows:
  // <initial>
  // | <explicit sign>
  // | @
  return parse_initial (input)
    || parse_explicit_sign (input)
    || parse_char (input, '@');
}

bool
parse_dot_subsequent (const char **input)
{
  return parse_sign_subsequent (input) || parse_char (input, '.');
}

bool
parse_peculiar_identifier (const char **input)
{
  const char *ptr = *input;

  // Parses an expression as follows:
  //   <explicit sign>
  // | <explicit sign> <sign subsequent> <subsequent>*
  // | <explicit sign> . <dot subsequent> <subsequent>*
  // | . <dot subsequent> <subsequent>*
  if (parse_explicit_sign (&ptr) &&
      parse_sign_subsequent (&ptr))
    {
      while (parse_subsequent (&ptr));
      *input = ptr;
      return true;
    }
  else if (parse_explicit_sign (&ptr) &&
           parse_char (&ptr, '.') &&
           parse_dot_subsequent (&ptr))
    {
      while (parse_subsequent (&ptr));
      *input = ptr;
      return true;
    }
  else if (parse_char (&ptr, '.') &&
           parse_dot_subsequent (&ptr))
    {
      while (parse_subsequent (&ptr));
      *input = ptr;
      return true;
    }
  else if (parse_explicit_sign (&ptr))
    {
      *input = ptr;
      return true;
    }

  return false;
}

bool
parse_identifier (const char **input, schptr_t *sptr)
{
  const char *ptr = *input;
  char *id = NULL;

  // Parses an expression as follows:
  //    <initial> <subsequent>*
  // |  <vertical line> <symbol element>* <vertical line>
  // |  <peculiar identifier>
  if (parse_initial (&ptr))
    {
      while (parse_subsequent (&ptr));
      id = strndup (*input, ptr - *input);
    }
  else if (parse_vertical_line (&ptr))
    {
      while (parse_symbol_element (&ptr));
      if (parse_vertical_line (&ptr))
        id = strndup (*input, ptr - *input);
    }
  else if (parse_peculiar_identifier (&ptr))
    id = strndup (*input, ptr - *input);

  if (!id)
    return false;

  // Successfully parsed an identifier so we can now create it
  schid_t *sid = (schid_t *) alloc (sizeof *sid);
  if (!sid)
    err_oom ();

  sid->type = SCH_ID;
  sid->name = id;

  *sptr = (schptr_t) sid;
  *input = ptr;
  return true;
}

bool
parse_binding_spec (const char **input, schptr_t *left, schptr_t *right)
{
  const char *ptr = *input;

  // Parses an expression as follows:
  // (<identifier> <expression>)
  if (!parse_lparen(&ptr))
    return false;

  // skip possible whitespace between lparen and the identifier
  (void) parse_whitespace (&ptr);

  schptr_t identifier;
  if (!parse_identifier (&ptr, &identifier))
    return false;

  // skip possible whitespace between identifier and expression
  (void) parse_whitespace (&ptr);

  schptr_t expression;
  if (!parse_expression (&ptr, &expression))
    return false;

  // skip possible whitespace between expression and rparen
  (void) parse_whitespace (&ptr);

  if (!parse_rparen (&ptr))
    return false;

  *input = ptr;

  *left = identifier;
  *right = expression;

  return true;
}

/* bool */
/* parse_definition (const char **input, schptr_t *sptr) */
/* { */
/*   return false; */
/* } */

bool
parse_body (const char **input, schptr_t *sptr)
{
  const char *ptr = *input;

  // a body is a sequence of definitions followed by a
  // non empty sequence of expressions
  // TODO skipping definitions for now
  /* definition_list_t *defs = NULL; */
  /* definition_t def; */
  /* while (parse_definition (ptr, *def)) */
  /*   { */

  /*   } */

  // parse for now a non-empty list of expressions
  expression_list_t *elst = NULL;
  expression_list_t *last = NULL;
  schptr_t e;
  if (!parse_expression (&ptr, &e))
    return false;

  elst = alloc (sizeof (*elst));
  elst->expr = e;
  elst->next = NULL;
  last = elst;

  (void) parse_whitespace (&ptr);

  while (parse_expression (&ptr, &e))
    {
      (void) parse_whitespace (&ptr);

      expression_list_t *n = alloc (sizeof (*n));
      n->expr = e;
      n->next = NULL;
      last->next = n;
      last = n;
    }

  *input = ptr;

  schexprseq_t *seq = alloc (sizeof (*seq));
  seq->type = SCH_EXPR_SEQ;
  seq->seq = elst;
  *sptr = (schptr_t) seq;

  return true;
}

bool
parse_let_wo_id (const char **input, schptr_t *sptr)
{
  const char *ptr = *input;

  // Parses an expression as follows:
  // (let (<binding spec>*) <body>)
  if (!parse_lparen (&ptr))
    return false;

  // skip possible whitespace between lparen and let keyword
  (void) parse_whitespace (&ptr);

  if (! parse_char_sequence (&ptr, "let"))
    return false;

  // skip possible whitespace between let and lparen
  (void) parse_whitespace (&ptr);

  if (!parse_lparen (&ptr))
    return false;

  // Now we parse each of the binding specs and store them
  binding_spec_list_t *bindings = NULL;
  schptr_t body;

  while (1)
    {
      schptr_t id;
      schptr_t expr;
      if (!parse_binding_spec (&ptr, &id, &expr))
        break;

      // skip possible whitespace between binding specs
      (void) parse_whitespace (&ptr);

      binding_spec_list_t *b = alloc (sizeof (*b));
      b->id = (schid_t *) id;
      b->expr = expr;
      b->next = bindings;

      bindings = b;
    }

  // skip possible whitespace between last binding spec and rparen
  (void) parse_whitespace (&ptr);

  if (!parse_rparen (&ptr))
    return false;

  // skip possible whitespace between rparen and body
  (void) parse_whitespace (&ptr);

  if (!parse_body (&ptr, &body))
    return false;

  // skip possible whitespace between body and rparen
  (void) parse_whitespace (&ptr);

  if (!parse_rparen (&ptr))
    return false;

  // Parse of let successful and complete
  *input = ptr;

  schlet_t *l = (schlet_t *) alloc (sizeof (*l));
  l->type = SCH_LET;
  l->bindings = bindings;
  l->body = body;

  *sptr = (schptr_t) l;

  return true;
}

bool
parse_if (const char **input, schptr_t *sptr)
{
  const char *ptr = *input;
  // Parses an expression as follows:
  // (if <expr> <expr> <expr>)
  if (!parse_lparen (&ptr))
    return false;

  // skip possible whitespace between lparen and if keyword
  (void) parse_whitespace (&ptr);

  if (! parse_char_sequence (&ptr, "if"))
    return false;

  // skip whitespace between if identifier and condition
  (void) parse_whitespace (&ptr);

  schptr_t condition;
  schptr_t thenv;
  schptr_t elsev;

  if (!parse_expression (&ptr, &condition))
    return false;

  // skip whitespace between condition and then-value
  (void) parse_whitespace (&ptr);

  if (!parse_expression (&ptr, &thenv))
      return false;

  // skip whitespace between then-value and else-value
  (void) parse_whitespace (&ptr);

  if (!parse_expression (&ptr, &elsev))
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
parse_expression (const char **input, schptr_t *sptr)
{
  // An expression is:
  //   * an immediate,
  //   * a primitive,
  //   * an if conditional
  // or a parenthesized expression

  if (parse_imm (input, sptr) ||
      parse_identifier (input, sptr) ||
      parse_prim (input, sptr) ||
      parse_if (input, sptr) ||
      parse_let_wo_id (input, sptr) ||
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

      if (!parse_expression (&ptr, &args[i]))
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
      err_unreachable ("unhandled number of args for primitive");
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
void emit_asm_if (FILE *, schptr_t, size_t, env_t *);
void emit_asm_let (FILE *, schptr_t, size_t, env_t *);
void emit_asm_expr_seq (FILE *, schptr_t, size_t, env_t *);

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
  env_t *nenv = make_env ();
  size_t freesi = si; // currently free si
  for (binding_spec_list_t *bs = let->bindings; bs != NULL; bs = bs->next)
    {
      emit_asm_expr (f, bs->expr, freesi, env);

      fprintf (f, "    movq %%rax, -%zu(%%rsp)\n", freesi);

      nenv = env_add (bs->id, freesi, nenv);
      freesi += WORD_BYTES;
    }

  env = env_append (nenv, env);
  emit_asm_expr (f, let->body, freesi, env);
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
  char itemplate[FILE_PATH_MAX] = { 0, };
  strncpy (itemplate, tmpdir, FILE_PATH_MAX);
  strncat (itemplate, "/rattleXXXXXX.s", FILE_PATH_MAX - strlen (tmpdir));
  itemplate[FILE_PATH_MAX - 1] = '\0';

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
  emit_asm_expr (i, sptr, WORD_BYTES, make_env ());
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
  if (!parse_expression (&cs, &sptr))
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
#ifdef UBSANLIB
        execl (CC, CC, "-o", output, asmtmp, "runtime.o", UBSANLIB, (char *) NULL);
#else
        execl (CC, CC, "-o", output, asmtmp, "runtime.o", (char *) NULL);
#endif

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
  static char real_tmpdir[FILE_PATH_MAX] = "/tmp/";
  const char *vars[] = { "TMPDIR", "TMP", "TEMPFILE", "TEMP" };
  const size_t varslen = sizeof (vars) / sizeof (vars[0]);

  for (size_t i = 0; i < varslen; i++)
    {
      const char *v = vars[i];
      char *tmpdir = getenv (v);

      if (tmpdir && *tmpdir != '\0')
        strncpy (real_tmpdir, tmpdir, FILE_PATH_MAX);
      real_tmpdir[FILE_PATH_MAX-1] = '\0';
    }
  return real_tmpdir;
}

void
compile_expression (const char *e)
{
  schptr_t sptr = 0;
  if (!parse_expression (&e, &sptr))
    err_parse (e);

  const char *tmpdir = find_system_tmpdir ();
  char otemplate[FILE_PATH_MAX] = { 0, };
  strncpy (otemplate, tmpdir, FILE_PATH_MAX);
  strncat (otemplate, "/librattleXXXXXX.so", FILE_PATH_MAX - strlen (tmpdir));
  otemplate[FILE_PATH_MAX - 1] = '\0';

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

