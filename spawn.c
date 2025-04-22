#include "spawn.h"
#include "log.h"
#include "ulist.h"
#include "pool.h"
#include "xstr.h"

#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

extern char **environ;

struct spawn {
  pid_t pid;
  int   wstatus;
  void *user_data;
  struct pool *pool;
  struct ulist args;
  struct ulist env;
  const char  *exec;

  size_t (*stdin_provider)(char *buf, size_t buflen, struct spawn*);
  void   (*stdout_handler)(char *buf, size_t buflen, struct spawn*);
  void   (*stderr_handler)(char *buf, size_t buflen, struct spawn*);
};

struct spawn* spawn_create(const char *exec, void *user_data) {
  akassert(exec);
  struct pool *pool = pool_create_empty();
  struct spawn *s = pool_alloc(pool, sizeof(*s));
  *s = (struct spawn) {
    .pool = pool,
    .pid = -1,
    .args = {
      .usize = sizeof(char*)
    },
    .env = {
      .usize = sizeof(char*)
    },
    .user_data = user_data,
  };
  s->exec = spawn_arg_add(s, exec);
  return s;
}

const char* spawn_arg_add(struct spawn *s, const char *arg) {
  if (!arg) {
    arg = "";
  }
  const char *v = pool_strdup(s->pool, arg);
  ulist_push(&s->args, &v);
  return v;
}

void spawn_env_set(struct spawn *s, const char *key, const char *val) {
  akassert(key);
  if (!val) {
    val = "";
  }
  const char *v = pool_printf(s->pool, "%s=%s", key, val);
  ulist_push(&s->env, &v);
}

void spawn_env_path_prepend(struct spawn *s, const char *path) {
  const char *path_env = getenv("PATH");
  if (path_env) {
    struct xstr *xstr = xstr_create_empty();
    xstr_printf(xstr, "%s:%s", path, path_env);
    spawn_env_set(s, "PATH", xstr_ptr(xstr));
    xstr_destroy(xstr);
  } else {
    spawn_env_set(s, "PATH", path);
  }
}

static char* _env_find(struct spawn *s, const char *key, size_t keylen) {
  if (keylen == 0) {
    return 0;
  }
  for (int i = 0; i < s->env.num; ++i) {
    char *v = *(char**) ulist_get(&s->env, i);
    if (strncmp(v, key, keylen) == 0 && v[keylen] == '=') {
      ulist_remove(&s->env, i); // Entry is not needed anymore.
      return v;
    }
  }
  return 0;
}

static char** _env_create(struct spawn *s) {
  int i = 0, c = 0;

  for (char **ep = environ; *ep; ++ep, ++c);
  c += s->env.num;

  char **nenv = pool_alloc(s->pool, sizeof(*nenv) * (c + 1));
  nenv[c] = 0;

  for (char **it = environ; *it && i < c; ++it, ++i) {
    const char *entry = *it;
    const char *ep = strchr(entry, '=');
    if (ep == 0) {
      continue;
    }
    size_t keylen = ep - entry;
    char *my = _env_find(s, entry, keylen);
    if (my) {
      nenv[i] = my;
    } else {
      nenv[i] = (char*) pool_strdup(s->pool, entry);
    }
  }
  for (c = 0; c < s->env.num; ++c, ++i) {
    nenv[i] = *(char**) ulist_get(&s->env, i);
  }
  nenv[i] = 0;
  return nenv;
}

static char** _args_create(struct spawn *s) {
  char **args = pool_alloc(s->pool, sizeof(*args) * (s->args.num + 1));
  for (int i = 0; i < s->args.num; ++i) {
    char *v = *(char**) ulist_get(&s->args, i);
    args[i] = v;
  }
  args[s->args.num] = 0;
  return args;
}

void* spawn_user_data(struct spawn *s) {
  return s->user_data;
}

pid_t spawn_pid(struct spawn *s) {
  return s->pid;
}

int spawn_exit_code(struct spawn *s) {
  if (WIFEXITED(s->wstatus)) {
    return WEXITSTATUS(s->wstatus);
  }
  return -1;
}

void spawn_set_stdin_provider(
  struct spawn *s,
  size_t (*provider)(char *buf, size_t buflen, struct spawn*)) {
  s->stdin_provider = provider;
}

