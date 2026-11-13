// Project 2 regression test, authored for this repository (not a supplied grader).
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define TEST_BLOCKS 65803
#define TEST_BSIZE  1024

static uint data[3 * TEST_BSIZE / sizeof(uint)];

static void
fail(char *message)
{
  fprintf(2, "bigfile: FAIL: %s\n", message);
  exit(1);
}

int
main(int argc, char **argv)
{
  char *mode = argc > 1 ? argv[1] : "run";
  int blocks = strcmp(mode, "small") == 0 ? 600 : TEST_BLOCKS;
  int fd;
  struct stat st;

  if (strcmp(mode, "unlink") == 0) {
    if (unlink("big.file") < 0)
      fail("unlink");
  } else if (strcmp(mode, "truncate") == 0) {
    fd = open("big.file", O_WRONLY | O_TRUNC);
    if (fd < 0 || fstat(fd, &st) < 0 || st.size != 0)
      fail("truncate");
    close(fd);
  } else {
    if (strcmp(mode, "verify") != 0) {
      fd = open("big.file", O_CREATE | O_WRONLY | O_TRUNC);
      if (fd < 0)
        fail("create");
      int bn;
      for (bn = 0; bn < blocks;) {
        int count = blocks - bn < 3 ? blocks - bn : 3;
        for (int b = 0; b < count; b++)
          for (int j = 0; j < TEST_BSIZE / sizeof(uint); j++)
            data[b * (TEST_BSIZE / sizeof(uint)) + j] =
              (bn + b) ^ (j * 0x01010101U);
        if (write(fd, data, count * TEST_BSIZE) != count * TEST_BSIZE)
          break;
        bn += count;
        if (bn / 8192 != (bn - count) / 8192)
          printf("bigfile: progress %d\n", bn);
      }
      printf("wrote %d blocks\n", bn);
      if (bn != blocks)
        fail("maximum size is not 65803 blocks");
      if (blocks == TEST_BLOCKS && write(fd, data, TEST_BSIZE) != -1)
        fail("accepted block beyond maximum");
      if (fstat(fd, &st) < 0 ||
          st.size != (blocks == TEST_BLOCKS ? 67382272 : 614400))
        fail("file size");
      close(fd);
    }
    fd = open("big.file", O_RDONLY);
    if (fd < 0)
      fail("reopen");
    for (int bn = 0; bn < blocks; bn++) {
      if (read(fd, data, TEST_BSIZE) != TEST_BSIZE)
        fail("short read");
      for (int j = 0; j < TEST_BSIZE / sizeof(uint); j++)
        if (data[j] != (bn ^ (j * 0x01010101U)))
          fail("block contents");
    }
    if (read(fd, data, 1) != 0)
      fail("missing EOF");
    close(fd);
    if (strcmp(mode, "keep") != 0 && strcmp(mode, "verify") != 0)
      if (unlink("big.file") < 0)
        fail("unlink after read");
  }
  sync();
  printf("done; ok\n");
  exit(0);
}
