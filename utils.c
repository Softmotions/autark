#include "utils.h"
#include "xstr.h"
#include "log.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


struct value utils_file_as_buf(const char *path, ssize_t buflen_max) {
  struct value ret = { 0 };
  struct xstr *xstr = xstr_create_empty();

  char buf[8192];
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    ret.error = errno;
    return ret;
  }
  while(buflen_max != 0 && ret.error != 0) {
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
  ret.buf = xstr_destroy_keep_ptr(xstr);
  return ret;
}
