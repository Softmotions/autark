#ifndef _AMALGAMATE_
#include "paths.h"
#include "xstr.h"
#include "alloc.h"
#include "utils.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <ftw.h>
#include <stdio.h>
#endif

#ifdef __APPLE__
#define st_atim st_atimespec
#define st_ctim st_ctimespec
#define st_mtim st_mtimespec
#endif

#define _TIMESPEC2MS(ts__) (((ts__).tv_sec * 1000ULL) + utils_lround((ts__).tv_nsec / 1.0e6))

bool path_is_absolute(const char *path) {
  if (!path) {
    return 0;
  }
  return *path == '/';
}

bool path_is_accesible(const char *path, enum akpath_access a) {
  if (!path || *path == '\0') {
    return 0;
  }
  int mode = 0;
  if (a & AKPATH_READ) {
    mode |= R_OK;
  }
  if (a & AKPATH_WRITE) {
    mode |= W_OK;
  }
  if (a & AKPATH_EXEC) {
    mode |= X_OK;
  }
  if (mode == 0) {
    mode = F_OK;
  }
  return access(path, mode) == 0;
}

bool path_is_accesible_read(const char *path) {
  return path_is_accesible(path, AKPATH_READ);
}

bool path_is_accesible_write(const char *path) {
  return path_is_accesible(path, AKPATH_WRITE);
}

bool path_is_accesible_exec(const char *path) {
  return path_is_accesible(path, AKPATH_EXEC);
}

const char* path_real(const char *path, char buf[PATH_MAX]) {
  return realpath(path, buf);
}

const char* path_real_pool(const char *path, struct pool *pool) {
  char buf[PATH_MAX];
  if (path_real(path, buf)) {
    return pool_strdup(pool, buf);
  } else {
    return 0;
  }
}

char* path_normalize(const char *path, char buf[PATH_MAX]) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, PATH_MAX)) {
    akfatal(errno, "Cannot get CWD", 0);
  }
  return path_normalize_cwd(path, cwd, buf);
}

char* path_normalize_cwd(const char *path, const char *cwd, char buf[PATH_MAX]) {
  akassert(cwd);
  if (path[0] == '/') {
    utils_strncpy(buf, path, PATH_MAX);
    char *p = strchr(buf, '.');
    if (  p == 0
       || !(*(p - 1) == '/' && (p[1] == '/' || p[1] == '.' || p[1] == '\0'))) {
      return buf;
    }
  } else {
    utils_strncpy(buf, cwd, PATH_MAX);
    size_t len = strlen(buf);
    if (len < PATH_MAX - 1) {
      buf[len] = '/';
      buf[len + 1] = '\0';
      strncat(buf, path, PATH_MAX - len - 2);
    } else {
      errno = ENAMETOOLONG;
      akfatal(errno, "Failed to normalize path, name is too long: %s", path);
    }
  }

  // Normalize: split and process each segment
  const char *rp = buf;
  char *segments[PATH_MAX];
  int top = 0;

  while (*rp) {
    while (*rp == '/') ++rp;
    if (!*rp) {
      break;
    }

    const char *sp = rp;
    while (*rp && *rp != '/') ++rp;

    size_t len = rp - sp;
    if (len == 0) {
      continue;
    }

    char segment[PATH_MAX];
    memcpy(segment, sp, len);
    segment[len] = '\0';

    if (strcmp(segment, ".") == 0) {
      continue;
    } else if (strcmp(segment, "..") == 0) {
      if (top > 0) {
        free(segments[--top]);
      }
    } else {
      segments[top++] = xstrdup(segment);
    }
  }

  if (top == 0) {
    buf[0] = '/';
    buf[1] = '\0';
  } else {
    buf[0] = '\0';
    for (int i = 0; i < top; ++i) {
      strcat(buf, "/");
      strcat(buf, segments[i]);
      free(segments[i]);
    }
  }
  return buf;
}

char* path_normalize_pool(const char *path, struct pool *pool) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, PATH_MAX)) {
    akfatal(errno, "Cannot get CWD", 0);
  }
  return path_normalize_cwd_pool(path, cwd, pool);
}

char* path_normalize_cwd_pool(const char *path, const char *cwd, struct pool *pool) {
  char buf[PATH_MAX];
  path_normalize_cwd(path, cwd, buf);
  return pool_strdup(pool, buf);
}

int path_mkdirs(const char *path) {
  if ((path[0] == '.' && path[1] == '\0') || path_is_exist(path)) {
    return 0;
  }
  int rc = 0;
  const size_t len = strlen(path);
  char buf[len + 1];
  char *rp = buf;

  memcpy(rp, path, len + 1);
  for (char *p = rp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(rp, S_IRWXU) != 0) {
        if (errno != EEXIST) {
          rc = errno;
          goto finish;
        }
      }
      *p = '/';
    }
  }
  if (mkdir(rp, S_IRWXU) != 0) {
    if (errno != EEXIST) {
      rc = errno;
      goto finish;
    }
  }
finish:
  return rc;
}

int path_mkdirs_for(const char *path) {
  char buf[PATH_MAX];
  utils_strncpy(buf, path, sizeof(buf));
  char *dir = path_dirname(buf);
  return path_mkdirs(dir);
}

