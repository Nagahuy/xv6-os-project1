#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define PREFIX "Here it is: "
#define PREFIX_LEN (sizeof(PREFIX) - 1)
#define TOTAL_SIZE (40 * PGSIZE)

int
main(int argc, char *argv[])
{
  char *mem = sbrk(TOTAL_SIZE);
  if (mem == SBRK_ERROR) {
    exit(1);
  }
  for (int i = 0; i <= TOTAL_SIZE - PREFIX_LEN; i++) {
    if (memcmp(mem + i, PREFIX, PREFIX_LEN) == 0) {
      char *secret = mem + i + PREFIX_LEN;
      char *end = secret;
      while (end < mem + TOTAL_SIZE && *end != 0)
        end++;
      if (end > secret && end < mem + TOTAL_SIZE) {
        printf("%s\n", secret);
        exit(0);
      }
    }
  }

  exit(1);
}
