// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/elf.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS  10
#define MAXLINE  128 // Includes the terminating NUL; matches console capacity.
#define NHISTORY 10

static int interactive;
static struct {
  int number;
  char line[MAXLINE];
} history[NHISTORY];
static int histstart, histcount;
static int nextnumber = 1;

static int
histslot(int index)
{
  return (histstart + index) % NHISTORY;
}

static void
savehistory(char *line)
{
  int slot;
  if (histcount == NHISTORY) {
    slot = histstart;
    histstart = (histstart + 1) % NHISTORY;
  } else {
    slot = histslot(histcount++);
  }
  history[slot].number = nextnumber++;
  strcpy(history[slot].line, line);
}

// Store expanded commands, so recalling !! can never recursively recall !!.
static int
expandhistory(char *line)
{
  if (line[0] != '!')
    return 0;

  int index = -1;
  if (strcmp(line, "!!") == 0) {
    if (histcount == 0) {
      fprintf(2, "history: no previous command\n");
      return -1;
    }
    index = histcount - 1;
  } else {
    int number = 0;
    char *p = line + 1;
    if (*p == 0)
      goto missing;
    for (; *p; p++) {
      if (*p < '0' || *p > '9' || number > (0x7fffffff - (*p - '0')) / 10)
        goto missing;
      number = number * 10 + (*p - '0');
    }
    for (int i = 0; i < histcount; i++)
      if (history[histslot(i)].number == number)
        index = i;
  }
  if (index < 0)
    goto missing;
  strcpy(line, history[histslot(index)].line);
  printf("%s\n", line);
  return 0;

missing:
  fprintf(2, "history: event not found: %s\n", line);
  return -1;
}

// Clear only the typed characters, leaving the prompt in place.
static void
replaceline(char *buf, int *len, char *replacement)
{
  for (int i = 0; i < *len; i++)
    write(2, "\b \b", 3);
  strcpy(buf, replacement);
  *len = strlen(buf);
  write(2, buf, *len);
}

static int
prefixmatch(char *name, char *prefix)
{
  while (*prefix)
    if (*prefix++ != *name++)
      return 0;
  return 1;
}

// There are no executable permission bits in xv6: identify ELF programs.
static int
isprogram(char *path)
{
  uint magic;
  struct stat st;
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return 0;
  // In particular, never read /console while looking for executables: that
  // would block Tab completion and consume the next keyboard character.
  int ok = fstat(fd, &st) == 0 && st.type == T_FILE &&
           read(fd, &magic, sizeof(magic)) == sizeof(magic) &&
           magic == ELF_MAGIC;
  close(fd);
  return ok;
}

// Stream matches rather than keeping an arbitrarily sized list of filenames.
static void
addmatch(char *name, char *first, int *count)
{
  if (*count == 0) {
    strcpy(first, name);
  } else {
    if (*count == 1)
      fprintf(2, "\n%s", first);
    fprintf(2, " %s", name);
  }
  (*count)++;
}