void spawn_set_stdout_handler(
  struct spawn *s,
  void (*handler)(char *buf, size_t buflen, struct spawn*)) {
  s->stdout_handler = handler;
}

static void _default_stdout_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stdout, "%s: %s", s->exec, buf);
}

static void _default_stderr_handler(char *buf, size_t buflen, struct spawn *s) {
  fprintf(stderr, "%s: %s", s->exec, buf);
}

void spawn_set_stderr_handler(
  struct spawn *s,
  void (*handler)(char *buf, size_t buflen, struct spawn*)) {
  s->stderr_handler = handler;
}

int spawn_do(struct spawn *s) {
  int rc = 0;

  if (!s->stderr_handler) {
    s->stderr_handler = _default_stderr_handler;
  }

  if (!s->stdout_handler) {
    s->stdout_handler = _default_stdout_handler;
  }

  int pipe_stdout[2] = { -1, -1 };
  int pipe_stderr[2] = { -1, -1 };
  int pipe_stdin[2] = { -1, -1 };

  char **envp = _env_create(s);
  char **args = _args_create(s);
  const char *file = args[0];

  char buf[1024];
  ssize_t len;

  if (pipe(pipe_stdout) == -1) {
    return errno;
  }
  if (pipe(pipe_stderr) == -1) {
    return errno;
  }
  if (s->stdin_provider) {
    if (pipe(pipe_stdin) == -1) {
      return errno;
    }
  }

  s->pid = fork();
  if (s->pid == -1) {
    return errno;
  }

  if (s->pid == 0) {
    if (pipe_stdin[0] != -1) {
      close(pipe_stdin[1]);
      if (dup2(pipe_stdin[0], STDIN_FILENO) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
      }
      close(pipe_stdin[0]);
    }

    close(pipe_stdout[0]);
    if (dup2(pipe_stdout[1], STDOUT_FILENO) == -1) {
      perror("dup2");
      _exit(EXIT_FAILURE);
    }
    close(pipe_stdout[1]);

    close(pipe_stderr[0]);
    if (dup2(pipe_stderr[1], STDERR_FILENO) == -1) {
      perror("dup2");
      _exit(EXIT_FAILURE);
    }
    close(pipe_stderr[1]);

    execve(file, args, envp);
    perror("execve");
    _exit(EXIT_FAILURE);
  } else {
    close(pipe_stdout[1]);
    close(pipe_stderr[1]);

    if (s->stdin_provider) {
      close(pipe_stdin[0]);
      ssize_t tow = 0;
      while ((tow = s->stdin_provider(buf, sizeof(buf), s)) > 0) {
        while (tow > 0) {
          len = write(pipe_stdin[1], buf, tow);
          if (len == -1 && errno == EAGAIN) {
            continue;
          }
          if (len > 0) {
            tow -= len;
          } else {
            break;
          }
        }
      }
      close(pipe_stdin[1]);
    }

    // Now read both stdout & stderr
    struct pollfd fds[] = {
      { .fd = pipe_stdout[0], .events = POLLIN },
      { .fd = pipe_stderr[0], .events = POLLIN }
    };

    int c = sizeof(fds) / sizeof(fds[0]);
    while (c > 0) {
      int ret = poll(fds, sizeof(fds) / sizeof(fds[0]), -1);
      if (ret == -1) {
        rc = ret;
        goto finish;
      }
      for (int i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i) {
        if (fds[i].events & POLLIN) {
          ssize_t n = read(fds[i].fd, buf, sizeof(buf) - 1);
          if (n > 0) {
            buf[n] = '\0';
            if (fds[i].fd == pipe_stdout[0]) {
              s->stdout_handler(buf, n, s);
            } else {
              s->stderr_handler(buf, n, s);
            }
          } else {
            close(fds[i].fd);
            fds[i].fd = -1;
            --c;
          }
        }
      }
    }

    if (waitpid(s->pid, &s->wstatus, 0) == -1) {
      perror("waitpid");
    }
  }

finish:
  if (rc) {
    akerror(rc, "Failed to spawn: %s", file);
  }
  return rc;
}

void spawn_destroy(struct spawn *s) {
  ulist_destroy_keep(&s->args);
  ulist_destroy_keep(&s->env);
  pool_destroy(s->pool);
}
