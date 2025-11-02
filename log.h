#ifndef LOG_H
#define LOG_H

#ifndef _AMALGAMATE_
#include "basedefs.h"
#include <stdarg.h>
#endif

enum akecode {
  AK_ERROR_OK                        = 0,
  AK_ERROR_FAIL                      = -1,
  AK_ERROR_UNIMPLEMETED              = -2,
  AK_ERROR_INVALID_ARGS              = -3,
  AK_ERROR_ASSERTION                 = -4,
  AK_ERROR_OVERFLOW                  = -5,
  AK_ERROR_IO                        = -6,
  AK_ERROR_SCRIPT_SYNTAX             = -10,
  AK_ERROR_SCRIPT                    = -11,
  AK_ERROR_CYCLIC_BUILD_DEPS         = -12,
  AK_ERROR_SCRIPT_ERROR              = -13,
  AK_ERROR_NOT_IMPLEMENTED           = -14,
  AK_ERROR_EXTERNAL_COMMAND          = -15,
  AK_ERROR_DEPENDENCY_UNRESOLVED     = -16,
  AK_ERROR_MACRO_MAX_RECURSIVE_CALLS = -17,
};

__attribute__((noreturn))
void akfatal2(const char *msg);

__attribute__((noreturn))
void _akfatal(const char *file, int line, int code, const char *fmt, ...);

__attribute__((noreturn))
void _akfatal_va(const char *file, int line, int code, const char *fmt, va_list);

int _akerror(const char *file, int line, int code, const char *fmt, ...);

void _akerror_va(const char *file, int line, int code, const char *fmt, va_list);

void _akverbose(const char *file, int line, const char *fmt, ...);

void akinfo(const char *fmt, ...);

void akwarn(const char *fmt, ...);

#define akfatal(code__, fmt__, ...) \
        _akfatal(__FILE__, __LINE__, (code__), (fmt__), __VA_ARGS__)

#define akfatal_va(code__, fmt__, va__) \
        _akfatal_va(__FILE__, __LINE__, (code__), (fmt__), (va__))

#define akerror(code__, fmt__, ...) \
        _akerror(__FILE__, __LINE__, (code__), (fmt__), __VA_ARGS__)

#define akerror_va(code__, fmt__, va__) \
        _akerror_va(__FILE__, __LINE__, (code__), (fmt__), (va__))

#define akverbose(fmt__, ...) \
        _akverbose(__FILE__, __LINE__, (fmt__), __VA_ARGS__)

#define akassert(exp__)                               \
        do {                                          \
          if (!(exp__)) {                             \
            akfatal(AK_ERROR_ASSERTION, Q(exp__), 0); \
          }                                           \
        } while (0)

#define akcheck(exp__)                    \
        do {                              \
          int e = (exp__);                \
          if (e) akfatal(e, Q(exp__), 0); \
        } while (0)


#endif
