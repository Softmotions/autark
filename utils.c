#ifndef _AMALGAMATE_
#include "utils.h"
#include "paths.h"
#include "xstr.h"
#include "log.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
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
  int fd = open(path, flags, 0644);
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

int utils_copy_file_streams(FILE *sf, FILE *df) {
  char buf[8192];
  size_t nr = 0;
  while (1) {
    nr = fread(buf, 1, sizeof(buf), sf);
    if (nr) {
      size_t offset = 0;
      while (offset < nr) {
        size_t nw = fwrite(buf + offset, 1, nr - offset, df);
        if (!nw) {
          return AK_ERROR_IO;
        }
        offset += nw;
      }
    } else if (feof(sf)) {
      break;
    } else if (ferror(sf)) {
      return AK_ERROR_IO;
    }
  }
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
      size_t offset = 0;
      while (offset < nr) {
        size_t nw = fwrite(buf + offset, 1, nr - offset, df);
        if (!nw) {
          rc = AK_ERROR_IO;
          goto finish;
        }
        offset += nw;
      }
    } else if (feof(sf)) {
      break;
    } else if (ferror(sf)) {
      rc = AK_ERROR_IO;
      break;
    }
  }
finish:
  fclose(sf);
  if (fclose(df)) {
    if (!rc) {
      rc = AK_ERROR_IO;
    }
  }
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

static inline int _utils_same_file(const struct stat *a, const struct stat *b) {
  return a->st_dev == b->st_dev
         && a->st_ino == b->st_ino;
}

static int _utils_path_is_same_or_child(const char *parent, const char *path) {
  size_t len = strlen(parent);
  if (strncmp(parent, path, len) != 0) {
    return 0;
  }
  if (path[len] == '\0') {
    return 1;
  }
  if (len == 1 && parent[0] == '/') {
    return path[0] == '/';
  }
  return path[len] == '/';
}

static char* _utils_real_existing_ancestor(const char *path) {
  char *current = strdup(path);
  if (!current) {
    errno = ENOMEM;
    return 0;
  }
  for ( ; ; ) {
    char *real = realpath(current, 0);
    if (real) {
      free(current);
      return real;
    }
    if (errno != ENOENT && errno != ENOTDIR) {
      int rc = errno;
      free(current);
      errno = rc;
      return 0;
    }
    size_t len = strlen(current);
    while (len > 1 && current[len - 1] == '/') {
      current[--len] = '\0';
    }
    char *slash = strrchr(current, '/');
    if (!slash) {
      free(current);
      current = strdup(".");
      if (!current) {
        errno = ENOMEM;
        return 0;
      }
    } else if (slash == current) {
      current[1] = '\0';
    } else {
      *slash = '\0';
    }
  }
}

static int _utils_check_overlap(const char *src, const char *dst) {
  struct stat st;
  if (lstat(src, &st) != 0) {
    return errno;
  }
  if (S_ISLNK(st.st_mode)) {
    return ELOOP;
  }
  if (!S_ISDIR(st.st_mode)) {
    return ENOTDIR;
  }
  char *src_real = realpath(src, 0);
  if (!src_real) {
    return errno;
  }
  char *dst_ancestor = _utils_real_existing_ancestor(dst);
  if (!dst_ancestor) {
    int rc = errno;
    free(src_real);
    return rc;
  }
  int rc = _utils_path_is_same_or_child(src_real, dst_ancestor) ? EINVAL : 0;
  if (!rc) {
    if (lstat(dst, &st) == 0) {
      if (S_ISLNK(st.st_mode)) {
        rc = ELOOP;
      } else if (!S_ISDIR(st.st_mode)) {
        rc = ENOTDIR;
      } else {
        char *dst_real = realpath(dst, 0);

        if (!dst_real) {
          rc = errno;
        } else {
          if (_utils_path_is_same_or_child(
                dst_real, src_real)) {
            rc = EINVAL;
          }
          free(dst_real);
        }
      }
    } else if (errno != ENOENT) {
      rc = errno;
    }
  }
  free(dst_ancestor);
  free(src_real);
  return rc;
}

static int _utils_copy_symlink(const char *src, const char *dst, const struct stat *src_st) {
  ssize_t len;
  char *target = 0;
  size_t capacity = src_st->st_size > 0 ? (size_t) src_st->st_size + 1 : 256;

  for ( ; ; ) {
    target = malloc(capacity + 1);
    if (!target) {
      return ENOMEM;
    }
    len = readlink(src, target, capacity);
    if (len < 0) {
      int rc = errno;
      free(target);
      return rc;
    }
    if ((size_t) len < capacity) {
      break;
    }
    free(target);
    if (capacity > SIZE_MAX / 2) {
      return EOVERFLOW;
    }
    capacity *= 2;
  }
  target[len] = '\0';

  struct stat dst_st;
  if (lstat(dst, &dst_st) == 0) {
    if (S_ISDIR(dst_st.st_mode)) {
      free(target);
      return EISDIR;
    }
    if (unlink(dst) != 0) {
      int rc = errno;
      free(target);
      return rc;
    }
  } else if (errno != ENOENT) {
    int rc = errno;
    free(target);
    return rc;
  }
  int rc = symlink(target, dst) == 0 ? 0 : errno;
  free(target);
  return rc;
}

