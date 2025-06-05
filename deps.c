#ifndef _AMALGAMATE_
#include "deps.h"
#include "log.h"
#include "utils.h"
#include "paths.h"
#include "env.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#endif

int deps_open(const char *path, int omode, struct deps *d) {
  int rc = 0;
  akassert(path && d);
  memset(d, 0, sizeof(*d));
  d->file = fopen(path, (omode & DEPS_OPEN_TRUNCATE) ? "w+" : ((omode & DEPS_OPEN_READONLY) ? "r" : "a+"));
  if (!d->file) {
    return errno;
  }
  if (fseek(d->file, 0, SEEK_SET) < 0) {
    return errno;
  }
  return rc;
}

bool deps_cur_next(struct deps *d) {
  int rc;
  if (d && d->file) {
    if (!fgets(d->buf, sizeof(d->buf), d->file)) {
      return false;
    }

    char *ls = 0;
    char *rp = d->buf;
    d->type = *rp++;
    d->flags = *rp++;

    if (d->type == DEPS_TYPE_ALIAS || d->type == DEPS_TYPE_ENV) {
      d->alias = rp;
      d->resource = 0;
    } else {
      d->resource = rp;
      d->alias = 0;
    }

    while (*rp) {
      if (*rp == '\1') {
        if (!d->resource) {
          *rp = '\0';
          d->resource = rp + 1;
        }
        ls = rp;
      }
      ++rp;
    }

    if (ls) {
      *ls = '\0';
      if (d->type == DEPS_TYPE_FILE || d->type == DEPS_TYPE_NODE_VALUE || d->type == DEPS_TYPE_ALIAS) {
        ++ls;
        d->serial = utils_strtoll(ls, 10, &rc);
        if (rc) {
          akerror(rc, "Failed to read '%s' as number", ls);
          return false;
        }
      }
    }

    return true;
  }

  return false;
}

bool deps_cur_is_outdated(struct deps *d) {
  if (d) {
    switch (d->type) {
      case DEPS_TYPE_FILE: {
        struct akpath_stat st;
        if (path_stat(d->resource, &st) || st.ftype == AKPATH_NOT_EXISTS || st.mtime > d->serial) {
          return true;
        }
        break;
      }
      case DEPS_TYPE_ALIAS: {
        struct akpath_stat st;
        if (path_stat(d->alias, &st) || st.ftype == AKPATH_NOT_EXISTS || st.mtime > d->serial) {
          return true;
        }
        break;
      }
      case DEPS_TYPE_ENV: {
        struct unit *unit = unit_peek();
        const char *val = unit_env_get(unit, d->alias);
        if (!val) {
          val = "";
        }
        return strcmp(val, d->resource) != 0;
      }
      case DEPS_TYPE_OUTDATED:
        return true;
    }
  }
  return false;
}

static int _deps_add(struct deps *d, char type, char flags, const char *resource, const char *alias, int64_t serial) {
  int rc = 0;
  char buf[2][PATH_MAX];

  if (flags == 0) {
    flags = ' ';
  }
  if (type == DEPS_TYPE_FILE) {
    path_normalize(resource, buf[0]);
    resource = buf[0];
    struct akpath_stat st;
    if (!path_stat(resource, &st) && st.ftype != AKPATH_NOT_EXISTS) {
      serial = st.mtime;
    }
  } else if (type == DEPS_TYPE_ALIAS) {
    path_normalize(resource, buf[0]);
    path_normalize(alias, buf[1]);
    resource = buf[0];
    alias = buf[1];
    struct akpath_stat st;
    if (!path_stat(alias, &st) && st.ftype != AKPATH_NOT_EXISTS) {
      serial = st.mtime;
    }
  }

  long int off = ftell(d->file);
  if (off < 0) {
    return errno;
  }
  fseek(d->file, 0, SEEK_END);

  if (type != DEPS_TYPE_ALIAS && type != DEPS_TYPE_ENV) {
    if (fprintf(d->file, "%c%c%s\1%" PRId64 "\n", type, flags, resource, serial) < 0) {
      rc = errno;
    }
  } else {
    if (fprintf(d->file, "%c%c%s\1%s\1%" PRId64 "\n", type, flags, alias, resource, serial) < 0) {
      rc = errno;
    }
  }

  fseek(d->file, off, SEEK_SET);
  if (!rc) {
    ++d->num_registered;
  }
  return rc;
}

int deps_add(struct deps *d, char type, char flags, const char *resource, int64_t serial) {
  return _deps_add(d, type, flags, resource, 0, serial);
}

int deps_add_alias(struct deps *d, char flags, const char *resource, const char *alias) {
  return _deps_add(d, DEPS_TYPE_ALIAS, flags, resource, alias, 0);
}

int deps_add_env(struct deps *d, char flags, const char *key, const char *value) {
  return _deps_add(d, DEPS_TYPE_ENV, flags, value, key, 0);
}

void deps_close(struct deps *d) {
  if (d && d->file) {
    fclose(d->file);
    d->file = 0;
  }
}

void deps_prune_all(const char *path) {
  unlink(path);
}
