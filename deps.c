#include "deps.h"
#include "log.h"
#include "utils.h"
#include "paths.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>

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
    char *rp = d->buf;
    d->type = *rp;
    ++rp;
    d->resource = rp;
    for ( ; *rp && *rp != '\1'; ++rp) ;
    if (*rp == '\1') {
      *rp = 0;
      if (d->type == DEPS_TYPE_FILE) {
        ++rp;
        d->serial = utils_strtoll(rp, 10, &rc);
        if (rc) {
          akerror(rc, "Failed to read '%s' as number", rp);
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
    if (d->type == DEPS_TYPE_FILE) {
      struct akpath_stat st;
      if (path_stat(d->resource, &st) || st.ftype == AKPATH_NOT_EXISTS || st.mtime > d->serial) {
        return true;
      }
    } else if (d->type == DEPS_TYPE_OUTDATED) {
      return true;
    }
  }
  return false;
}

int deps_register(struct deps *d, int type, const char *file) {
  int rc = 0;
  int64_t serial = 0;
  char buf[PATH_MAX];

  if (type == DEPS_TYPE_FILE) {
    akassert(path_normalize(file, buf));
    file = buf;
    struct akpath_stat st;
    if (!path_stat(file, &st) && st.ftype != AKPATH_NOT_EXISTS) {
      serial = st.mtime;
    }
  }
  long int off = ftell(d->file);
  if (off < 0) {
    return errno;
  }
  fseek(d->file, 0, SEEK_END);
  if (fprintf(d->file, "%c%s\1%" PRId64 "\n", (char) type, file, serial) < 0) {
    rc = errno;
  }
  fseek(d->file, off, SEEK_SET);
  if (!rc) {
    ++d->num_registered;
  }
  return rc;
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
