#include "script.h"
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

static int _input(struct _yycontext *yy, char *buf, int max_size) {
  struct xparse *xp = yy->x->xp;
  int cnt = 0;
  while (cnt < max_size && xp->pos < xp->val.len) {
    char ch = *((char*) xp->val.buf + xp->pos);
    *(buf + cnt) = ch;
    ++xp->pos;
    ++cnt;
  }
  return cnt;
}

static void _xnode_destroy(struct xnode *x) {
  _xparse_destroy(x->xp);
  x->xp = 0;
}

static unsigned _rule_type(const char *key) {
  if (strcmp(key, "$") == 0) {
    return NODE_TYPE_SUBST;
  } else if (strcmp(key, "meta") == 0) {
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

static void _node_register(struct script *p, struct xnode *x) {
  x->base.index = p->nodes.num;
  ulist_push(&p->nodes, &x);
}

static struct xnode* _push_and_register(struct _yycontext *yy, struct xnode *x) {
  struct ulist *s = &yy->x->xp->stack;
  ulist_push(s, &x);
  _node_register(yy->x->base.project, x);
  return x;
}

static struct xnode* _node_text(struct  _yycontext *yy, const char *text) {
  struct script *p = XPROJECT(yy->x);
  struct xnode *x = pool_calloc(p->pool, sizeof(*x));
  x->base.project = p;
  x->base.value = pool_strdup(p->pool, text);
  x->base.type = NODE_TYPE_VALUE;
  return x;
}

static struct xnode* _node_text_push(struct  _yycontext *yy, const char *text) {
  return _push_and_register(yy, _node_text(yy, text));
}

static char* _text_escaped(char *wp, const char *rp) {
  // Replace: \} \{ \n \r \t
  int esc = 0;
  char *ret = wp;
  while (*rp != '\0') {
    if (esc) {
      switch (*rp) {
        case 'n':
          *wp = '\n';
          break;
        case 'r':
          *wp = '\r';
          break;
        case 't':
          *wp = '\t';
          break;
        default:
          *wp = *rp;
          break;
      }
      esc = 0;
    } else if (*rp == '\\') {
      esc = 1;
      ++rp;
      continue;
    } else {
      *wp = *rp;
    }
    ++wp;
    ++rp;
  }
  *wp = '\0';
  return ret;
}

static struct xnode* _node_text_escaped_push(struct  _yycontext *yy, const char *text) {
  char buf[strlen(text) + 1];
  return _push_and_register(yy, _node_text(yy, _text_escaped(buf, text)));
}

static struct xnode* _rule(struct _yycontext *yy, struct xnode *key) {
  struct xparse *xp = yy->x->xp;
  struct ulist *s = &xp->stack;
  key->base.type = _rule_type(key->base.value);
  while (s->num) {
    struct xnode *x = XNODE_PEEK(s);
    if (x != key) {
      x->base.next = key->base.child;
      x->base.parent = &key->base;
      key->base.child = &x->base;
      ulist_pop(s);
    } else {
      // Keep rule on the stack
      break;
    }
  }
  return key;
}

static void _finish(struct _yycontext *yy) {
  struct xnode *root = yy->x;
  struct xparse *xp = root->xp;
  struct ulist *s = &xp->stack;
  while (s->num) {
    struct xnode *x = XNODE_PEEK(s);
    x->base.next = root->base.child;
    root->base.child = &x->base;
    ulist_pop(s);
  }
}

///////////////////////////////////////////////////////////////////////////

static int _node_visit(struct node *n, int lvl, void *ctx, int (*visitor)(struct node*, int, void*)) {
  int ret = visitor(n, lvl, ctx);
  if (ret) {
    return ret;
  }
  for (struct node *c = n->child; c; c = c->next) {
    ret = _node_visit(c, lvl + 1, ctx, visitor);
    if (ret) {
      return ret;
    }
  }
  return visitor(n, -lvl, ctx);
}

static int _script_from_value(
  struct node *parent, const struct value *val,
  struct node **out) {
  int rc = 0;
  struct xnode *script = 0;

  if (!parent) {
    struct pool *pool = pool_create_empty();
    struct script *project = pool_calloc(pool, sizeof(*project));
    project->pool = pool;
    ulist_init(&project->nodes, 64, sizeof(struct node*));

    script = pool_calloc(pool, sizeof(*script));
    script->base.project = project;
    project->root = (struct node*) script;
  } else {
    script = pool_calloc(parent->project->pool, sizeof(*script));
    script->base.parent = parent;
  }

  script->base.value = "script";
  script->base.type = NODE_TYPE_SCRIPT;
  _node_register(script->base.project, script);

  script->xp = xmalloc(sizeof(*script->xp));
  *script->xp = (struct xparse) {
    .stack = { .usize = sizeof(struct xnode*) },
    .xerr = xstr_create_empty(),
    .val = *val,
  };

  yycontext *yy = script->xp->yy = xmalloc(sizeof(yycontext));
  *yy = (yycontext) {
    .x = script
  };

  if (!yyparse(yy)) {
    rc = AK_ERROR_SCRIPT_SYNTAX;
    _yyerror(yy);
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
  int ret = _script_from_value(parent, &buf, out);
  value_destroy(&buf);
  return ret;
}

static void _project_destroy(struct script *p) {
  if (p) {
    for (int i = 0; i < p->nodes.num; ++i) {
      struct xnode *x = XNODE_AT(&p->nodes, i);
      _xnode_destroy(x);
    }
    ulist_destroy_keep(&p->nodes);
    pool_destroy(p->pool);
  }
}

int script_open(const char *script_path, struct script **out) {
  *out = 0;
  struct node *n;
  int rc = _script_from_file(0, script_path, &n);
  RCGO(rc, finish);


  *out = n->project;
finish:
  return rc;
}

int script_build(struct script *p) {
  return 0;
}

void script_close(struct script **pp) {
  if (pp && *pp) {
    _project_destroy(*pp);
    *pp = 0;
  }
}

struct node_print_ctx {
  struct xstr *xstr;
};

static int _node_dump_visitor(struct node *n, int lvl, void *d) {
  struct node_print_ctx *ctx = d;
  struct xstr *xstr = ctx->xstr;
  if (lvl > 0) {
    xstr_cat_repeat(xstr, ' ', (lvl - 1) * NODE_PRINT_INDENT);
    const char *v = n->value ? n->value : "";
    if (node_is_value(n)) {
      xstr_printf(xstr, "%s:0x%x\n", v, n->type);
    } else {
      xstr_printf(xstr, "%s:0x%x {\n", v, n->type);
    }
  } else if (lvl < 0 && node_is_rule(n)) {
    xstr_cat_repeat(xstr, ' ', (-lvl - 1) * NODE_PRINT_INDENT);
    xstr_cat2(xstr, "}\n", 2);
  }
  return 0;
}

void script_print(struct script *p, struct xstr *xstr) {
  if (!p || !p->root) {
    xstr_cat(xstr, "null");
    return;
  }
  struct node_print_ctx ctx = {
    .xstr = xstr,
  };
  _node_visit(p->root, 1, &ctx, _node_dump_visitor);
}

#ifdef TESTS

int test_script_parse(const char *script_path, struct script **out) {
  *out = 0;
  struct node *n;
  int rc = _script_from_file(0, script_path, &n);
  RCGO(rc, finish);
  *out = n->project;
finish:
  return rc;
}

#endif
