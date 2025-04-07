#ifndef BASEDEFS_H
#define BASEDEFS_H

#define QSTR(x__) #x__
#define Q(x__)    QSTR(x__)

#if __GNUC__ >= 4
#define AUR      __attribute__((warn_unused_result))
#define AK_ALLOC __attribute__((malloc)) __attribute__((warn_unused_result))
#define AK_NORET __attribute__((noreturn))
#else
#define AUR
#define AK_ALLOC
#define AK_NORET
#endif

#define AK_CONSTRUCTOR __attribute__((constructor))
#define AK_DESTRUCTOR  __attribute__((destructor))

#define LLEN(l__) (sizeof(l__) - 1)

/* Align x_ with v_. v_ must be simple power for 2 value. */
#define ROUNDUP(x_, v_) (((x_) + (v_) - 1) & ~((v_) - 1))

/* Round down align x_ with v_. v_ must be simple power for 2 value. */
#define ROUNDOWN(x_, v_) ((x_) - ((x_) & ((v_) - 1)))

#ifdef __GNUC__
#define RCGO(rc__, label__) if (__builtin_expect((!!(rc__)), 0)) goto label__
#else
#define RCGO(rc__, label__) if (rc__) goto label__
#endif

#define RCHECK(rc__, label__, expr__) \
        rc__ = expr__;                \
        RCGO(rc__, label__)

#define RCC(rc__, label__, expr__) RCHECK(rc__, label__, expr__)

#define RCN(label__, expr__) \
        rc = (expr__);       \
        if (rc) {            \
          akerror(rc, 0, 0); \
          goto label__;      \
        }


#include <stdlib.h>
#include <stddef.h>

struct value {
  void  *buf;
  size_t len;
  int    error;
};

static inline int value_destroy(struct value *v) {
  if (v->buf) {
    free(v->buf);
    v->buf = 0;
  }
  return v->error;
}

#endif