static int _rmfile(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
  if (remove(pathname) < 0) {
    perror(pathname);
  }
  return 0;
}

int path_rmdir(const char *path) {
  if (nftw(path, _rmfile, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) < 0) {
    return errno != ENOENT ? errno : 0;
  }
  return 0;
}

static int _stat(const char *path, int fd, struct akpath_stat *fs) {
  struct stat st = { 0 };
  memset(fs, 0, sizeof(*fs));
  if (path) {
    if (stat(path, &st)) {
      if (errno == ENOENT) {
        fs->ftype = AKPATH_NOT_EXISTS;
        return 0;
      } else {
        return errno;
      }
    }
  } else {
    if (fstat(fd, &st)) {
      if (errno == ENOENT) {
        fs->ftype = AKPATH_NOT_EXISTS;
        return 0;
      } else {
        return errno;
      }
    }
  }
  fs->atime = _TIMESPEC2MS(st.st_atim);
  fs->mtime = _TIMESPEC2MS(st.st_mtim);
  fs->ctime = _TIMESPEC2MS(st.st_ctim);
  fs->size = (uint64_t) st.st_size;

  if (S_ISREG(st.st_mode)) {
    fs->ftype = AKPATH_TYPE_FILE;
  } else if (S_ISDIR(st.st_mode)) {
    fs->ftype = AKPATH_TYPE_DIR;
  } else if (S_ISLNK(st.st_mode)) {
    fs->ftype = AKPATH_LINK;
  } else {
    fs->ftype = AKPATH_OTHER;
  }
  return 0;
}

int path_stat(const char *path, struct akpath_stat *stat) {
  return _stat(path, -1, stat);
}

int path_stat_fd(int fd, struct akpath_stat *stat) {
  return _stat(0, fd, stat);
}

int path_stat_file(FILE *file, struct akpath_stat *stat) {
  return _stat(0, fileno(file), stat);
}

uint64_t path_mtime(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.mtime;
}

bool path_is_dir(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.ftype == AKPATH_TYPE_DIR;
}

bool path_is_file(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.ftype == AKPATH_TYPE_FILE;
}

bool path_is_exist(const char *path) {
  struct akpath_stat st;
  if (path_stat(path, &st)) {
    return 0;
  }
  return st.ftype != AKPATH_NOT_EXISTS;
}

static inline int _path_num_segments(const char *path) {
  int c = 0;
  for (const char *rp = path; *rp != '\0'; ++rp) {
    if (*rp == '/' || *rp == '\0') {
      ++c;
    }
  }
  return c;
}

char* path_relativize(const char *from, const char *to) {
  char cwd[PATH_MAX];
  if (!getcwd(cwd, PATH_MAX)) {
    akfatal(errno, "Cannot get CWD", 0);
  }
  return path_relativize_cwd(from, to, cwd);
}

char* path_relativize_cwd(const char *from_, const char *to_, const char *cwd) {
  char from[PATH_MAX], to[PATH_MAX];
  if (from_ == 0) {
    from_ = cwd;
  }
  path_normalize_cwd(from_, cwd, from);
  path_normalize_cwd(to_, cwd, to);
  akassert(*from == '/' && *to == '/');

  int sf, sc;
  struct xstr *xstr = xstr_create_empty();
  const char *frp = from, *trp = to, *srp = to;

  for (++frp, ++trp, sc = 0, sf = _path_num_segments(frp - 1); *frp == *trp; ++frp, ++trp) {
    if (*frp == '/' || *frp == '\0') {
      ++sc;
      srp = trp;
      if (*frp == '\0') {
        break;
      }
    }
  }
  if ((*frp == '\0' && *trp == '/') || (*frp == '/' && *trp == '\0')) {
    ++sc;
    srp = trp;
  }

  for (int i = 0, l = sf - sc; i < l; ++i) {
    xstr_cat2(xstr, "../", AK_LLEN("../"));
  }
  if (*srp != '\0') {
    xstr_cat(xstr, srp + 1);
  }
  return xstr_destroy_keep_ptr(xstr);
}

char* path_dirname(char *path) {
  return dirname(path);
}

char* path_basename(char *path) {
  size_t i;
  if (!path || *path == '\0') {
    return ".";
  }
  i = strlen(path) - 1;
  for ( ; i && path[i] == '/'; i--) path[i] = 0;
  for ( ; i && path[i - 1] != '/'; i--) ;
  return path + i;
}

const char* path_is_prefix_for(const char *prefix, const char *path, const char *cwd) {
  akassert(path_is_absolute(prefix));
  char buf[PATH_MAX];
  if (!path_is_absolute(path)) {
    char cwdbuf[PATH_MAX];
    if (!cwd) {
      cwd = getcwd(cwdbuf, sizeof(cwdbuf));
    }
    path = path_normalize_cwd(path, cwd, buf);
  }

  int len = strlen(prefix);
  if (len < 1 || !utils_startswith(path, prefix)) {
    return 0;
  }
  const char *p = path + len;
  if (*p == '/') {
    while (*p == '/') ++p;
    return p;
  } else if (prefix[len - 1] == '/') {
    return p;
  } else {
    return 0;
  }
}
