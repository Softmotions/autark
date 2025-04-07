#include "utils.h"
#include "xstr.h"
#include "log.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

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


