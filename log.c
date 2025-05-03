#include "basedefs.h"
#include "log.h"
#include "xstr.h"
#include "alloc.h"

#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define LEVEL_VERBOSE 0
#define LEVEL_INFO    1
#define LEVEL_ERROR   2
#define LEVEL_FATAL   3

static const char* _error_get(int code) {
  switch (code) {
    case AK_ERROR_OVERFLOW:
      return "Value/argument overflow. (AK_ERROR_OVERFLOW)";
    case AK_ERROR_ASSERTION:
      return "Assertion failed (AK_ERROR_ASSERTION)";
    case AK_ERROR_INVALID_ARGS:
      return "Invalid function arguments (AKL_ERROR_INVALID_ARGS)";
    case AK_ERROR_FAIL:
      return "Fail (AK_ERROR_FAIL)";
    case AK_ERROR_IO:
      return "IO Error (AK_ERROR_IO)";
    case AK_ERROR_UNIMPLEMETED:
      return "Not implemented (AK_ERROR_UNIMPLEMETED)";
    case AK_ERROR_SCRIPT_SYNTAX:
      return "Invalid autark config syntax (AK_ERROR_SCRIPT_SYNTAX)";
    case AK_ERROR_CYCLIC_BUILD_DEPS:
      return "Detected cyclic build dependency (AK_ERROR_CYCLIC_BUILD_DEPS)";
    case AK_ERROR_SCRIPT_ERROR:
      return "Build script error (AK_ERROR_SCRIPT_ERROR)";
    case AK_ERROR_NOT_IMPLEMENTED:
      return "Not implemented (AK_ERROR_NOT_IMPLEMENTED)";
    case AK_ERROR_EXTERNAL_COMMAND:
      return "External command failed (AK_ERROR_EXTERNAL_COMMAND)";
    case AK_ERROR_DEPENDENCY_UNRESOLVED:
      return "I don't know how to build dependency (AK_ERROR_DEPENDENCY_UNRESOLVED)";
    case AK_ERROR_OK:
      return "OK";
    default:
      return strerror(code);
  }
}

static char* _basename(char *path) {
  size_t i;
  if (!path || *path == '\0') {
    return ".";
  }
  i = strlen(path) - 1;
  for ( ; i && path[i] == '/'; i--) path[i] = 0;
  for ( ; i && path[i - 1] != '/'; i--) ;
  return path + i;
}

static void _event_va(
  int         level,
  const char *file,
  int         line,
  int         code,
  const char *format,
  va_list     va) {
  struct xstr *xstr = 0;
  xstr = xstr_create_empty();
  switch (level) {
    case LEVEL_FATAL:
      xstr_cat2(xstr, "FATAL   ", LLEN("FATAL   "));
      break;
    case LEVEL_INFO:
      xstr_cat2(xstr, "INFO    ", LLEN("INFO    "));
      break;
    case LEVEL_ERROR:
      xstr_cat2(xstr, "ERROR    ", LLEN("ERROR    "));
      break;
    case LEVEL_VERBOSE:
      xstr_cat2(xstr, "VERBOSE ", LLEN("VERBOSE "));
      break;
    default:
      xstr_cat2(xstr, "XXX     ", LLEN("XXX     "));
      break;
  }
  if (file) {
    char *cfile = xstrdup(file);
    if (cfile) {
      cfile = _basename(cfile);
      xstr_printf(xstr, "%s:%d ", cfile, line);
      free(cfile);
    }
  }
  if (code) {
    const char *err = _error_get(code);
    xstr_printf(xstr, "%d|", code);
    if (err) {
      xstr_cat(xstr, err);
    }
  }
  if (format) {
    if (code) {
      xstr_cat2(xstr, "|", 1);
    }
    xstr_printf_va(xstr, format, va);
  }
  xstr_cat2(xstr, "\n", 1);
  fputs(xstr_ptr(xstr), stderr);
  xstr_destroy(xstr);
}

void akfatal2(const char *msg) {
  if (msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
  }
  _exit(254);
}

void _akfatal(const char *file, int line, int code, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_FATAL, file, line, code, fmt, ap);
  va_end(ap);
  akfatal2(0);
}

int _akerror(const char *file, int line, int code, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_ERROR, file, line, code, fmt, ap);
  va_end(ap);
  return code;
}

void _akverbose(const char *file, int line, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_VERBOSE, file, line, 0, fmt, ap);
  va_end(ap);
}

void akinfo(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  _event_va(LEVEL_INFO, 0, 0, 0, fmt, ap);
  va_end(ap);
}

void _akerror_va(const char *file, int line, int code, const char *fmt, va_list ap) {
  _event_va(LEVEL_ERROR, file, line, code, fmt, ap);
}

void _akfatal_va(const char *file, int line, int code, const char *fmt, va_list ap) {
  _event_va(LEVEL_FATAL, file, line, code, fmt, ap);
  akfatal2(0);
}
