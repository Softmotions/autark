#include "project.h"
#include "utils.h"
#include "log.h"
#include "pool.h"
#include "xstr.h"
#include "alloc.h"
#include "config.h"

#define XNODE(n__)              ((struct xnode*) (n__))
#define XNODE_AT(list__, idx__) XNODE(*(struct node**) ulist_get(list__, idx__))
#define XNODE_PEEK(list__)      XNODE(*(struct node**) ulist_peek(list__))
#define XPROJECT(n__)           (n__)->base.project
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
  struct xparse *xp; // Used only by root script node
};

#define YY_DEBUG
#define YY_CTX_LOCAL 1
#define YY_CTX_MEMBERS \
        struct xnode *x;


#define YYSTYPE struct xnode*
#define YY_MALLOC(yy_, sz_)        _x_malloc(yy_, sz_)
#define YY_REALLOC(yy_, ptr_, sz_) _x_realloc(yy_, ptr_, sz_)
#define YY_INPUT(yy_, buf_, result_, max_size_)                 \
        {                                                       \
          struct xnode *x = (yy_)->x;                           \
          if (x->xp->pos >= x->xp->val.len) {                   \
            result_ = 0;                                        \
          } else {                                              \
            char ch = *((char*) x->xp->val.buf + x->xp->pos++); \
            result_ = 1;                                        \
            *(buf_) = ch;                                       \
          }                                                     \
        }

static void* _x_malloc(struct _yycontext *yy, size_t size) {
  return xmalloc(size);
}

static void* _x_realloc(struct _yycontext *yy, void *ptr, size_t size) {
  return xrealloc(ptr, size);
}

struct _yycontext* yyrelease(struct _yycontext *yyctx);

static void _xparse_destroy(struct xparse *xp) {
  if (xp) {
    xstr_destroy(xp->xerr);
    ulist_destroy_keep(&xp->stack);
    if (xp->yy) {
      yyrelease(xp->yy);
      free(xp->yy);
      xp->yy = 0;
    }
    free(xp);
  }
}

static void _xnode_destroy(struct xnode *x) {
  _xparse_destroy(x->xp);
  x->xp = 0;
}

static struct xnode* _push(struct _yycontext *yy, struct xnode *n);
static struct xnode* _node_text(struct  _yycontext *yy, const char *text);
static struct xnode* _rule(struct _yycontext *yy, struct xnode *n);
static void _finish(struct _yycontext *yy);

#include "scriptx.h"

static void _yyerror(yycontext *yy) {
  struct xnode *x = yy->x;
  struct xstr *xerr = x->xp->xerr;
  if (yy->__pos && yy->__text[0]) {
    xstr_cat(xerr, "near token: '");
    xstr_cat(xerr, yy->__text);
    xstr_cat(xerr, "'\n");
  }
  if (yy->__pos < yy->__limit) {
    char buf[2] = { 0 };
    yy->__buf[yy->__limit] = '\0';
    xstr_cat(xerr, "\n");
    while (yy->__pos < yy->__limit) {
      buf[0] = yy->__buf[yy->__pos++];
      xstr_cat2(xerr, buf, 1);
    }
  }
  xstr_cat(xerr, " <--- \n");
}

static unsigned _rule_type(const char *key) {
  if (strcmp(key, "meta") == 0) {
    return NODE_TYPE_META;
  } else if (strcmp(key, "consumes") == 0) {
    return NODE_TYPE_CONSUMES;
  } else if (strcmp(key, "check") == 0) {
    return NODE_TYPE_CHECK;
  } else if (strcmp(key, "sources") == 0) {
    return NODE_TYPE_SOURCES;
  } else if (strcmp(key, "flags") == 0) {
    return NODE_TYPE_FLAGS;
  } else if (strcmp(key, "exec") == 0) {
    return NODE_TYPE_EXEC;
  } else if (strcmp(key, "static") == 0) {
    return NODE_TYPE_STATIC;
  } else if (strcmp(key, "shared") == 0) {
    return NODE_TYPE_SHARED;
  } else if (strcmp(key, "include") == 0) {
    return NODE_TYPE_INCLUDE;
  } else if (strcmp(key, "if") == 0) {
    return NODE_TYPE_IF;
  } else {
    return NODE_TYPE_BAG;
  }
}

