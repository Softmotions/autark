#include "env.h"
#include "script.h"
#include "utils.h"
#include "log.h"
#include "pool.h"
#include "xstr.h"
#include "alloc.h"
#include "nodes.h"
#include "autark.h"
#include "config.h"
#include "paths.h"
#include "deps.h"

#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#define XNODE(n__)              ((struct xnode*) (n__))
#define NODE_AT(list__, idx__)  *(struct node**) ulist_get(list__, idx__)
#define NODE_PEEK(list__)       ((list__)->num ? *(struct node**) ulist_peek(list__) : 0)
#define XNODE_AT(list__, idx__) XNODE(NODE_AT(list__, idx__))
#define XNODE_PEEK(list__)      ((list__)->num ? XNODE(*(struct node**) ulist_peek(list__)) : 0)
#define XCTX(n__)               (n__)->base.ctx
#define NODE(x__)               (struct node*) (x__)
#define NODE_IS_EXCLUDED(n__)   (((n__)->flags & NODE_FLG_EXCLUDED) != 0)
#define NODE_IS_INCLUDED(n__)   (!NODE_IS_EXCLUDED(n__))
#define NODE_IS_SETUP(n__)      (((n__)->flags & NODE_FLG_SETUP) != 0)

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

static int _node_bind(struct node *n);

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
  struct node *n = &x->base;
  if (n->unit && n->unit->n == n) {
    n->unit->n = 0;
  }
  if (x->base.dispose) {
    x->base.dispose(n);
  }
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

static const char* _node_file(struct node *n) {
  for ( ; n; n = n->parent) {
    if (n->type == NODE_TYPE_SCRIPT) {
      return n->value;
    }
  }
  return "script";
}

void node_fatal(int rc, struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akfatal(rc, "%s:%d %s:0x%x %s", _node_file(n), n->pos, n->value, n->type, xstr_ptr(xstr));
  } else {
    akfatal(rc, "%s:%d %s:0x%x", _node_file(n), n->pos, n->value, n->type);
  }
  xstr_destroy(xstr);
}

int node_error(int rc, struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akerror(rc, "%s:%d %s:0x%x %s", _node_file(n), n->pos, n->value, n->type, xstr_ptr(xstr));
  } else {
    akerror(rc, "%s:%d %s:0x%x", _node_file(n), n->pos, n->value, n->type);
  }
  xstr_destroy(xstr);
  return rc;
}

static void _node_register(struct sctx *p, struct xnode *x) {
  x->base.index = p->nodes.num;
  ulist_push(&p->nodes, &x);
}

static struct xnode* _push_and_register(struct _yycontext *yy, struct xnode *x) {
  struct ulist *s = &yy->x->xp->stack;
  ulist_push(s, &x);
  _node_register(yy->x->base.ctx, x);
  return x;
}

static struct xnode* _node_text(struct  _yycontext *yy, const char *text) {
  struct sctx *ctx = XCTX(yy->x);
  struct xnode *x = pool_calloc(g_env.pool, sizeof(*x));
  x->base.value = pool_strdup(g_env.pool, text);
  x->base.ctx = ctx;
  x->base.type = NODE_TYPE_VALUE;
  x->base.pos = yy->__pos;
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
    x->base.parent = &root->base;
    root->base.child = &x->base;
    ulist_pop(s);
  }
}

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
  struct node        *parent,
  const char         *file,
  const struct value *val,
  struct node       **out) {
  int rc = 0;

  struct xnode *x = 0;
  struct pool *pool = g_env.pool;

  if (!parent) {
    struct sctx *ctx = pool_calloc(pool, sizeof(*ctx));
    ulist_init(&ctx->nodes, 64, sizeof(struct node*));

    x = pool_calloc(pool, sizeof(*x));
    x->base.ctx = ctx;
    ctx->root = (struct node*) x;
  } else {
    x = pool_calloc(pool, sizeof(*x));
    x->base.parent = parent;
    x->base.ctx = parent->ctx;
  }

  if (file) {
    struct unit *unit;
    if (g_env.units.num == 1) {
      unit = unit_peek();
      akassert(unit->n == 0); // We are the root script
    } else {
      unit = unit_create(file, UNIT_FLG_SRC_CWD, g_env.pool);
    }
    unit->n = &x->base;
    x->base.unit = unit;
    unit_ch_dir(unit);
  }

  x->base.value = pool_strdup(pool, file ? file : "<script>");
  x->base.type = NODE_TYPE_SCRIPT;
  _node_register(x->base.ctx, x);
  _node_bind(&x->base);

  x->xp = xmalloc(sizeof(*x->xp));
  *x->xp = (struct xparse) {
    .stack = { .usize = sizeof(struct xnode*) },
    .xerr = xstr_create_empty(),
    .val = *val,
  };

  yycontext *yy = x->xp->yy = xmalloc(sizeof(yycontext));
  *yy = (yycontext) {
    .x = x
  };

  if (!yyparse(yy)) {
    rc = AK_ERROR_SCRIPT_SYNTAX;
    _yyerror(yy);
    if (x->xp->xerr->size) {
      akerror(rc, "%s\n", x->xp->xerr->ptr);
    } else {
      akerror(rc, 0, 0);
    }
    goto finish;
  }

finish:
  _xparse_destroy(x->xp);
  x->xp = 0;
  if (rc) {
    _xnode_destroy(x);
  } else {
    *out = &x->base;
  }
  return rc;
}

