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

#include "config.h"

static const char *CC = "/usr/bin/gcc";

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
void evaluate(char **);
void compile (const char *, const char*);
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
      char **commands = malloc ((ncommands + 1) * sizeof(*commands));
      for (int i = 0; i < ncommands; i++)
        commands[i] = strdup(argv[optind + i]);

      commands[ncommands] = NULL;
      evaluate(commands);
    }

  if (compile_p)
    compile(input, output);

  return 0;
}

// Parsing
enum sch_type { SCH_NULL, SCH_FIXNUM, SCH_BOOL, SCH_CHAR };

struct sch_imm
{
  enum sch_type type;
  uint64_t value;
};

char *
skip_space (char *input)
{
  while (isspace (*input)) input++;
  return input;
}

struct sch_imm *
parse_imm_bool (const char **input)
{
  struct sch_imm *imm = NULL;
  if (*input[0] == '#')
    {
      if (*input[1] == 't')
        {
          imm = malloc (sizeof(*imm));
          imm->type = SCH_BOOL;
          imm->value = 1;
          *input += 2;
        }
      else if (*input[1] == 'f')
        {
          imm = malloc (sizeof(*imm));
          imm->type = SCH_BOOL;
          imm->value = 0;
          *input += 2;
        }
    }
  return imm;
}

#define FIXNUM_MIN -2305843009213693952LL
#define FIXNUM_MAX  2305843009213693951LL

struct sch_imm *
parse_imm_fixnum (const char **input)
{
  struct sch_imm *imm = NULL;
  const char *sign  = NULL;
  const char *end   = NULL;

  if (**input == '+' || **input == '-')
    {
      sign = *input;
      end = *input + 1;
    }
  while (isdigit(end++));

  unsigned long long v = 0;
  while (--end != sign)
      v = (v * 10) + *end;

  if (v <= FIXNUM_MAX)
    {
      imm = malloc(sizeof(*imm));
      imm->type = SCH_FIXNUM;
      imm->value = (sign && *sign == '-') ? -v : v;
    }

  return imm;
}

struct sch_imm *
parse_imm_char (const char **input)
{
  struct sch_imm *imm = NULL;

  if (*input[0] == '#' &&
      *input[1] == '\\' &&
      isascii(*input[2]))
    {
      imm = malloc(sizeof(*imm));
      imm->type = SCH_CHAR;
      imm->value = *input[2];
      *input += 3;
    }

  return imm;
}

struct sch_imm *
parse_imm_null (const char **input)
{
  struct sch_imm *imm = NULL;

  if (*input[0] == 'n' &&
      *input[1] == 'u' &&
      *input[2] == 'l' &&
      *input[3] == 'l')
    {
      imm = malloc (sizeof (*imm));
      imm->type = SCH_NULL;
      imm->value = 0;
      *input += 4;
    }

  return imm;
}

struct sch_imm *
parse_imm (const char **input)
{
  struct sch_imm *imm = parse_imm_null (input);

  if (!imm)
    {
      imm = parse_imm_char (input);
    }
  if (!imm)
    {
      imm = parse_imm_fixnum (input);
    }
  if (!imm)
    {
      imm = parse_imm_bool (input);
    }

    return imm;
}

// Evaluation
void
evaluate(char **cmds)
{
  int i = 0;
  char *cmd = cmds[i];
  while (cmd)
    {
      compile_expression(cmd);
      cmd = cmds[++i];
    }
}

// Compilation
void emit_immediate(uint64_t imm, FILE *f)
{
  fprintf (f, "    .text\n");
  fprintf (f, "    .globl scheme_entry\n");
  fprintf (f, "    .type scheme_entry, @function\n");
  fprintf (f, "scheme_entry:\n");
  fprintf (f, "    movl $%" PRIu64 ", %%eax\n", imm);
  fprintf (f, "    ret\n");
}

void
compile (const char *i __attribute__((unused)),
         const char *o __attribute__((unused)))
{
  // todo
}


void
compile_fixnum (uint64_t n, FILE *f)
{
  // Fixnums have two bottom bits as 0
  emit_immediate (n << 2, f);
}

void
compile_char (char c, FILE *f)
{
  // Chars have the lowest byte as 0x0f
  emit_immediate ((c << 8) | 0x0f, f);
}

void
compile_bool (bool b, FILE* f)
{
  // Booleans are represented as:
  // false: 00101111 (0x2f)
  // true:  01101111 (0x6f)
  if (b)
    emit_immediate (0x6f, f);
  else
    emit_immediate (0x2f, f);
}

void
compile_null(FILE* f)
{
  // null is represented as 0x3f
  emit_immediate (0x3f, f);
}

void
compile_expression (const char *e)
{
  struct sch_imm *imm = parse_imm (&e);

  char *template = "rattleXXXXXX";
  int idesc = mkstemp (template);
  int odesc = mkstemp (template);
  FILE *i = fdopen (idesc, "w");

  // close descriptor opened by mkstemp
  close (odesc);
  
  if (imm)
    {
      switch (imm->type)
        {
        case SCH_NULL:
          compile_null (f);
          break;
        case SCH_BOOL:
          compile_bool (imm->value, f);
          break;
        case SCH_CHAR:
          compile_char (imm->value, f);
          break;
        case SCH_FIXNUM:
          compile_char (imm->value, f);
          break;
        default:
          // unreachable
          assert (false);
        }
      fclose (f);
      free (imm);
    }
  else
    {
      fclose (f);
      fprintf (stderr, "error: cannot parse `%s'\n", e);
      exit (EXIT_FAILURE);
    }

  // Now compile file and link with runtime
  if (fork () == 0)
    {
      // inside child
      execl (CC, "gcc", "-o", 
    }

  
  
}
