#include <stdio.h>
#include <inttypes.h>

// Runtime entry point.
// The compiler generated code is linked here.

/* all scheme values are of type ptrs */
typedef uint64_t ptr;

extern ptr scheme_entry (void);

/* define all scheme constants */
#define boolf    0x2f
#define boolt    0x6f
#define null     0x3f

#define ch_mask  0xff
#define ch_tag   0x0f
#define ch_shift 8

#define fx_mask  0x03
#define fx_tag   0x00
#define fx_shift 2

static void
print_char (uint64_t code) {
  switch (code)
    {
    case 0x7:
      printf ("#\\alarm");
      break;
    case 0x8:
      printf ("#\\backspace");
      break;
    case 0x7f:
      printf ("#\\delete");
      break;
    case 0x1b:
      printf ("#\\escape");
      break;
    case 0xa:
      printf ("#\\newline");
      break;
    case 0x0:
      printf ("#\\null");
      break;
    case 0xd:
      printf ("#\\return");
      break;
    case ' ':
      printf ("#\\space");
      break;
    case 0x9:
      printf ("#\\tab");
      break;
    default:
      printf ("#\\%c", (char) code);
      break;
    }
}

static void
print_ptr(ptr x)
{
  if ((x & fx_mask) == fx_tag)
    printf ("%" PRIi64, (int64_t) ((uint64_t) x) >> fx_shift);
  else if ((x & ch_mask) == ch_tag)
    print_char (((uint64_t) x) >> ch_shift);
  else if (x == boolf)
    printf ("#f");
  else if (x == boolt)
    printf ("#t");
  else if (x == null)
    printf ("()");
  else
    printf ("#<unknown 0x%08lx>", x);
  printf ("\n");
}

int
main(void)
{
  print_ptr (scheme_entry ());
  return 0;
}
