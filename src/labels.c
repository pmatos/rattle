#include "labels.h"

#include <stdio.h>
#include <string.h>

void
gen_new_temp_label (char *str)
{
  static size_t lcount = 0;
  const char prefix[] = ".LTrattle";

  sprintf (str, "%s%zu", prefix, lcount++);
}

char *
gen_new_label (void)
{
  char label[LABEL_MAX];
  gen_new_temp_label (label);
  return strdup (label);
}
