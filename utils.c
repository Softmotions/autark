#ifndef _AMALGAMATE_
#include "utils.h"
#include "xstr.h"
#include "log.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#endif

struct value utils_file_as_buf(const char *path, ssize_t buflen_max) {
  struct value ret = { 0 };
  struct xstr *xstr = xstr_create_empty();

  char buf[8192];
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    ret.error = errno;
    xstr_destroy(xstr);
    return ret;
  }
  while (buflen_max != 0 && ret.error == 0) {
    ssize_t rb = read(fd, buf, sizeof(buf));
    if (rb > 0) {
      if (buflen_max > -1) {
        if (rb > buflen_max) {
          rb = buflen_max;
          ret.error = AK_ERROR_OVERFLOW;
        }
        buflen_max -= rb;
      }
      xstr_cat2(xstr, buf, rb);
    } else if (rb == -1) {
      if (errno != EINTR && errno != EAGAIN) {
        ret.error = errno;
        break;
      }
    } else {
      break;
    }
  }
  ret.len = xstr_size(xstr);
  ret.buf = xstr_destroy_keep_ptr(xstr);
  return ret;
}

int utils_file_write_buf(const char *path, const char *buf, size_t len, bool append) {
  int flags = O_WRONLY | O_CREAT;
  if (append) {
    flags |= O_APPEND;
  } else {
    flags |= O_TRUNC;
  }
  int fd = open(path, flags);
  if (fd == -1) {
    return errno;
  }
  for (ssize_t w, tow = len; tow > 0; ) {
    w = write(fd, buf + len - tow, tow);
    if (w >= 0) {
      tow -= w;
    } else if (w < 0) {
      if (errno == EAGAIN) {
        continue;
      }
      int ret = errno;
      close(fd);
      return ret;
    }
  }
  close(fd);
  return 0;
}

int utils_copy_file(const char *src, const char *dst) {
  int rc = 0;
  char buf[8192];
  FILE *sf = fopen(src, "rb");
  if (!sf) {
    return errno;
  }
  FILE *df = fopen(dst, "wb");
  if (!df) {
    rc = errno;
    fclose(sf);
    return rc;
  }
  size_t nr = 0;
  while (1) {
    nr = fread(buf, 1, sizeof(buf), sf);
    if (nr) {
      nr = fwrite(buf, 1, nr, df);
      if (!nr) {
        rc = AK_ERROR_IO;
        break;
      }
    } else if (feof(sf)) {
      break;
    } else if (ferror(sf)) {
      rc = AK_ERROR_IO;
      break;
    }
  }
  fclose(sf);
  fclose(df);
  return rc;
}

int utils_rename_file(const char *src, const char *dst) {
  if (rename(src, dst) == -1) {
    if (errno == EXDEV) {
      int rc = utils_copy_file(src, dst);
      if (!rc) {
        unlink(src);
      }
      return rc;
    } else {
      return errno;
    }
  }
  return 0;
}

long int utils_strtol(const char *v, int base, int *rcp) {
  *rcp = 0;
  char *ep = 0;
  long int ret = strtol(v, &ep, base);
  if (*ep != '\0' || errno == ERANGE) {
    *rcp = AK_ERROR_INVALID_ARGS;
    return 0;
  }
  return ret;
}

long long utils_strtoll(const char *v, int base, int *rcp) {
  *rcp = 0;
  char *ep = 0;
  long long ret = strtoll(v, &ep, base);
  if ((*ep != '\0' && *ep != '\n') || errno == ERANGE) {
    *rcp = AK_ERROR_INVALID_ARGS;
    return 0;
  }
  return ret;
}

void utils_split_values_add(const char *v, struct xstr *xstr) {
  if (is_vlist(v)) {
    xstr_cat(xstr, v);
    return;
  }
  char buf[strlen(v) + 1];
  const char *p = v;

  while (*p) {
    while (utils_char_is_space(*p)) ++p;
    if (*p == '\0') {
      break;
    }
    char *w = buf;
    char q = 0;

    while (*p && (q || !utils_char_is_space(*p))) {
      if (*p == '\\') {
        ++p;
        if (*p) {
          *w++ = *p++;
        }
      } else if (q) {
        if (*p == q) {
          q = 0;
          ++p;
        } else {
          *w++ = *p++;
        }
      } else if (*p == '\'' || *p == '"') {
        q = *p++;
      } else {
        *w++ = *p++;
      }
    }
    *w = '\0';
    xstr_cat(xstr, "\1");
    xstr_cat(xstr, buf);
  }
}

int utils_fd_make_non_blocking(int fd) {
  int rci, flags;
  while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR) ;
  if (flags == -1) {
    return errno;
  }
  while ((rci = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR) ;
  if (rci == -1) {
    return errno;
  }
  return 0;
}