static int _script_from_file(struct node *parent, const char *file, struct node **out) {
  *out = 0;
  struct value buf = utils_file_as_buf(file, CFG_SCRIPT_MAX_SIZE_BYTES);
  if (buf.error) {
    akerror(buf.error, "Failed to read script file: %s", file);
    return value_destroy(&buf);
  }
  int ret = _script_from_value(parent, file, &buf, out);
  value_destroy(&buf);
  return ret;
}

static void _script_destroy(struct sctx *e) {
  if (e) {
    for (int i = 0; i < e->nodes.num; ++i) {
      struct xnode *x = XNODE_AT(&e->nodes, i);
      _xnode_destroy(x);
    }
    ulist_destroy_keep(&e->nodes);
  }
}

static int _node_bind(struct node *n) {
  if (!(n->flags & NODE_FLG_BOUND)) {
    n->flags |= NODE_FLG_BOUND;
    switch (n->type) {
      case NODE_TYPE_SCRIPT:
        return node_script_setup(n);
      case NODE_TYPE_META:
        return node_meta_setup(n);
      case NODE_TYPE_CONSUMES:
        return node_consumes_setup(n);
      case NODE_TYPE_CHECK:
        return node_check_setup(n);
      case NODE_TYPE_SOURCES:
        return node_sources_setup(n);
      case NODE_TYPE_FLAGS:
        return node_flags_setup(n);
      case NODE_TYPE_EXEC:
        return node_exec_setup(n);
      case NODE_TYPE_STATIC:
        return node_static_setup(n);
      case NODE_TYPE_SHARED:
        return node_shared_setup(n);
      case NODE_TYPE_INCLUDE:
        return node_include_setup(n);
      case NODE_TYPE_IF:
        return node_if_setup(n);
      case NODE_TYPE_SUBST:
        return node_subst_setup(n);
    }
  }
  return 0;
}

static struct node* _unit_node_find(struct node *n) {
  for ( ; n; n = n->parent) {
    if (n->unit) {
      return n;
    }
  }
  akassert(0);
}

static void _node_context_push(struct node *n) {
  struct node *s = _unit_node_find(n);
  unit_push(s->unit);
}

static void _node_context_pop(struct node *n) {
  unit_pop();
}

static void _node_reset(struct node *n) {
  n->flags &= ~(NODE_FLG_UPDATED | NODE_FLG_BUILT | NODE_FLG_SETUP | NODE_FLG_EXCLUDED);
}

static void _node_setup(struct node *n) {
  if (!(n->flags & NODE_FLG_SETUP)) {
    n->flags |= NODE_FLG_SETUP;
    if (n->setup) {
      _node_context_push(n);
      n->setup(n);
      _node_context_pop(n);
    }
  }
}

void node_build(struct node *n) {
  if (!(n->flags & NODE_FLG_BUILT)) {
    if (n->build) {
      struct xnode *x = (void*) n;
      x->bld_calls++;
      if (x->bld_calls > 1) {
        node_fatal(AK_ERROR_CYCLIC_BUILD_DEPS, n, 0);
      }
      _node_context_push(n);
      n->build(n);
      _node_context_pop(n);
      x->bld_calls--;
    }
    n->flags |= NODE_FLG_BUILT;
  }
}

static int _script_bind(struct sctx *s) {
  int rc = 0;
  for (int i = 0; i < s->nodes.num; ++i) {
    struct node *n = NODE_AT(&s->nodes, i);
    if (!(n->flags & NODE_FLG_BOUND)) {
      RCC(rc, finish, _node_bind(n));
    }
  }
finish:
  return rc;
}

int script_open(const char *file, struct sctx **out) {
  *out = 0;
  int rc = 0;
  struct node *n;
  akassert(file);
  autark_build_prepare(file);

  char buf[PATH_MAX];
  strncpy(buf, file, PATH_MAX - 1);
  buf[PATH_MAX - 1] = '\0';
  const char *path = path_basename(buf);

  RCC(rc, finish, _script_from_file(0, path, &n));
  RCC(rc, finish, _script_bind(n->ctx));
  *out = n->ctx;
finish:
  return rc;
}

