
#include <linux/limits.h>
#ifndef _AMALGAMATE_
#include "log.h"
#include "fetchreg.h"
#include "alloc.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>
#endif

struct fetchreg {
  FILE *f;
};

static void _fetchreg_destroy(struct fetchreg *r) {
  if (r) {
    if (r->f) {
      fclose(r->f);
    }
    free(r);
  }
}

int fetchreg_open(const char *path, struct fetchreg **out) {
  akassert(path && out);
  FILE *f = fopen(path, "a+");
  if (!f) {
    *out = 0;
    return errno;
  }
  struct fetchreg *r = xmalloc(sizeof(*r));
  r->f = f;
  *out = r;
  return 0;
}

bool fetchreg_find(
  struct fetchreg *r,
  const char *url,
  void *user_data,
  void (*cb)(const struct fetcherg_entry*, void*)) {
  akassert(r && url);

  char buf[PATH_MAX * 3 + 2];
  char sbuf[sizeof(buf)];
  struct fetcherg_entry se = { 0 };

  if (fseek(r->f, 0, SEEK_SET) == -1) {
    akerror(errno, "fetchreg for %s", url);
    return false;
  }

  // Find the last matched fetched entry
  while (fgets(buf, sizeof(buf), r->f)) {
    struct fetcherg_entry e = { 0 };
    int idxs[1] = { 0 };
    char *rp = buf;

    for ( ; *rp; ++rp) {
      if (*rp == '\1') {
        *rp = '\0';
        if (strcmp(buf, url) != 0) {
          break;
        }
        e.url = buf;
        if (rp[1] != '\0' && rp[1] != '\n') {
          e.target = rp + 1;
          idxs[0] = e.target - buf;
        }
        break;
      }
    }

    if (e.url) {
      memset(&se, 0, sizeof(se));
      memcpy(sbuf, buf, sizeof(buf));
      se.url = sbuf;
      if (idxs[0]) {
        char *target = sbuf + idxs[0];
        int rv = utils_endswith(target, "\n");
        if (rv) {
          target[rv - 1] = '\0';
        }
        se.target = target;
      }
    }
  }

  if (se.url) {
    if (cb) {
      cb(&se, user_data);
    }
    return true;
  } else {
    return false;
  }
}

int fetchreg_register(struct fetchreg *r, const struct fetcherg_entry *entry) {
  if (!r || !entry || !entry->url) {
    return AK_ERROR_INVALID_ARGS;
  }
  long int old_pos = ftell(r->f);
  if (fseek(r->f, SEEK_END, 0) == -1) {
    return errno;
  }
  if (entry->target) {
    if (fprintf(r->f, "%s\1%s\n", entry->url, entry->target) < 0) {
      return errno;
    }
  } else {
    if (fprintf(r->f, "%s\1\n", entry->url) < 0) {
      return errno;
    }
  }
  fseek(r->f, old_pos, SEEK_SET);
  fflush(r->f);
  return 0;
}

void fetchreg_close(struct fetchreg *r) {
  _fetchreg_destroy(r);
}
