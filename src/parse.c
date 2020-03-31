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

#include "parse.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "memory.h"
#include "err.h"
#include "structs.h"
#include "primitives.h"

// Comments behave like whitespaces so they are removed with it
bool
parse_whitespace (const char **input)
{
  const char *tmp = *input;
  while (isspace (**input) || **input == ';')
    {
      if (**input == ';')
        {
          (*input)++;
          while (**input && **input != '\n')
            (*input)++;
        }
      else
        (*input)++;
    }
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

  if (! **input)
    return false;

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
    {
      free_expression (identifier);
      return false;
    }

  // skip possible whitespace between expression and rparen
  (void) parse_whitespace (&ptr);

  if (!parse_rparen (&ptr))
    {
      free_expression (identifier);
      free_expression (expression);
      return false;
    }

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
parse_command (const char **input, schptr_t *sptr)
{
  // Syntax:
  // <command> -> <expression>

  return parse_expression(input, sptr);
}

bool
parse_command_or_definition (const char **input, schptr_t *sptr)
{
  // Syntax:
  // <command or definition> ->
  //          <command>
  // |        <definition>
  // |        (begin <command or definition>+)
  //
  // TODO: implement definition and (begin ...) support

  return parse_command (input, sptr);
}

bool
parse_program (const char **input, schptr_t *sptr)
{
  const char *ptr = *input;

  // Syntax:
  // <program> ->
  //          <import declaration>+
  // |        <command or definition>+
  //
  // TODO: implement import declaration support

  expression_list_t *elst = NULL;
  expression_list_t *last = NULL;
  schptr_t e;
  if (!parse_command_or_definition (&ptr, &e))
    return false;

  elst = alloc (sizeof (*elst));
  elst->expr = e;
  elst->next = NULL;
  last = elst;

  (void) parse_whitespace (&ptr);

  while (parse_command_or_definition (&ptr, &e))
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

  let_t t = LET;
  if (parse_char_sequence (&ptr, "let*"))
    t = LETS;
  else if (parse_char_sequence (&ptr, "letrec"))
    t = LETREC;
  else if (!parse_char_sequence (&ptr, "let"))
    return false;

  // skip possible whitespace between let and lparen
  (void) parse_whitespace (&ptr);

  if (!parse_lparen (&ptr))
    return false;

  // Now we parse each of the binding specs and store them
  binding_spec_list_t *bindings = NULL;
  binding_spec_list_t *last = NULL;
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
      b->next = NULL;

      if (!bindings)
        {
          bindings = b;
          last = b;
        }
      else
        {
          last->next = b;
          last = b;
        }
    }

  // skip possible whitespace between last binding spec and rparen
  (void) parse_whitespace (&ptr);

  if (!parse_rparen (&ptr))
    {
      free_binding_spec_list (bindings);
      return false;
    }

  // skip possible whitespace between rparen and body
  (void) parse_whitespace (&ptr);

  if (!parse_body (&ptr, &body))
    {
      free_binding_spec_list (bindings);
      return false;
    }

  // skip possible whitespace between body and rparen
  (void) parse_whitespace (&ptr);

  if (!parse_rparen (&ptr))
    {
      free_binding_spec_list (bindings);
      free_expression (body);
      return false;
    }

  // Parse of let successful and complete
  *input = ptr;

  schlet_t *l = (schlet_t *) alloc (sizeof (*l));
  l->type = SCH_LET;
  l->let = t;
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
  const char *ptr = *input;

  if (ptr[0] == '#' && ptr[1] == '\\')
    {
      unsigned char c = 0;
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
      parse_if (input, sptr) ||
      parse_let_wo_id (input, sptr) ||
      parse_procedure_call (input, sptr))
    return true;

  return false;
}

bool
parse_operator (const char **input, schptr_t *sptr)
{
  return parse_expression (input, sptr);
}

bool
parse_operand (const char **input, schptr_t *sptr)
{
  return parse_expression (input, sptr);
}