static void
complete(char *buf, int *len, int nbuf)
{
  int start = *len;
  while (start > 0 && !strchr(" \t<|>&;()", buf[start - 1]))
    start--;
  int command = 1;
  for (int i = 0; i < start; i++)
    if (buf[i] != ' ' && buf[i] != '\t')
      command = 0;

  char dir[MAXLINE], path[MAXLINE + DIRSIZ + 2];
  char first[DIRSIZ + 1], name[DIRSIZ + 1];
  char *prefix = buf + start;
  int slash = -1;
  for (int i = start; i < *len; i++)
    if (buf[i] == '/')
      slash = i;
  if (slash >= 0) {
    int size = slash - start + 1;
    memmove(dir, buf + start, size);
    dir[size] = 0;
    prefix = buf + slash + 1;
  } else {
    strcpy(dir, command ? "/" : ".");
  }

  int count = 0;
  int fd = open(dir, O_RDONLY);
  struct stat st;
  struct dirent de;
  if (fd >= 0) {
    if (fstat(fd, &st) == 0 && st.type == T_DIR) {
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
          continue;
        memmove(name, de.name, DIRSIZ);
        name[DIRSIZ] = 0;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ||
            !prefixmatch(name, prefix))
          continue;
        strcpy(path, dir);
        int n = strlen(path);
        if (n && path[n - 1] != '/')
          path[n++] = '/';
        strcpy(path + n, name);
        if (!command || isprogram(path))
          addmatch(name, first, &count);
      }
    }
    close(fd);
  }
  // Built-ins have no executable file, but are also usable commands.
  if (command && slash < 0) {
    if (prefixmatch("cd", prefix))
      addmatch("cd", first, &count);
    if (prefixmatch("history", prefix))
      addmatch("history", first, &count);
  }
  if (count == 1) {
    char *suffix = first + strlen(prefix);
    int extra = strlen(suffix);
    if (*len + extra < nbuf) {
      strcpy(buf + *len, suffix);
      *len += extra;
      write(2, suffix, extra);
    }
  } else if (count > 1) {
    fprintf(2, "\n$ %s", buf);
  }
}

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);
void runcmd(struct cmd *) __attribute__((noreturn));

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit(1);

  switch (cmd->type) {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0) {
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    if (pipe(p) < 0)
      panic("pipe");
    if (fork1() == 0) {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0) {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  memset(buf, 0, nbuf);
  if (!interactive) {
    // Consume a whole physical line, even if it is too long. Never execute
    // the tail of an oversized script line as a separate command.
    for (;;) {
      int n = 0, overflow = 0, cc;
      char c;
      while ((cc = read(0, &c, 1)) == 1 && c != '\n') {
        if (n + 1 < nbuf)
          buf[n++] = c;
        else
          overflow = 1;
      }
      buf[n] = 0;
      if (cc < 0 || (cc == 0 && n == 0 && !overflow))
        return -1;
      if (!overflow)
        return 0;
      fprintf(2, "sh: line too long\n");
    }
  }

  if (consolemode(1) < 0) {
    fprintf(2, "sh: cannot enable console input\n");
    return -1;
  }
  write(2, "$ ", 2);
  int len = 0, position = histcount, escape = 0, eof = 0;
  char draft[MAXLINE];
  draft[0] = 0;

  for (;;) {
    uchar c;
    if (read(0, &c, 1) != 1) {
      eof = 1;
      break;
    }
    if (escape == 1) {
      escape = (c == '[' || c == 'O') ? 2 : 0;
      if (escape)
        continue;
    } else if (escape == 2) {
      // Ignore parameters (e.g. modified arrow keys) until the final byte.
      if (c >= 0x30 && c <= 0x3f)
        continue;
      escape = 0;
      if (c == 'A' && position > 0) {
        if (position == histcount)
          strcpy(draft, buf);
        position--;
        replaceline(buf, &len, history[histslot(position)].line);
      } else if (c == 'B' && position < histcount) {
        position++;
        replaceline(buf, &len,
                    position == histcount ? draft
                                          : history[histslot(position)].line);
      }
      continue;
    }

    if (c == '\033') {
      escape = 1;
    } else if (c == '\n' || c == '\r' || c == 4) {
      eof = c == 4 && len == 0; // Ctrl-D on an empty line exits this shell.
      write(2, "\n", 1);
      break;
    } else if (c == '\t') {
      complete(buf, &len, nbuf);
    } else if (c == '\b' || c == 127) {
      if (len > 0) {
        buf[--len] = 0;
        write(2, "\b \b", 3);
      }
    } else if (c == 21) { // Ctrl-U, as in the original cooked console.
      replaceline(buf, &len, "");
    } else if (c >= ' ' && c < 127 && len + 1 < nbuf) {
      buf[len++] = c;
      buf[len] = 0;
      write(2, &c, 1);
    }
  }
  consolemode(0);
  return eof ? -1 : 0;
}

int
main(void)
{
  static char buf[MAXLINE];
  int fd;
  struct stat st;

  // Ensure that three file descriptors are open.
  while ((fd = open("console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  interactive = fstat(0, &st) == 0 && st.type == T_DEVICE;

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0) {
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    // Normalize into the full-size buffer before a longer history expansion.
    memmove(buf, cmd, strlen(cmd) + 1);
    cmd = buf;
    int n = strlen(cmd);
    while (n > 0 && strchr(" \t\r\n", cmd[n - 1]))
      cmd[--n] = 0;
    if (*cmd == 0)
      continue;
    if (expandhistory(cmd) < 0)
      continue;
    savehistory(cmd);

    if (strcmp(cmd, "history") == 0) {
      for (int i = 0; i < histcount; i++) {
        int slot = histslot(i);
        printf("%d %s\n", history[slot].number, history[slot].line);
      }
    } else if (strcmp(cmd, "cd") == 0 || (cmd[0] == 'c' && cmd[1] == 'd' &&
                                          (cmd[2] == ' ' || cmd[2] == '\t'))) {
      // Chdir must be called by the parent, not the child.
      char *path = cmd + 2;
      while (*path == ' ' || *path == '\t')
        path++;
      if (*path == 0)
        path = "/";
      if (chdir(path) < 0)
        fprintf(2, "cannot cd %s\n", path);
    } else {
      if (fork1() == 0)
        runcmd(parsecmd(cmd));
      wait(0);
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd *
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s) {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>') {
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if (peek(ps, es, ";")) {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd *
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>")) {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok) {
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
      break;
    case '+': // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd *
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd *)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd *
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type) {
  case EXEC:
    ecmd = (struct execcmd *)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
