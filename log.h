#ifndef LOG_H
#define LOG_H

enum akecode {
  AK_ERROR_OK                = 0,
  AK_ERROR_FAIL              = -1,
  AK_ERROR_UNIMPLEMETED      = -2,
  AK_ERROR_INVALID_ARGS      = -3,
  AK_ERROR_ASSERTION         = -4,
  AK_ERROR_OVERFLOW          = -5,
  AK_ERROR_IO                = -6,
  AK_ERROR_SCRIPT_SYNTAX     = -10,
  AK_ERROR_CYCLIC_BUILD_DEPS = -11,
};

__attribute__((noreturn))
void akfatal2(const char *msg);

__attribute__((noreturn))
void _akfatal(const char *file, int line, int code, const char *fmt, ...);

void _akerror(const char *file, int line, int code, const char *fmt, ...);

void _akverbose(const char *file, int line, const char *fmt, ...);

void akinfo(const char *fmt, ...);

#define akfatal(code__, fmt__, ...) \
        _akfatal(__FILE__, __LINE__, (code__), (fmt__), __VA_ARGS__)

#define akerror(code__, fmt__, ...) \
        _akerror(__FILE__, __LINE__, (code__), (fmt__), __VA_ARGS__)

#define akverbose(fmt__, ...) \
        _akverbose(__FILE__, __LINE__, (fmt__), __VA_ARGS__)

#endif