bool
parse_procedure_call (const char **input, schptr_t *sptr)
{
  const char *ptr = *input;
  // Syntax:
  // <procedure call> ->
  //          ( <operator> <operand>* )

  if (!parse_lparen (&ptr))
    return false;

  (void) parse_whitespace (&ptr);

  schptr_t op;
  if (!parse_operator (&ptr, &op))
    return false;

  (void) parse_whitespace (&ptr);

  // Parse zero or more arguments : the operands
  // Gathers a list of expressions with arguments
  expression_list_t *es = NULL;
  expression_list_t *last = es;
  schptr_t e;
  unsigned int noperands = 0;
  while (parse_operand (&ptr, &e))
    {
      noperands++;
      (void) parse_whitespace (&ptr);

      expression_list_t *node = alloc (sizeof *node);
      node->expr = e;
      node->next = NULL;

      if (!es)
        es = node;
      if (last)
        last->next = node;
      last = node;
    }

  if (!parse_rparen (&ptr))
    {
      free_expression_list (es);
      return false;
    }

  // Done with parsing procedure call
  *input = ptr;

  // Ensure that we support these types of procedure calls
  // Currently operator should be a primitive
  schid_t *id = (schid_t *) op;
  if (sch_imm_p (op) || id->type != SCH_ID)
    {
      fprintf (stderr, "unsupported operator type for procedure call\n");
      exit (EXIT_FAILURE);
    }

  const schprim_t *prim = NULL;
  for(size_t pi = 0; pi < primitives_count; pi++)
    {
      const schprim_t *p = &(primitives[pi]);
      if (!strcmp (id->name, p->name))
        {
          prim = p;
          break;
        }
    }

  if (!prim)
    {
      fprintf (stderr, "unknown primitive function `%s'\n", id->name);
      exit (EXIT_FAILURE);
    }

  if (prim->argcount != noperands)
    {
      fprintf (stderr, "too many arguments to `%s', expected %ud, got %ud\n",
               prim->name,
               prim->argcount,
               noperands);
      exit (EXIT_FAILURE);
    }

  // We found the primitive function we needed - time to free id
  free_identifier (id);

  // we only support primitives at the moment so transform the
  // procedure call into a primitive evaluation
  switch (noperands)
    {
    case 1:
      {
        schprim_eval1_t *p1 = alloc (sizeof *p1);
        p1->type = SCH_PRIM_EVAL1;
        p1->prim = prim;
        p1->arg1 = es->expr;
        *sptr = (schptr_t) p1;
      }
      break;
    case 2:
      {
        schprim_eval2_t *p2 = alloc (sizeof *p2);
        p2->type = SCH_PRIM_EVAL2;
        p2->prim = prim;
        p2->arg1 = es->expr;
        p2->arg2 = es->next->expr;
        *sptr = (schptr_t) p2;
      }
      break;
    default:
      err_unreachable ("primitive with more than 2 operands");
      break;
    }

  // free the expression list as we don't need it any more
  expression_list_t *tmp = es;
  while (tmp)
    {
      tmp = tmp->next;
      free (es);
      es = tmp;
    }

  return true;
}

bool
parse_formals (const char **input, lambda_formals_t **sptr)
{
  // Syntax:
  // <formals> ->
  //             ( <identifier>* )
  // |           ( <identifier>+ . <identifier> )
  // |           <identifier>

  const char *ptr = *input;

  if (parse_lparen (&ptr))
    {
      
    }

  schptr_t id;
  if (!parse_identifier (&ptr, &id))
    return false;

  
  return true;
}

bool
parse_lambda_expression (const char **input, schptr_t *sptr)
{
  // Syntax:
  // <lambda expression> ->
  //         ( lambda <formals> <body> )
  const char *ptr = *input;

  if (!parse_lparen (&ptr))
    return false;

  (void) parse_whitespace (&ptr);

  if (! parse_char_seq (&ptr, "lambda"))
    return false;

  (void) parse_whitespace (&ptr);

  lambda_formals_t *formals = NULL;
  if (! parse_formals (&ptr, &formals))
    return false;

  (void) parse_whitespace (&ptr);

  schptr_t body;
  if (! parse_body (&ptr, &body))
    return false;

  (void) parse_whitespace (&ptr);

  if (!parse_rparen (&ptr))
    return false;

  // create lambda structure

  return true;
}
