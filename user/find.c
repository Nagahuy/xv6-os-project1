#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

void run_cmd(char **cmd_argv, char *filename);
void find(char *path, char *target_name, char **cmd_argv);

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "Usage: find [path] [target_name] [-exec cmd_args...]\n");
    exit(1);
  }

  char **cmd_argv = 0;
  if (argc > 3) {
    if (strcmp(argv[3], "-exec") != 0) {
      fprintf(2, "find: invalid argument %s\n", argv[3]);
      exit(1);
    }
    if (argc < 5) {
      fprintf(2, "find: missing command after -exec\n");
      exit(1);
    }
    cmd_argv = &argv[4];
  }

  find(argv[1], argv[2], cmd_argv);
  exit(0);
}

void
run_cmd(char **cmd_argv, char *filename)
{
  char *new_argv[MAXARG];
  int i = 0;

  while (i < MAXARG - 2 && cmd_argv[i] != 0) {
    new_argv[i] = cmd_argv[i];
    i++;
  }
  new_argv[i] = filename;
  new_argv[i + 1] = 0;

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "find: fork failed\n");
    return;
  }
  if (pid == 0) {
    exec(new_argv[0], new_argv);
    fprintf(2, "find: exec %s failed\n", new_argv[0]);
    exit(1);
  }
  wait(0);
}

void
find(char *path, char *target_name, char **cmd_argv)
{
  int fd;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  char *end = path + strlen(path);
  while (end > path && end[-1] == '/')
    end--;
  char *name = end;
  while (name > path && name[-1] != '/')
    name--;
  if (strlen(target_name) == end - name &&
      memcmp(name, target_name, end - name) == 0) {
    if (cmd_argv == 0)
      printf("%s\n", path);
    else
      run_cmd(cmd_argv, path);
  }

  if (st.type == T_DIR) {
    struct dirent de;
    char buf[512];
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      fprintf(2, "find: path too long\n");
      close(fd);
      return;
    }
    strcpy(buf, path);
    char *p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
        continue;
      find(buf, target_name, cmd_argv);
    }
  }
  close(fd);
}