void script_setup(struct sctx *s) {
  for (int i = 0; i < s->nodes.num; ++i) {
    struct node *n = NODE_AT(&s->nodes, i);
    _node_reset(n);
  }
  for (int i = 0; i < s->nodes.num; ++i) {
    struct node *n = NODE_AT(&s->nodes, i);
    if (n->type == NODE_TYPE_CHECK && NODE_IS_INCLUDED(n)) {
      _node_setup(n);
    }
  }
  int nsetup;
  do {
    nsetup = 0;
    for (int i = 0; i < s->nodes.num; ++i) {
      struct node *n = NODE_AT(&s->nodes, i);
      if (n->type != NODE_TYPE_CHECK && NODE_IS_INCLUDED(n) && !NODE_IS_SETUP(n)) {
        ++nsetup;
        _node_setup(n);
      }
    }
  } while (nsetup > 0);
}

void script_build(struct sctx *s) {
  script_setup(s);
  for (int i = 0; i < s->nodes.num; ++i) {
    struct node *n = NODE_AT(&s->nodes, i);
    node_build(n);
  }
}

void script_close(struct sctx **sp) {
  if (sp && *sp) {
    _script_destroy(*sp);
    *sp = 0;
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

void script_dump(struct sctx *p, struct xstr *xstr) {
  if (!p || !p->root) {
    xstr_cat(xstr, "null");
    return;
  }
  struct node_print_ctx ctx = {
    .xstr = xstr,
  };
  _node_visit(p->root, 1, &ctx, _node_dump_visitor);
}

const char* node_env_get(struct node *n, const char *key) {
  const char *ret = 0;
  for ( ; n; n = n->parent) {
    if (n->unit) {
      ret = unit_env_get(n->unit, key);
      if (ret) {
        return ret;
      }
    }
  }
  return 0;
}

void node_env_set(struct node *n, const char *key, const char *val) {
  for ( ; n; n = n->parent) {
    if (n->unit) {
      if (val) {
        unit_env_set(n->unit, key, val);
      } else {
        unit_env_remove(n->unit, key);
      }
      return;
    }
  }
}

void node_resolve(struct node_resolve *r) {
  akassert(r && r->path);

  int rc;
  struct deps deps;
  struct pool *pool = pool_create_empty();

  const char *deps_path = pool_printf(pool, "%s.deps", r->path);
  const char *env_path = pool_printf(pool, "%s.env", r->path);
  const char *deps_path_tmp = pool_printf(pool, "%s.deps.tmp", r->path);
  const char *env_path_tmp = pool_printf(pool, "%s.env.tmp", r->path);

  unlink(deps_path_tmp);
  unlink(env_path_tmp);

  r->pool = pool;
  r->num_deps = 0;
  r->num_outdated = 0;
  r->deps_path_tmp = deps_path_tmp;
  r->env_path_tmp = env_path_tmp;


  if (!deps_open(deps_path, DEPS_OPEN_READONLY, &deps)) {
    while (deps_cur_next(&deps)) {
      ++r->num_deps;
      if (deps_cur_is_outdated(&deps)) {
        ++r->num_outdated;
        if (r->on_outdated_dependency) {
          r->on_outdated_dependency(r, &deps);
        }
      }
    }
  }
  if (deps.file) {
    deps_close(&deps);
  }

  bool env_created = false;

  if (r->on_resolve && (r->num_deps == 0 || r->num_outdated)) {
    r->on_resolve(r);
    if (access(deps_path_tmp, R_OK) == 0) {
      rc = utils_rename_file(deps_path_tmp, deps_path);
      if (rc) {
        akfatal(rc, "Rename failed of %s to %s", deps_path_tmp, deps_path);
      }
    }
    if (r->on_env_value && !access(env_path_tmp, R_OK)) {
      rc = utils_rename_file(env_path_tmp, env_path);
      if (rc) {
        akfatal(rc, "Rename failed of %s to %s", env_path_tmp, env_path);
      }
      env_created = true;
    }
  }

  if (r->on_env_value && (r->mode & NODE_RESOLVE_ENV_ALWAYS) && (env_created || !access(env_path, R_OK))) {
    char buf[4096];
    FILE *f = fopen(env_path, "r");
    if (f) {
      while (fgets(buf, sizeof(buf), f)) {
        char *p = strchr(buf, '=');
        if (p) {
          *p = '\0';
          char *val = p + 1;
          for (int vlen = strlen(val); vlen >= 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r'); --vlen) {
            val[vlen - 1] = '\0';
          }
          r->on_env_value(r, buf, val);
        }
      }
      fclose(f);
    } else {
      akfatal(rc, "Failed to open env file: %s", env_path);
    }
  }

  pool_destroy(pool);
}

#ifdef TESTS

int test_script_parse(const char *script_path, struct sctx **out) {
  *out = 0;
  char buf[PATH_MAX];
  akassert(getcwd(buf, sizeof(buf)));
  int rc = script_open(script_path, out);
  chdir(buf);
  return rc;
}

#endif
