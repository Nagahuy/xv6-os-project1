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
  char buf[MAXPATH];
  struct {
    int len;
    int entries;
  } dirs[MAXPATH / 2]; // Each level adds at least a slash and one name byte.
  int depth = 0;
  int fd;
  struct stat st;
  struct dirent de;

  if (strlen(path) >= sizeof(buf)) {
    fprintf(2, "find: path too long\n");
    return;
  }
  strcpy(buf, path);
  dirs[0].len = strlen(buf);
  dirs[0].entries = 0;

visit:
  if ((fd = open(buf, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", buf);
    goto backtrack;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", buf);
    close(fd);
    goto backtrack;
  }

  char *end = buf + dirs[depth].len;
  while (end > buf && end[-1] == '/')
    end--;
  char *name = end;
  while (name > buf && name[-1] != '/')
    name--;
  if (dirs[depth].entries == 0 && strlen(target_name) == end - name &&
      memcmp(name, target_name, end - name) == 0) {
    if (cmd_argv == 0)
      printf("%s\n", buf);
    else
      run_cmd(cmd_argv, buf);
  }

  if (st.type == T_DIR) {
    // Reopen and resume the parent so traversal keeps only one descriptor.
    for (int i = 0; i < dirs[depth].entries; i++) {
      if (read(fd, &de, sizeof(de)) != sizeof(de))
        break;
    }
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      dirs[depth].entries++;
      if (de.inum == 0)
        continue;
      if (de.name[0] == '.' &&
          (de.name[1] == 0 || (de.name[1] == '.' && de.name[2] == 0)))
        continue;
      int n = 0;
      while (n < DIRSIZ && de.name[n])
        n++;
      int len = dirs[depth].len;
      if (len + 1 + n + 1 > sizeof(buf)) {
        fprintf(2, "find: path too long\n");
        continue;
      }
      buf[len] = '/';
      memmove(buf + len + 1, de.name, n);
      buf[len + 1 + n] = 0;
      close(fd);
      depth++;
      dirs[depth].len = len + 1 + n;
      dirs[depth].entries = 0;
      goto visit;
    }
  }
  close(fd);

backtrack:
  if (depth > 0) {
    depth--;
    buf[dirs[depth].len] = 0;
    goto visit;
  }
}
