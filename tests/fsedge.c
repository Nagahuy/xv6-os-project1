// Run only on the synthetic full/fragmented disk from run_project2.py.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static char data[3072];

static void
check(int ok, char *message)
{
  if (!ok) {
    printf("fsedge: FAIL: %s\n", message);
    exit(1);
  }
}

int
main(int argc, char **argv)
{
  if (argc > 1) {
    int fd =
      open("partial", O_RDWR | (strcmp(argv[1], "reclaim") == 0 ? O_TRUNC : 0));
    check(fd >= 0, "partial fixture open");
    struct stat st;
    if (strcmp(argv[1], "reclaim") == 0) {
      check(fstat(fd, &st) == 0 && st.size == 0, "partial truncate");
    } else {
      for (int i = 0; i < 267; i++)
        check(read(fd, data, 1024) == 1024, "partial fixture read");
      check(write(fd, data, 1024) == -1, "double-indirect ENOSPC");
      check(fstat(fd, &st) == 0 && st.size == 273408,
            "failed write changed size");
    }
    close(fd);
    sync();
    printf("fsedge: partial case passed\n");
    exit(0);
  }

  // There are no free data/index blocks: creation must roll back cleanly.
  for (int i = 0; i < 60; i++)
    check(symlink("frag.data", "no-space") == -1, "symlink ENOSPC");
  check(open("no-space", O_RDONLY | O_NOFOLLOW) == -1,
        "failed symlink published");
  int fd = open("empty", O_RDWR | O_CREATE);
  check(fd >= 0, "empty file");
  check(write(fd, data, 1024) == -1, "write ENOSPC");
  close(fd);
  unlink("empty");

  int ready[2], go[2];
  check(pipe(ready) == 0 && pipe(go) == 0, "pipes");
  for (int i = 0; i < 2; i++) {
    int pid = fork();
    check(pid >= 0, "fork");
    if (pid == 0) {
      close(ready[0]);
      close(go[1]);
      check(write(ready[1], "r", 1) == 1, "ready");
      check(read(go[0], data, 1) == 1, "go");
      char *path = i ? "frag.w1" : "frag.w0";
      for (int j = 0; j < 100; j++) {
        fd = open(path, O_WRONLY);
        check(fd >= 0, "writer open");
        check(write(fd, data, sizeof(data)) == sizeof(data),
              "concurrent overwrite");
        close(fd);
      }
      exit(0);
    }
  }
  close(ready[1]);
  close(go[0]);
  check(read(ready[0], data, 1) == 1 && read(ready[0], data, 1) == 1,
        "children ready");
  check(write(go[1], "gg", 2) == 2, "start writers");
  fd = open("frag.data", O_WRONLY | O_TRUNC);
  struct stat st;
  check(fd >= 0 && fstat(fd, &st) == 0 && st.size == 0, "fragmented truncate");
  close(fd);
  for (int i = 0; i < 2; i++) {
    int status;
    check(wait(&status) > 0 && status == 0, "writer failed");
  }
  close(ready[0]);
  close(go[1]);
  sync();
  printf("fsedge: all tests passed\n");
  exit(0);
}
