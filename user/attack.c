#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define PREFIX "Here it is: "
#define PREFIX_LEN 12
#define TOTAL_SIZE (40 * 4096)

int
main(int argc, char *argv[])
{
  char *mem = sbrk(TOTAL_SIZE);
  if (mem == (char*)-1) {
    exit(1);
  }
  for (int i = 0; i <= TOTAL_SIZE - PREFIX_LEN; i++) {
    if (memcmp(mem + i, PREFIX, PREFIX_LEN) == 0) {
      char *secret = mem + i + PREFIX_LEN;
      printf("%s\n", secret);
      exit(0);
    }
  }

  exit(1);
}
