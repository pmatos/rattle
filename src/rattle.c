#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>

#include "config.h"

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
      char **commands = malloc (ncommands * sizeof(*commands));
      for (int i = 0; i < ncommands; i++)
        commands[i] = strdup(argv[optind + i]);

      commands[ncommands] = NULL;
      evaluate(commands);
    }

  if (compile_p)
    compile(input, output);

  return 0;
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
void
compile (const char *i __attribute__((unused)),
         const char *o __attribute__((unused)))
{
  // todo
}

void
compile_expression (const char *e)
{
  unsigned long long num = 0;

  errno = 0;
  num = strtoull(e, NULL, 0);

  if (errno == 0)
    {
      uint64_t num64 = num & INT64_MAX;

      if (num64 == num)
        {
          printf ("    .text\n");
          printf ("    .globl scheme_entry\n");
          printf ("    .type scheme_entry, @function\n");
          printf ("scheme_entry:\n");
          printf ("    movl $%" PRIu64 ", %%eax\n", num64);
          printf ("    ret\n");
        }
    }
  else
    fprintf (stderr, "error\n");
}

void emit_immediate(uint64_t imm)
{
  printf ("    .text\n");
  printf ("    .globl scheme_entry\n");
  printf ("    .type scheme_entry, @function\n");
  printf ("scheme_entry:\n");
  printf ("    movl $%" PRIu64 ", %%eax\n", imm);
  printf ("    ret\n");
}

void
compile_fixnum (uint64_t n)
{
  // Fixnums have two bottom bits as 0
  emit_immediate (n << 2);
}

void
compile_char (char c)
{
  // Chars have the lowest byte as 0x0f
  emit_immediate ((c << 8) | 0x0f);
}

void
compile_bool (bool b)
{
  // Booleans are represented as:
  // false: 00101111 (0x2f)
  // true:  01101111 (0x6f)
  if (b)
    emit_immediate (0x6f);
  else
    emit_immediate (0x2f);
}

void
compile_null()
{
  // null is represented as 0x3f
  emit_immediate (0x3f);
}


// Parsing
enum sch_type { SCH_NULL, SCH_FIXNUM, SCH_BOOL, SCH_CHAR };

struct sch_imm
{
  enum sch_type type;
  uint64_t value;
};

char *skip_space (char *input)
{
  while (isspace (*input)) input++;
  return input;
}

bool parse_imm_bool (const char **input, struct sch_imm *imm)
{
  if (*input[0] == '#')
    {
      if (*input[1] == 't')
        {
          imm = malloc (sizeof(*imm));
          imm->type = SCH_BOOL;
          imm->value = 1;
          *input += 2;
          return true;
        }
      else if (*input[1] == 'f')
        {
          imm = malloc (sizeof(*imm));
          imm->type = SCH_BOOL;
          imm->value = 0;
          *input += 2;
          return true;
        }
      return false;
    }
  return false;
}

#define FIXNUM_MIN -2305843009213693952LL
#define FIXNUM_MAX  2305843009213693951LL


bool parse_imm_fixnum (const char **input, struct sch_imm *imm)
{
  const char *sign  = NULL;
  const char *start = NULL;
  const char *end   = NULL;

  if (**input == '+' || **input == '-')
    {
      sign = *input;
      start = *input + 1;
      end = start;
    }
  while (isdigit(*end++));
  
}

bool parse_imm_char (const char **input, struct sch_imm *imm)
{
  if (*input[0] == '#' &&
      *input[1] == '\\' &&
      isascii(*input[2]))
    {
      imm = malloc(sizeof(*imm));
      imm->type = SCH_CHAR;
      imm->value = *input[2];
      *input += 3;
      return true;
    }
  return false;
}

bool parse_imm_null (const char **input, struct sch_imm *imm)
{
  if (*input[0] == 'n' &&
      *input[1] == 'u' &&
      *input[2] == 'l' &&
      *input[3] == 'l')
    {
      imm = malloc (sizeof (*imm));
      imm->type = SCH_NULL;
      imm->value = 0;
      *input += 4;
      return true;
    }

  return false;
}
