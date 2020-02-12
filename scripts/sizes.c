#include <stdio.h>
#include <stdint.h>

int
main (void)
{
  printf ("System is %zu bit\n", sizeof(void *) * 8);
  printf ("sizeof(uintptr_t): %zu\n", sizeof(uintptr_t));
  printf ("sizeof(uint64_t): %zu\n", sizeof(uint64_t));
  return 0;
}
