#include "spawn.h"
#include "env.h"
#include "log.h"
#include "ulist.h"
#include "alloc.h"
#include "pool.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

extern char **environ;

struct spawn {
  pid_t pid;
  int   wstatus;
  void *user_data;
  struct pool *pool;
  struct ulist args;
  struct ulist env;
  int  (*provider)(char *buf, size_t buflen, struct spawn*);
  void (*stdout_handler)(char *buf, size_t buflen, struct spawn*);
  void (*stderr_handler)(char *buf, size_t buflen, struct spawn*);
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
  spawn_arg_add(s, exec);
  return s;
}

void spawn_arg_add(struct spawn *s, const char *arg) {
  if (!arg) {
    arg = "";
  }
  const char *v = pool_strdup(s->pool, arg);
  ulist_push(&s->args, &v);
}

void spawn_env_set(struct spawn *s, const char *key, const char *val) {
  akassert(key);
  if (!val) {
    val = "";
  }
  const char *v = pool_printf(s->pool, "%s=%s", key, val);
  ulist_push(&s->env, &v);
}

static const char* _env_find(struct spawn *s, const char *key, size_t keylen) {
  if (keylen == 0) {
    return 0;
  }
  for (int i = 0; i < s->env.num; ++i) {
    const char *v = *(const char**) ulist_get(&s->env, i);
    if (strncmp(v, key, keylen) == 0 && v[keylen] == '=') {
      ulist_remove(&s->env, i); // Entry is not needed anymore.
      return v;
    }
  }
  return 0;
}

static const char** _env_create(struct spawn *s) {
  int i = 0, c = 0;

  for (char **ep = environ; *ep; ++ep, ++c);
  c += s->env.num;

  const char **nenv = pool_alloc(s->pool, c + 1);
  nenv[c] = 0;

  for (char **it = environ; *it && i < c; ++it, ++i) {
    const char *entry = *it;
    const char *ep = strchr(entry, '=');
    if (ep == 0) {
      continue;
    }
    size_t keylen = ep - entry;
    const char *my = _env_find(s, entry, keylen);
    if (my) {
      nenv[i] = my;
    } else {
      nenv[i] = pool_strdup(s->pool, entry);
    }
  }
  nenv[i] = 0;
  return nenv;
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
  int (*provider)(char *buf, size_t buflen, struct spawn*)) {
  s->provider = provider;
}

void spawn_set_stdout_handler(
  struct spawn *s,
  void (*handler)(char *buf, size_t buflen, struct spawn*)) {
  s->stdout_handler = handler;
}

void spawn_set_stderr_handler(
  struct spawn *s,
  void (*handler)(char *buf, size_t buflen, struct spawn*)) {
  s->stderr_handler = handler;
}

int spawn_do(struct spawn *s) {
  int rc = 0;
  int pipe_stdout[2];
  int pipe_stdin[2];
  char buf[1024];
  ssize_t len;

  // TODO:

  return rc;
}

void spawn_destroy(struct spawn *s) {
  ulist_destroy_keep(&s->args);
  ulist_destroy_keep(&s->env);
  pool_destroy(s->pool);
}
