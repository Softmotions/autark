
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

/// Splits a single registry line into url, target and build id parts.
/// Modifies the given buffer. On invalid input `url` is zeroed.
static void _fetchreg_parse_line(char *buf, struct fetcherg_entry *e) {
  e->url = 0;
  e->target = 0;
  e->build_id = 0;
  char *sep = strchr(buf, '\1');
  if (!sep || sep == buf) {
    return;
  }
  *sep = '\0';
  char *target = sep + 1;
  char *sep2 = strchr(target, '\1');
  if (sep2) {
    *sep2 = '\0';
    char *build_id = sep2 + 1;
    int rv = utils_endswith(build_id, "\n");
    if (rv) {
      build_id[rv - 1] = '\0';
    }
    if (*build_id) {
      e->build_id = build_id;
    }
  }
  int rv = utils_endswith(target, "\n");
  if (rv) {
    target[rv - 1] = '\0';
  }
  e->url = buf;
  if (*target) {
    e->target = target;
  }
}

bool fetchreg_find(
  struct fetchreg *r,
  const char *url,
  void *user_data,
  void (*cb)(const struct fetcherg_entry*, void*)) {
  akassert(r && url);

  char buf[PATH_MAX * 3 + 2];
  char sbuf[sizeof(buf)];
  size_t url_off = 0;
  size_t target_off = 0;
  size_t build_id_off = 0;
  bool found = false;
  struct fetcherg_entry e = { 0 };

  if (fseek(r->f, 0, SEEK_SET) == -1) {
    akerror(errno, "Failed to seek fetch registry for: %s", url);
    return false;
  }

  // Find the last matched fetched entry
  while (fgets(buf, sizeof(buf), r->f)) {
    _fetchreg_parse_line(buf, &e);
    if (!e.url || strcmp(e.url, url) != 0) {
      continue;
    }
    url_off = e.url - buf;
    target_off = e.target ? (size_t) (e.target - buf) : 0;
    build_id_off = e.build_id ? (size_t) (e.build_id - buf) : 0;
    memcpy(sbuf, buf, sizeof(sbuf));
    found = true;
  }

  if (!found) {
    return false;
  }

  struct fetcherg_entry se = {
    .url = sbuf + url_off,
    .target = target_off ? sbuf + target_off : 0,
    .build_id = build_id_off ? sbuf + build_id_off : 0,
  };
  if (cb) {
    cb(&se, user_data);
  }
  return true;
}

void fetchreg_foreach(
  struct fetchreg *r,
  void *user_data,
  void (*cb)(const struct fetcherg_entry*, void*)) {
  akassert(r && cb);

  char buf[PATH_MAX * 3 + 2];
  struct fetcherg_entry e = { 0 };

  if (fseek(r->f, 0, SEEK_SET) == -1) {
    akerror(errno, "Failed to seek fetch registry", 0);
    return;
  }

  while (fgets(buf, sizeof(buf), r->f)) {
    _fetchreg_parse_line(buf, &e);
    if (e.url) {
      cb(&e, user_data);
    }
  }
}

int fetchreg_register(struct fetchreg *r, const struct fetcherg_entry *entry) {
  if (!r || !entry || !entry->url) {
    return AK_ERROR_INVALID_ARGS;
  }
  long int old_pos = ftell(r->f);
  if (old_pos < 0) {
    return errno;
  }
  if (fseek(r->f, 0, SEEK_END) == -1) {
    return errno;
  }
  if (entry->target) {
    int rc;
    if (entry->build_id) {
      rc = fprintf(r->f, "%s\1%s\1%s\n", entry->url, entry->target, entry->build_id);
    } else {
      rc = fprintf(r->f, "%s\1%s\n", entry->url, entry->target);
    }
    if (rc < 0) {
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