static struct xnode* _push(struct _yycontext *yy, struct xnode *n) {
  struct xnode *x = yy->x;
  ulist_push(&x->xp->stack, &n);
  return n;
}

static struct xnode* _node_text(struct  _yycontext *yy, const char *text) {
  struct project *p = XPROJECT(yy->x);
  struct xnode *x = pool_calloc(p->pool, sizeof(*x));
  x->base.project = p;
  x->base.value = pool_strdup(p->pool, text);
  x->base.type = NODE_TYPE_VALUE;
  return x;
}

static struct xnode* _rule(struct _yycontext *yy, struct xnode *key) {
  struct xparse *xp = yy->x->xp;
  struct ulist *s = &xp->stack;
  key->base.type = _rule_type(key->base.value);
  while (s->num) {
    struct xnode *n = XNODE_PEEK(s);
    if (n != key) {
      n->base.next = key->base.child;
      n->base.parent = &key->base;
      key->base.child = &n->base;
      ulist_pop(s);
    } else {
      break; // Keep rule on stack
    }
  }
  return key;
}

static void _finish(struct _yycontext *yy) {
  struct xnode *root = yy->x;
  struct xparse *xp = root->xp;
  struct ulist *s = &xp->stack;
  while (s->num) {
    struct xnode *n = XNODE_PEEK(s);
    if (n->base.type != NODE_TYPE_VALUE) {
      n->base.next = root->base.child;
      root->base.child = &n->base;
    }
    ulist_pop(s);
  }
}

static int _script_from_buf(
  struct node *parent, const struct value *buf,
  struct node **out) {
  int rc = 0;

  struct xnode *script = 0;
  if (!parent) {
    struct pool *pool = pool_create_empty();
    struct project *project = pool_calloc(pool, sizeof(*project));
    project->pool = pool;
    ulist_init(&project->nodes, 64, sizeof(struct node*));

    script = pool_calloc(pool, sizeof(*script));
    script->base.project = project;
    project->root = (struct node*) script;
  } else {
    script = pool_calloc(parent->project->pool, sizeof(*script));
    script->base.parent = parent;
  }
  script->base.type = NODE_TYPE_SCRIPT;
  script->base.index = script->base.project->nodes.num;
  ulist_push(&script->base.project->nodes, &script);

  script->xp = malloc(sizeof(*script->xp));
  *script->xp = (struct xparse) {
    .stack = { .usize = sizeof(struct xnode*) },
    .xerr = xstr_create_empty(),
    .val = *buf,
  };
  script->xp->yy = malloc(sizeof(yycontext));
  *script->xp->yy = (yycontext) {
    .x = script
  };

  if (!yyparse(script->xp->yy)) {
    rc = AK_ERROR_SCRIPT_SYNTAX;
    _yyerror(script->xp->yy);
    if (script->xp->xerr->size) {
      akerror(rc, "%s\n", script->xp->xerr->ptr);
    } else {
      akerror(rc, 0, 0);
    }
    goto finish;
  }

finish:
  _xparse_destroy(script->xp);
  script->xp = 0;
  if (rc) {
    _xnode_destroy(script);
  } else {
    *out = &script->base;
  }
  return rc;
}

static int _script_from_file(struct node *parent, const char *path, struct node **out) {
  *out = 0;
  struct value buf = utils_file_as_buf(path, CFG_SCRIPT_MAX_SIZE_BYTES);
  if (buf.error) {
    return value_destroy(&buf);
  }
  int ret = _script_from_buf(parent, &buf, out);
  value_destroy(&buf);
  return ret;
}

static void _project_destroy(struct project *proj) {
  if (proj) {
    for (int i = 0; i < proj->nodes.num; ++i) {
      struct xnode *x = XNODE_AT(&proj->nodes, i);
      _xnode_destroy(x);
    }
    ulist_destroy_keep(&proj->nodes);
    pool_destroy(proj->pool);
  }
}

int project_open(const char *script_path, struct project **out) {
  *out = 0;
  struct node *n;
  int rc = _script_from_file(0, script_path, &n);
  RCGO(rc, finish);
  *out = n->project;
finish:
  return rc;
}

int project_build(struct project *p) {
  return 0;
}

void project_close(struct project **pp) {
  if (pp && *pp) {
    _project_destroy(*pp);
    *pp = 0;
  }
}
