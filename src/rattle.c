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

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "emit.h"
#include "err.h"
#include "memory.h"
#include "parse.h"
#include "structs.h"

#include "common.h"
#include "config.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

static const char *CC = "/usr/bin/cc";

// Compiler main entry point file
void __attribute__ ((noreturn)) usage (const char *prog)
{
  fprintf (stderr, "rattle version %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
  fprintf (stderr, "Usage: %s [-hec] [expression ...]\n", prog);
  exit (EXIT_FAILURE);
}

void __attribute__ ((noreturn)) help (const char *prog) { usage (prog); }

// Prototypes
void evaluate (const char *);
void compile (const char *, const char *);
void compile_program (const char *);

// TODO find correct posix value
#define FILE_PATH_MAX 1024

// Option globals
static bool dump_p = false;
static bool save_temps_p = false;

int
main (int argc, char *argv[])
{

  bool compile_p = false;
  bool evaluate_p = false;
  char input[FILE_PATH_MAX];
  char output[FILE_PATH_MAX];

  int opt;
  while ((opt = getopt (argc, argv, "hdsec:o:")) != -1)
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
evaluate (const char *cmd)
{
  compile_program (cmd);
}

char *
output_asm (schptr_t sptr)
{
  const char *tmpdir = find_system_tmpdir ();
  char itemplate[FILE_PATH_MAX] = {
    0,
  };
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
  char *s = (char *)alloc (ssize);
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

  s[bytes_read] = '\0';

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

  (void)parse_whitespace (&cs);

  if (!parse_program (&cs, &sptr))
    err_parse (cs);

  // free parsed string
  free (s);

  char *asmtmp = output_asm (sptr);
  dump_asm_if_needed (asmtmp);

  // free expression
  free_expression (sptr);

  // Now compile file and link with runtime
  {
    int child = fork ();
    if (child == 0)
      {
        // inside child
#ifdef UBSANLIB
        execl (CC, CC, "-o", output, asmtmp, "runtime.o", UBSANLIB,
               (char *)NULL);
#else
        execl (CC, CC, "-o", output, asmtmp, "runtime.o", (char *)NULL);
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
      real_tmpdir[FILE_PATH_MAX - 1] = '\0';
    }
  return real_tmpdir;
}

void
compile_program (const char *e)
{
  schptr_t sptr = 0;

  (void)parse_whitespace (&e);

  if (!parse_program (&e, &sptr))
    err_parse (e);

  const char *tmpdir = find_system_tmpdir ();
  char otemplate[FILE_PATH_MAX] = {
    0,
  };
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

  // free expression
  free_expression (sptr);

  // close ofd so gcc can write to it
  close (ofd);

  // Now compile file and link with runtime
  {
    int child = fork ();
    if (child == 0)
      {
        // inside child
        execl (CC, CC, "-shared", "-fPIC", "-o", otemplate, asmtmp,
               "runtime.o", (char *)NULL);

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
        fprintf (stderr, "%s\n", dlerror ());
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
