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

int
main(int argc, char *argv[]) {

  bool compile_p = false;
  bool evaluate_p = false;
  char *input = NULL;
  char *output = NULL;

  int opt;
  while ((opt = getopt(argc, argv, "hec:o:")) != -1)
    {
      switch (opt)
        {
        case 'h':
          help(argv[0]);
          break;
        case 'c':
          compile_p = true;
          input = strdup(optarg);
          break;
        case 'o':
          output = strdup(optarg);
          break;
        case 'e':
          evaluate_p = true;
          break;
        case ':':
          fprintf (stderr, "flag missing operand\n");
          usage(argv[0]);
          break;
        case '?':
          fprintf (stderr, "unrecognized option: '-%c'\n", optopt);
          usage(argv[0]);
          break;
        default:
          usage(argv[0]);
          break;
        }
    }

  if (evaluate_p && compile_p)
    {
      fprintf(stderr, "cannot specify both -e and -c\n");
      usage(argv[0]);
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
      evaluate(cmd);
    }

  if (compile_p)
    compile(input, output);

  return 0;
}

///////////////////////////////////////////////////////////////////////
//
// Section Scheme Values
//
//
///////////////////////////////////////////////////////////////////////
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
// Fixnum:        0x01
// Boolean True:  0x2f - 0b 0010 1111
// Boolean False: 0x6f - 0b 0110 1111
// Null:          0x3f - 0b 0011 1111
// Character:     0x0f - 0b 0000 1111
// Non-Immediate: 0x00 - 0b 0000 0000

// Immediates are just represented by a single 64bit value
typedef uint64_t sch_imm;

typedef enum { SCH_PRIM } sch_type;

// Primitives
struct sch_prim; // fwd declaration
typedef void (*prim_emmiter) (FILE *, struct sch_prim *);

typedef struct sch_prim
{
  sch_type type;         // SCH_PRIM for all sch_prim values
  char *name;            // Primitive name
  unsigned int argcount; // Number of arguments for the primitive
  prim_emmiter fn;       // Primiive function emmiter
} sch_prim;


// Primitive emitter prototypes
void emit_asm_prim_fxadd1 (FILE *, sch_prim *);

sch_prim primitives[] =
  { { SCH_PRIM, "fxadd1", 1, emit_asm_prim_fxadd1 } };

///////////////////////////////////////////////////////////////////////
//
//  Section Parsing
//
//
///////////////////////////////////////////////////////////////////////

char *
skip_space (char *input)
{
  while (isspace (*input)) input++;
  return input;
}

bool
parse_imm_bool (const char **input, sch_imm *imm)
{
  if ((*input)[0] == '#')
    {
      if ((*input)[1] == 't' || (*input)[1] == 'f')
        {
          // Booleans are represented as:
          // false: 00101111 (0x2f)
          // true:  01101111 (0x6f)
          *imm = ((*input)[1] == 't') ? (sch_imm) 0x6f : (sch_imm) 0x2f;
          *input += 2;
          return true;
        }
    }

  return false;
}

bool
parse_imm_fixnum (const char **input, sch_imm *imm)
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

  if (seen_num && v <= FIXNUM_MAX)
    {
      int64_t fx =  (int64_t) v;
      if (sign && *sign == '-')
        fx = -v;

      // Fixnums have two bottom bits as 0
      *imm = (sch_imm) (fx << 2);
      return true;
    }

  return false;
}