static int _utils_copy_regular(
  const char        *src,
  const char        *dst,
  const struct stat *src_st) {
  struct stat dst_st;

  if (lstat(dst, &dst_st) == 0) {
    if (S_ISDIR(dst_st.st_mode)) {
      return EISDIR;
    }
    if (S_ISLNK(dst_st.st_mode)) {
      if (unlink(dst) != 0) {
        return errno;
      }
    } else if (!S_ISREG(dst_st.st_mode)) {
      return ENOTSUP;
    } else if (_utils_same_file(src_st, &dst_st)) {
      return EINVAL;
    }
  } else if (errno != ENOENT) {
    return errno;
  }
  int rc = utils_copy_file(src, dst);
  if (  !rc
     && chmod(dst, src_st->st_mode & 07777) != 0) {
    rc = errno;
  }
  return rc;
}

static int _utils_copy_dir_recursive(const char *src, const char *dst) {
  struct stat src_st;
  struct stat dst_st;
  if (lstat(src, &src_st) != 0) {
    return errno;
  }
  if (S_ISLNK(src_st.st_mode)) {
    return ELOOP;
  }
  if (!S_ISDIR(src_st.st_mode)) {
    return ENOTDIR;
  }

  int created = 0;
  if (lstat(dst, &dst_st) == 0) {
    if (S_ISLNK(dst_st.st_mode)) {
      return ELOOP;
    }
    if (!S_ISDIR(dst_st.st_mode)) {
      return ENOTDIR;
    }
    if (_utils_same_file(&src_st, &dst_st)) {
      return EINVAL;
    }
  } else if (errno == ENOENT) {
    if (mkdir(dst, 0700) != 0) {
      return errno;
    }
    created = 1;
  } else {
    return errno;
  }

  DIR *dir = opendir(src);
  if (!dir) {
    return errno;
  }

  int rc = 0;
  for ( ; ; ) {
    errno = 0;
    struct dirent *entry = readdir(dir);
    if (!entry) {
      if (errno) {
        rc = errno;
      }
      break;
    }
    if (  !strcmp(entry->d_name, ".")
       || !strcmp(entry->d_name, "..")) {
      continue;
    }
    struct stat entry_st;
    char *src_path = path_join_path_alloc(src, entry->d_name, 0);
    char *dst_path = path_join_path_alloc(dst, entry->d_name, 0);
    if (lstat(src_path, &entry_st) != 0) {
      rc = errno;
    } else if (S_ISDIR(entry_st.st_mode)) {
      rc = _utils_copy_dir_recursive(src_path, dst_path);
    } else if (S_ISREG(entry_st.st_mode)) {
      rc = _utils_copy_regular(src_path, dst_path, &entry_st);
    } else if (S_ISLNK(entry_st.st_mode)) {
      rc = _utils_copy_symlink(src_path, dst_path, &entry_st);
    } else {
      // FIFO, socket, block/character device.
      rc = ENOTSUP;
    }

    free(src_path);
    free(dst_path);
    if (rc) {
      break;
    }
  }
  if (closedir(dir) != 0 && !rc) {
    rc = errno;
  }
  if (  created
     && chmod(dst, src_st.st_mode & 07777) != 0
     && !rc) {
    rc = errno;
  }
  return rc;
}

int utils_copy_dir(const char *src, const char *dst) {
  if (!src || !*src || !dst || !*dst) {
    return AK_ERROR_INVALID_ARGS;
  }
  int rc = _utils_check_overlap(src, dst);
  return rc ? rc : _utils_copy_dir_recursive(src, dst);
}

int utils_copy_dir_to_parent(const char *src, const char *dst) {
  if (!src || !*src || !dst || !*dst) {
    return AK_ERROR_INVALID_ARGS;
  }
  struct pool *pool = pool_create_empty();
  char *bname = path_basename(pool_strdup(pool, src));
  dst = path_join_path_pool(pool, dst, bname, 0);
  int rv = utils_copy_dir(src, dst);
  pool_destroy(pool);
  return rv;
}

long int utils_strtol(const char *v, int base, int *rcp) {
  *rcp = 0;
  char *ep = 0;
  errno = 0;
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
  errno = 0;
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

int64_t utils_current_time_ms(void) {
  struct timespec ts;
#if defined(CLOCK_REALTIME)
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
    akfatal(errno, "", 0);
  }
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return (int64_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
  return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

const char* utils_json_escape_str(const char *val, ssize_t len, struct xstr *xstr) {
  if (!val || !xstr) {
    return 0;
  }
  if (len < 0) {
    len = strlen(val);
  }
  static const char *specials = "btnvfr";
  xstr_cat2(xstr, "\"", 1);
  for (size_t i = 0; i < len; ++i) {
    uint8_t ch = (uint8_t) val[i];
    if (ch == '"' || ch == '\'') {
      xstr_cat2(xstr, "\\", 1);
      xstr_cat2(xstr, &ch, 1);
    } else if (ch >= '\b' && ch <= '\r') {
      xstr_cat2(xstr, "\\", 1);
      xstr_cat2(xstr, &specials[ch - '\b'], 1);
    } else {
      xstr_cat2(xstr, &ch, 1);
    }
  }
  xstr_cat2(xstr, "\"", 1);
  return xstr_ptr(xstr);
}
