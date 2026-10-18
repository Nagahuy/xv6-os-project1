// Test fixture only: linked into a temporary image by test_shell.py.
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  if (argc != 2)
    exit(2);
  if (strcmp(argv[1], "delayed") == 0) {
    pause(10);
    exit(0);
  }
  if (strcmp(argv[1], "invalid") == 0) {
    if (consolemode(-1) != -1 || consolemode(2) != -1)
      exit(1);
    printf("INVALID_OK\n");
    exit(0);
  }
  if (strcmp(argv[1], "kill") == 0) {
    int sync[2];
    if (pipe(sync) < 0)
      exit(1);
    int pid = fork();
    if (pid < 0)
      exit(1);
    if (pid == 0) {
      close(sync[0]);
      if (consolemode(1) < 0)
        exit(1);
      write(sync[1], "r", 1);
      char c;
      read(0, &c, 1);
      exit(1);
    }
    close(sync[1]);
    char c;
    if (read(sync[0], &c, 1) != 1)
      exit(1);
    kill(pid);
    wait(0);
    close(sync[0]);
    printf("KILL_OK\n");
    exit(0);
  }
  if (consolemode(1) < 0)
    exit(1);
  if (strcmp(argv[1], "owner") == 0) {
    int pid = fork();
    if (pid < 0)
      exit(1);
    if (pid == 0)
      exit(consolemode(0) == -1 && consolemode(1) == -1 ? 0 : 1);
    int status;
    wait(&status);
    if (status || consolemode(0) < 0)
      exit(1);
    printf("OWNER_OK\n");
  } else if (strcmp(argv[1], "exec") == 0) {
    char *args[] = {"cat", 0};
    exec("cat", args);
    exit(1);
  } else if (strcmp(argv[1], "exit") == 0) {
    printf("RAW_EXIT\n");
  } else {
    exit(2);
  }
  exit(0);
}