bool
parse_imm_char (const char **input, sch_imm *imm)
{
  unsigned char c = 0;
  const char *ptr = *input;

  if (ptr[0] == '#' && ptr[1] == '\\')
    {
      ptr += 2;
      if (!strncmp (ptr, "alarm", 5))
        {
          c = 0x7;
          *input += 5;
        }
      else if (!strncmp (ptr, "backspace", 9))
        {
          c = 0x8;
          *input += 9;
        }
      else if (!strncmp (ptr, "delete", 6))
        {
          c = 0x7f;
          *input += 6;
        }
      else if (!strncmp (ptr, "escape", 6))
        {
          c = 0x1b;
          *input += 6;
        }
      else if (!strncmp (ptr, "newline", 7))
        {
          c = 0xa;
          *input += 7;
        }
      else if (!strncmp (ptr, "null", 4))
        {
          c = 0x0;
          *input += 4;
        }
      else if (!strncmp (ptr, "return", 6))
        {
          c = 0xd;
          *input += 6;
        }
      else if (!strncmp (ptr, "space", 5))
        {
          c = (uint64_t)' ';
          *input += 5;
        }
      else if (!strncmp (ptr, "tab", 3))
        {
          c = 0x9;
          *input += 3;
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

      // Chars have the lowest byte as 0x0f
      *imm = (c << 8) | 0x0f;
      return true;
    }

  return false;
}

bool
parse_imm_null (const char **input, sch_imm *imm)
{
  if ((*input)[0] == 'n' &&
      (*input)[1] == 'u' &&
      (*input)[2] == 'l' &&
      (*input)[3] == 'l')
    {
      *input += 4;

      // null is represented as 0x3f
      *imm = 0x3f;
      return true;
    }

  return false;
}

bool
parse_imm (const char **input, sch_imm *imm)
{
  return
  parse_imm_fixnum (input, imm) ||
    parse_imm_bool (input, imm) ||
    parse_imm_null (input, imm) ||
    parse_imm_char (input, imm);
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
emit_asm_imm (FILE *f, sch_imm imm)
{
  if (imm > 4294967295)
    fprintf (f, "    movabsq $%" PRIu64 ", %%rax\n", imm);
  else
    fprintf (f, "    movl $%" PRIu64 ", %%eax\n", imm);
}

void
emit_asm_expr (sch_

// Primitives Emitter
void
emit_asm_prim_fxadd1 (FILE *f, sch_prim *p)
{
  emit_asm_expr (p);
  fprintf (f, "    addl $4, %%eax");
}


///////////////////////////////////////////////////////////////////////
//
// Section Compilation and Evalution
//
// Top-level compilation and evaluation functions.
//
//
///////////////////////////////////////////////////////////////////////


// Evaluation
void
evaluate(const char *cmd)
{
  compile_expression(cmd);
}

void
compile(const char *input, const char *output)
{
  (void) input;
  (void) output;
  assert(false);
}

const char *
find_system_tmpdir ()
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
  sch_imm imm = 0;
  bool parsed = parse_imm (&e, &imm);

  if (!parsed)
    {
      fprintf (stderr, "error: cannot parse `%s'\n", e);
      exit (EXIT_FAILURE);
    }

  const char *tmpdir = find_system_tmpdir ();
  char itemplate[1024];
  char otemplate[1024];
  strcpy (itemplate, tmpdir);
  strcat (itemplate, "/rattleXXXXXX.s");
  strcpy (otemplate, tmpdir);
  strcat (otemplate, "/librattleXXXXXX.so");

  int ifd = mkstemps (itemplate, 2);
  int ofd = mkstemps (otemplate, 3);

  if (ifd == -1 || ofd == -1)
    {
      fprintf (stderr, "error creating temporary files for compilation\n");
      exit (EXIT_FAILURE);
    }

  FILE *i = fdopen (ifd, "w");

  // write asm file
  emit_asm_prologue (i, "scheme_entry");
  emit_asm_imm (i, imm);
  emit_asm_epilogue (i);

  // close file
  fclose (i);

  // close ofd so gcc can write to it
  close (ofd);

  // Now compile file and link with runtime
  {
    int child = fork ();
    if (child == 0)
      {
        // inside child
        execl (CC, CC, "-shared", "-fPIC", "-o", otemplate, itemplate, "runtime.o", (char *) NULL);

        // unreachable
        assert (false);
      }

    // wait for child to complete compilation
    waitpid (child, NULL, 0);
  }

  // remove input file and close port
  unlink (itemplate);
  close (ifd);

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
  unlink (otemplate);
}

