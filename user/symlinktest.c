// Project 2 behavioral tests; written for this base, not a supplied grader.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static void
check(int condition, char *message)
{
  if (!condition) {
    fprintf(2, "symlinktest: FAIL: %s\n", message);
    exit(1);
  }
}

static void
putfile(char *path, char *text)
{
  int fd = open(path, O_CREATE | O_RDWR | O_TRUNC);
  check(fd >= 0, "create target");
  check(write(fd, text, strlen(text)) == strlen(text), "write target");
  close(fd);
}

static void
contents(char *path, int flags, char *expected, int length)
{
  char buf[MAXPATH + 1];
  int fd = open(path, flags);
  check(fd >= 0, "open");
  int n = read(fd, buf, sizeof(buf));
  check(n == length && memcmp(buf, expected, length) == 0, "read contents");
  close(fd);
}

static void
badopen(char *path)
{
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
    close(fd);
  check(fd < 0, "open must fail");
}

static void
basic(void)
{
  putfile("sl-target", "hello");
  check(symlink("sl-target", "sl-link") == 0, "create symlink");
  contents("sl-link", O_RDONLY, "hello", 5);
  contents("sl-link", O_RDONLY | O_CREATE, "hello", 5);
  contents("sl-link", O_RDONLY | O_NOFOLLOW, "sl-target", 10);
  int fd = open("sl-link", O_RDONLY | O_NOFOLLOW);
  struct stat st;
  check(fd >= 0 && fstat(fd, &st) == 0 && st.type == T_SYMLINK,
        "nofollow type");
  close(fd);
  check(symlink("sl-target", "sl-link") == -1, "refuse existing path");
  check(symlink("missing", "sl-target") == -1, "preserve existing file");
  check(symlink("sl-missing", "sl-dangling") == 0, "create dangling link");
  badopen("sl-dangling");
  fd = open("sl-dangling", O_CREATE | O_RDWR);
  check(fd < 0, "create must not silently replace a dangling link");
  putfile("sl-missing", "later");
  contents("sl-dangling", O_RDONLY, "later", 5);
  check(unlink("sl-dangling") == 0, "unlink link");
  contents("sl-missing", O_RDONLY, "later", 5);
  check(link("sl-link", "sl-hard") == 0, "hardlink symlink inode");
  check(unlink("sl-link") == 0, "remove original link name");
  contents("sl-hard", O_RDONLY | O_NOFOLLOW, "sl-target", 10);
  contents("sl-hard", O_RDONLY, "hello", 5);
  check(unlink("sl-target") == 0, "unlink target");
  badopen("sl-hard");
  contents("sl-hard", O_RDONLY | O_NOFOLLOW, "sl-target", 10);
  unlink("sl-hard");
  unlink("sl-missing");

  putfile("sl-target", "erase");
  check(symlink("sl-target", "sl-link") == 0, "truncate fixture");
  fd = open("sl-link", O_WRONLY | O_TRUNC);
  check(fd >= 0, "truncate through symlink");
  close(fd);
  contents("sl-target", O_RDONLY, "", 0);
  contents("sl-link", O_RDONLY | O_TRUNC | O_NOFOLLOW, "sl-target", 10);
  unlink("sl-link");
  unlink("sl-target");
  printf("test symlinks: ok\n");
}

static void
chains(void)
{
  putfile("sl-target", "chain");
  char name[] = "sl0", target[] = "sl0";
  for (int i = 9; i >= 0; i--) {
    name[2] = '0' + i;
    target[2] = '0' + i + 1;
    check(symlink(i == 9 ? "sl-target" : target, name) == 0, "chain creation");
  }
  contents("sl0", O_RDONLY, "chain", 5);
  check(symlink("sl0", "sl-extra") == 0, "11th link");
  badopen("sl-extra");
  check(symlink("sl-self", "sl-self") == 0, "self cycle creation");
  check(symlink("sl-b", "sl-a") == 0 && symlink("sl-a", "sl-b") == 0,
        "cross cycle creation");
  for (int i = 0; i < 30; i++) {
    badopen("sl-self");
    badopen("sl-a");
    badopen("sl-extra");
  }
  for (int i = 0; i < 10; i++) {
    name[2] = '0' + i;
    unlink(name);
  }
  unlink("sl-extra");
  unlink("sl-self");
  unlink("sl-a");
  unlink("sl-b");
  unlink("sl-target");
  printf("test symlink chains and cycles: ok\n");
}

static void
errors(void)
{
  char longpath[MAXPATH + 1];
  memset(longpath, 'x', sizeof(longpath));
  longpath[MAXPATH] = 0;
  check(symlink(longpath, "sl-bad") == -1, "oversized target");
  check(symlink("target", longpath) == -1, "oversized link path");
  check(symlink((char *)0xffffffffffffffffULL, "sl-bad") == -1,
        "bad user pointer");
  check(symlink("", "sl-empty") == -1, "empty target");
  check(symlink("x", "no-parent/sl") == -1, "missing parent");
  check(symlink("x", "") == -1, "empty path");
  putfile("sl-target", "safe");
  check(symlink("sl-target", "sl-link") == 0, "malformed fixture");
  int fd = open("sl-link", O_WRONLY | O_NOFOLLOW);
  check(fd >= 0, "open link for malformed data");
  char broken[10];
  memset(broken, 'x', sizeof(broken));
  check(write(fd, broken, sizeof(broken)) == sizeof(broken),
        "corrupt target payload");
  close(fd);
  badopen("sl-link");
  unlink("sl-link");

  check(symlink("sl-target", "sl-link") == 0, "fd exhaustion fixture");
  int fds[NOFILE], n = 0;
  while (n < NOFILE && (fd = open("sl-link", O_RDONLY)) >= 0)
    fds[n++] = fd;
  check(n > 0 && n < NOFILE, "fd exhaustion reached");
  for (int i = 0; i < n; i++)
    close(fds[i]);
  contents("sl-link", O_RDONLY, "safe", 4);
  unlink("sl-link");
  unlink("sl-target");
  printf("test symlink errors and cleanup: ok\n");
}

static void
concurrent(void)
{
  putfile("sl-target", "hello");
  for (int child = 0; child < 3; child++) {
    int pid = fork();
    check(pid >= 0, "fork");
    if (pid == 0) {
      char path[] = "sl-worker0";
      path[9] = '0' + child;
      for (int i = 0; i < 50; i++) {
        check(symlink("sl-target", path) == 0, "concurrent create");
        contents(path, O_RDONLY, "hello", 5);
        check(unlink(path) == 0, "concurrent unlink");
        symlink("sl-target", "sl-race");
        int fd = open("sl-race", O_RDONLY);
        if (fd >= 0) {
          char buf[5];
          check(read(fd, buf, 5) == 5 && memcmp(buf, "hello", 5) == 0,
                "shared link never exposes incomplete target");
          close(fd);
        }
        unlink("sl-race");
      }
      exit(0);
    }
  }
  for (int i = 0; i < 3; i++) {
    int status;
    check(wait(&status) > 0 && status == 0, "concurrent child failed");
  }
  unlink("sl-race");
  unlink("sl-target");
  printf("test concurrent symlinks: ok\n");
}

int
main(void)
{
  basic();
  chains();
  errors();
  concurrent();
  sync();
  printf("symlinktest: all tests passed\n");
  exit(0);
}
