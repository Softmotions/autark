#include "utils.h"
#include "xstr.h"
#include "log.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

int utils_exec_path(char buf[PATH_MAX]) {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
  const int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
  if (sysctl(mib, 4, opath, &opath_maxlen, 0, 0) < 0) {
    return errno;
  }
  return 0;
 #elif defined(__linux__)
  char *path = "/proc/self/exe";
  ssize_t ret = readlink(path, buf, PATH_MAX);
  if (ret == -1) {
    return errno;
  } else if (ret < PATH_MAX) {
    buf[ret] = '\0';
  } else {
    buf[PATH_MAX - 1] = '\0';
  }
  return 0;
#elif defined(__APPLE__)
  pid_t pid = getpid();
  int ret = proc_pidpath(pid, opath, opath_maxlen);
  if (ret < 0) {
    return iwrc_set_errno(IW_ERROR_ERRNO, errno);
  }
  return 0;
#else
  return AK_ERROR_FAIL;
#endif
}

struct value utils_file_as_buf(const char *path, ssize_t buflen_max) {
  struct value ret = { 0 };
  struct xstr *xstr = xstr_create_empty();

  char buf[8192];
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    ret.error = errno;
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
  if (*ep != '\0' || errno == ERANGE) {
    *rcp = AK_ERROR_INVALID_ARGS;
    return 0;
  }
  return ret;
}
