#ifndef _AMALGAMATE_
#include "env.h"
#include "script.h"
#include "utils.h"
#include "log.h"
#include "pool.h"
#include "xstr.h"
#include "alloc.h"
#include "nodes.h"
#include "autark.h"
#include "paths.h"

#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#endif

#define XNODE(n__)              ((struct xnode*) (n__))
#define NODE_AT(list__, idx__)  *(struct node**) ulist_get(list__, idx__)
#define NODE_PEEK(list__)       ((list__)->num ? *(struct node**) ulist_peek(list__) : 0)
#define XNODE_AT(list__, idx__) XNODE(NODE_AT(list__, idx__))
#define XNODE_PEEK(list__)      ((list__)->num ? XNODE(*(struct node**) ulist_peek(list__)) : 0)
#define XCTX(n__)               (n__)->base.ctx
#define NODE(x__)               (struct node*) (x__)

struct _yycontext;

struct xparse {
  struct _yycontext *yy;
  struct ulist       stack;   // ulist<struct xnode*>
  struct value       val;
  struct xstr       *xerr;
  int pos;
};

struct xnode {
  struct node    base;
  struct xparse *xp;        // Used only by root script node
  unsigned       bld_calls; // Build calls counter in order to detect cyclic build deps
  unsigned       lnum;
};

//#define YY_DEBUG
#define YY_CTX_LOCAL 1
#define YY_CTX_MEMBERS \
        struct xnode *x;

#define YYSTYPE struct xnode*
#define YY_MALLOC(yy_, sz_)        _x_malloc(yy_, sz_)
#define YY_REALLOC(yy_, ptr_, sz_) _x_realloc(yy_, ptr_, sz_)


#define YY_INPUT(yy_, buf_, result_, max_size_) \
        result_ = _input(yy_, buf_, max_size_);

static void* _x_malloc(struct _yycontext *yy, size_t size) {
  return xmalloc(size);
}

static void* _x_realloc(struct _yycontext *yy, void *ptr, size_t size) {
  return xrealloc(ptr, size);
}

static int _input(struct _yycontext *yy, char *buf, int max_size);
static struct xnode* _push_and_register(struct _yycontext *yy, struct xnode *x);
static struct xnode* _node_text(struct  _yycontext *yy, const char *text);
static struct xnode* _node_text_push(struct  _yycontext *yy, const char *text);
static struct xnode* _node_text_escaped_push(struct  _yycontext *yy, const char *text);
static struct xnode* _rule(struct _yycontext *yy, struct xnode *x);
static void _finish(struct _yycontext *yy);

#ifndef _AMALGAMATE_
#include "scriptx.h"
#endif


