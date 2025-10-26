#ifndef _AMALGAMATE_
#include "script_impl.h"
#endif

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

static unsigned _rule_type(const char *key, unsigned *flags) {
  *flags = 0;
  if (key[0] == '.' && key[1] == '.') {
    key += 2;
  }
  if (key[0] == '!') {
    *flags = NODE_FLG_NEGATE;
    key += 1;
  }
  if (strcmp(key, "$") == 0 || strcmp(key, "@") == 0 || strcmp(key, "@@") == 0) {
    return NODE_TYPE_SUBST;
  } else if (strcmp(key, "^") == 0) {
    return NODE_TYPE_JOIN;
  } else if (strcmp(key, "set") == 0 || strcmp(key, "env") == 0) {
    return NODE_TYPE_SET;
  } else if (strcmp(key, "check") == 0) {
    return NODE_TYPE_CHECK;
  } else if (strcmp(key, "include") == 0) {
    return NODE_TYPE_INCLUDE;
  } else if (strcmp(key, "if") == 0) {
    return NODE_TYPE_IF;
  } else if (strcmp(key, "run") == 0 || strcmp(key, "run-on-install") == 0) {
    return NODE_TYPE_RUN;
  } else if (strcmp(key, "meta") == 0) {
    return NODE_TYPE_META;
  } else if (strcmp(key, "cc") == 0 || strcmp(key, "cxx") == 0) {
    return NODE_TYPE_CC;
  } else if (strcmp(key, "configure") == 0) {
    return NODE_TYPE_CONFIGURE;
  } else if (strcmp(key, "%") == 0) {
    return NODE_TYPE_BASENAME;
  } else if (strcmp(key, "foreach") == 0) {
    return NODE_TYPE_FOREACH;
  } else if (strcmp(key, "in-sources") == 0) {
    return NODE_TYPE_IN_SOURCES;
  } else if (  strcmp(key, "S") == 0 || strcmp(key, "C") == 0
            || strcmp(key, "SS") == 0 || strcmp(key, "CC") == 0) {
    return NODE_TYPE_DIR;
  } else if (strcmp(key, "option") == 0) {
    return NODE_TYPE_OPTION;
  } else if (strcmp(key, "error") == 0) {
    return NODE_TYPE_ERROR;
  } else if (strcmp(key, "echo") == 0) {
    return NODE_TYPE_ECHO;
  } else if (strcmp(key, "install") == 0) {
    return NODE_TYPE_INSTALL;
  } else if (strcmp(key, "library") == 0) {
    return NODE_TYPE_FIND;
  } else if (strcmp(key, "macro") == 0) {
    return NODE_TYPE_MACRO;
  } else if (strcmp(key, "call") == 0) {
    return NODE_TYPE_CALL;
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

void node_info(struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akinfo("%s: %s", n->name, xstr_ptr(xstr));
  } else {
    akinfo(n->name, 0);
  }
  xstr_destroy(xstr);
}

void node_warn(struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akwarn("%s: %s", n->name, xstr_ptr(xstr));
  } else {
    akinfo(n->name, 0);
  }
  xstr_destroy(xstr);
}

void node_fatal(int rc, struct node *n, const char *fmt, ...) {
  struct xstr *xstr = xstr_create_empty();
  if (fmt) {
    va_list ap;
    va_start(ap, fmt);
    xstr_printf_va(xstr, fmt, ap);
    va_end(ap);
    akfatal(rc, "%s %s", n->name, xstr_ptr(xstr));
  } else {
    akfatal(rc, n->name, 0);
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
    akerror(rc, "%s %s:0x%x %s", _node_file(n), n->value, n->type, xstr_ptr(xstr));
  } else {
    akerror(rc, "%s %s:0x%x", _node_file(n), n->value, n->type);
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
  x->base.lnum = yy->x->lnum + 1;
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
  unsigned flags = 0;
  key->base.type = _rule_type(key->base.value, &flags);
  key->base.flags |= flags;
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

int node_visit(struct node *n, int lvl, void *ctx, int (*visitor)(struct node*, int, void*)) {
  return _node_visit(n, lvl, ctx, visitor);
}

static void _preprocess_script(struct value *v) {
  const char *p = v->buf;
  char *nv = xmalloc(v->len + 1), *w = nv;
  bool comment = false, eol = true;
  while (*p) {
    if (*p == '\n') {
      eol = true;
      comment = false;
    } else if (eol) {
      const char *sp = p;
      while (utils_char_is_space(*sp)) {
        ++sp;
      }
      if (*sp == '#') {
        comment = true;
      }
      eol = false;
    }
    if (!comment) {
      *w++ = *p++;
    } else {
      p++;
    }
  }
  *w = '\0';
  free(v->buf);
  v->buf = nv;
  v->len = w - nv;
}

static int _script_from_value(
  struct node  *parent,
  const char   *file,
  struct value *val,
  struct node **out) {
  int rc = 0;

  struct xnode *x = 0;
  struct pool *pool = g_env.pool;

  char prevcwd_[PATH_MAX];
  char *prevcwd = 0;

  _preprocess_script(val);

  if (!parent) {
    struct sctx *ctx = pool_calloc(pool, sizeof(*ctx));
    ulist_init(&ctx->nodes, 64, sizeof(struct node*));
    ctx->products = map_create_str(map_k_free);

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
    char *f = path_relativize(g_env.project.root_dir, file);
    if (parent == 0 && g_env.units.num == 1) {
      unit = unit_peek();
      akassert(unit->n == 0); // We are the root script
    } else {
      unit = unit_create(f, UNIT_FLG_SRC_CWD, g_env.pool);
    }
    unit->n = &x->base;
    x->base.unit = unit;
    unit_ch_dir(&(struct unit_ctx) { unit, 0 }, prevcwd_);
    prevcwd = prevcwd_;
    x->base.value = pool_strdup(pool, f);
    free(f);
  } else {
    x->base.value = "<script>";
  }

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
  if (prevcwd) {
    akcheck(chdir(prevcwd));
  }
  return rc;
}

static int _script_from_file(struct node *parent, const char *file, struct node **out) {
  *out = 0;
  struct value buf = utils_file_as_buf(file, (1024 * 1024));
  if (buf.error) {
    return value_destroy(&buf);
  }
  int ret = _script_from_value(parent, file, &buf, out);
  value_destroy(&buf);
  return ret;
}

static void _script_destroy(struct sctx *s) {
  if (s) {
    for (int i = 0; i < s->nodes.num; ++i) {
      struct xnode *x = XNODE_AT(&s->nodes, i);
      _xnode_destroy(x);
    }
    map_destroy(s->products);
    ulist_destroy_keep(&s->nodes);
  }
}

static int _node_bind(struct node *n) {
  int rc = 0;
  // Tree has been built since its safe to compute node name
  if (n->type != NODE_TYPE_SCRIPT) {
    n->name = pool_printf(g_env.pool, "%s:%-3u %5s", _node_file(n), n->lnum, n->value);
  } else {
    n->name = pool_printf(g_env.pool, "%s     %5s", _node_file(n), "");
  }
  n->vfile = pool_printf(g_env.pool, ".%u", n->index);

  if (!(n->flags & NODE_FLG_BOUND)) {
    n->flags |= NODE_FLG_BOUND;
    switch (n->type) {
      case NODE_TYPE_SCRIPT:
        rc = node_script_setup(n);
        break;
      case NODE_TYPE_META:
        rc = node_meta_setup(n);
        break;
      case NODE_TYPE_CHECK:
        rc = node_check_setup(n);
        break;
      case NODE_TYPE_SET:
        rc = node_set_setup(n);
        break;
      case NODE_TYPE_INCLUDE:
        rc = node_include_setup(n);
        break;
      case NODE_TYPE_IF:
        rc = node_if_setup(n);
        break;
      case NODE_TYPE_SUBST:
        rc = node_subst_setup(n);
        break;
      case NODE_TYPE_RUN:
        rc = node_run_setup(n);
        break;
      case NODE_TYPE_JOIN:
        rc = node_join_setup(n);
        break;
      case NODE_TYPE_CC:
        rc = node_cc_setup(n);
        break;
      case NODE_TYPE_CONFIGURE:
        rc = node_configure_setup(n);
        break;
      case NODE_TYPE_BASENAME:
        rc = node_basename_setup(n);
        break;
      case NODE_TYPE_FOREACH:
        rc = node_foreach_setup(n);
        break;
      case NODE_TYPE_IN_SOURCES:
        rc = node_in_sources_setup(n);
        break;
      case NODE_TYPE_DIR:
        rc = node_dir_setup(n);
        break;
      case NODE_TYPE_OPTION:
        rc = node_option_setup(n);
        break;
      case NODE_TYPE_ERROR:
        rc = node_error_setup(n);
        break;
      case NODE_TYPE_ECHO:
        rc = node_echo_setup(n);
        break;
      case NODE_TYPE_INSTALL:
        rc = node_install_setup(n);
        break;
      case NODE_TYPE_FIND:
        rc = node_find_setup(n);
        break;
      case NODE_TYPE_MACRO:
        rc = node_macro_setup(n);
        break;
      case NODE_TYPE_CALL:
        rc = node_call_setup(n);
        break;
    }

    switch (n->type) {
      case NODE_TYPE_RUN:
      case NODE_TYPE_SUBST: {
        struct node *nn = node_find_parent_of_type(n, NODE_TYPE_IN_SOURCES);
        if (nn) {
          if (n->flags & NODE_FLG_IN_CACHE) {
            n->flags &= ~(NODE_FLG_IN_CACHE);
          }
          n->flags |= NODE_FLG_IN_SRC;
        }
      }
    }
  }
  return rc;
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
  unit_push(s->unit, n);
}

static void _node_context_pop(struct node *n) {
  unit_pop();
}

const char* node_value(struct node *n) {
  if (n) {
    node_setup(n);
    if (n->value_get) {
      _node_context_push(n);
      const char *ret = n->value_get(n);
      _node_context_pop(n);
      return ret;
    } else {
      return n->value;
    }
  } else {
    return 0;
  }
}

void node_reset(struct node *n) {
  n->flags &= ~(NODE_FLG_UPDATED | NODE_FLG_BUILT | NODE_FLG_SETUP | NODE_FLG_INIT);
}

static void _init_subnodes(struct node *n) {
  int c;
  do {
    c = 0;
    for (struct node *nn = n->child; nn; nn = nn->next) {
      if (!node_is_init(nn)) {
        ++c;
        node_init(nn);
      }
    }
  } while (c > 0);
}

static void _setup_subnodes(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_setup(nn);
  }
}

static void _build_subnodes(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_build(nn);
  }
}

static void _post_build_subnodes(struct node *n) {
  for (struct node *nn = n->child; nn; nn = nn->next) {
    node_post_build(nn);
  }
}

static void _macro_call_init(struct node *n) {
}

static void _macros_init(struct sctx *s) {
  for (int i = 0; i < s->nodes.num; ++i) {
    struct node *n = NODE_AT(&s->nodes, i);
    if (n->type == NODE_TYPE_MACRO) {
      n->flags |= NODE_FLG_SETUP;
      _node_context_push(n);
      n->init(n);
      _node_context_pop(n);
    }
  }
  for (int i = 0; i < s->nodes.num; ++i) {
    struct node *n = NODE_AT(&s->nodes, i);
    if (n->type == NODE_TYPE_CALL && !(n->flags & NODE_FLG_CALLED)) {
      n->flags |= NODE_FLG_CALLED;
      _node_context_push(n);
      _macro_call_init(n);
      _node_context_pop(n);
    }
  }
}

void node_init(struct node *n) {
  if (!node_is_init(n)) {
    n->flags |= NODE_FLG_INIT;
    switch (n->type) {
      case NODE_TYPE_IF:
      case NODE_TYPE_INCLUDE:
      case NODE_TYPE_FOREACH:
      case NODE_TYPE_IN_SOURCES:
        _node_context_push(n);
        n->init(n);
        _init_subnodes(n);
        _node_context_pop(n);
        break;
      default:
        if (n->init || n->child) {
          _node_context_push(n);
          _init_subnodes(n);
          if (n->init) {
            n->init(n);
          }
          _node_context_pop(n);
        }
    }
  }
}

void node_setup(struct node *n) {
  if (!node_is_setup(n)) {
    n->flags |= NODE_FLG_SETUP;
    if (n->type != NODE_TYPE_FOREACH) {
      if (n->child || n->setup) {
        _node_context_push(n);
        _setup_subnodes(n);
        if (n->setup) {
          n->setup(n);
        }
        _node_context_pop(n);
      }
    } else {
      _node_context_push(n);
      n->setup(n);
      _setup_subnodes(n);
      _node_context_pop(n);
    }
  }
}

void node_build(struct node *n) {
  if (!node_is_built(n)) {
    _build_subnodes(n);
    if (n->build) {
      if (g_env.verbose) {
        node_info(n, "Build");
      }
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

void node_post_build(struct node *n) {
  if (!node_is_post_built(n)) {
    _post_build_subnodes(n);
    if (n->post_build) {
      if (g_env.verbose) {
        node_info(n, "Post-build");
      }
      struct xnode *x = (void*) n;
      x->bld_calls++;
      if (x->bld_calls > 1) {
        node_fatal(AK_ERROR_CYCLIC_BUILD_DEPS, n, 0);
      }
      _node_context_push(n);
      n->post_build(n);
      _node_context_pop(n);
      x->bld_calls--;
    }
    n->flags |= NODE_FLG_POST_BUILT;
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

static int _script_open(struct node *parent, const char *file, struct node **out) {
  int rc = 0;
  struct node *n;
  char buf[PATH_MAX];
  autark_build_prepare(path_normalize(file, buf));
  RCC(rc, finish, _script_from_file(parent, buf, &n));
  RCC(rc, finish, _script_bind(n->ctx));
  if (out) {
    *out = n;
  }
finish:
  return rc;
}

int script_open(const char *file, struct sctx **out) {
  if (out) {
    *out = 0;
  }
  struct node *n = 0;
  int rc = _script_open(0, file, &n);
  if (!rc) {
    struct unit *root = unit_root();
    if (g_env.install.enabled) {
      unit_env_set_val(root, "INSTALL_ENABLED", "1");
    }
    if (g_env.install.prefix_dir || g_env.install.bin_dir) {
      unit_env_set_val(root, "INSTALL_PREFIX", g_env.install.prefix_dir);
      unit_env_set_val(root, "INSTALL_BIN_DIR", g_env.install.bin_dir);
      unit_env_set_val(root, "INSTALL_LIB_DIR", g_env.install.lib_dir);
      unit_env_set_val(root, "INSTALL_DATA_DIR", g_env.install.data_dir);
      unit_env_set_val(root, "INSTALL_INCLUDE_DIR", g_env.install.include_dir);
      unit_env_set_val(root, "INSTALL_PKGCONFIG_DIR", g_env.install.pkgconf_dir);
      unit_env_set_val(root, "INSTALL_MAN_DIR", g_env.install.man_dir);
      if (g_env.verbose) {
        akinfo("%s: INSTALL_PREFIX=%s", root->rel_path, g_env.install.prefix_dir);
        akinfo("%s: INSTALL_BIN_DIR=%s", root->rel_path, g_env.install.bin_dir);
        akinfo("%s: INSTALL_LIB_DIR=%s", root->rel_path, g_env.install.lib_dir);
        akinfo("%s: INSTALL_DATA_DIR=%s", root->rel_path, g_env.install.data_dir);
        akinfo("%s: INSTALL_INCLUDE_DIR=%s", root->rel_path, g_env.install.include_dir);
        akinfo("%s: INSTALL_PKGCONFIG_DIR=%s", root->rel_path, g_env.install.pkgconf_dir);
        akinfo("%s: INSTALL_MAN_DIR=%s", root->rel_path, g_env.install.man_dir);
      }
    }
    if (out && n) {
      *out = n->ctx;
    }
  }
  return rc;
}

int script_include(struct node *parent, const char *file, struct node **out) {
  return _script_open(parent, file, out);
}

void script_build(struct sctx *s) {
  akassert(s->root);
  _macros_init(s);
  node_init(s->root);
  node_setup(s->root);
  node_build(s->root);
  node_post_build(s->root);
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
      ret = unit_env_get_raw(n->unit, key);
      if (ret) {
        return ret;
      }
    }
  }
  return getenv(key);
}

void node_env_set(struct node *n, const char *key, const char *val) {
  if (g_env.verbose) {
    if (val) {
      node_info(n, "%s=%s", key, val);
    } else {
      node_info(n, "UNSET %s", key);
    }
  }
  if (key[0] == '_' && key[1] == '\0') {
    // Skip on special '_' key
    return;
  }
  for ( ; n; n = n->parent) {
    if (n->unit) {
      if (val) {
        unit_env_set_val(n->unit, key, val);
      } else {
        unit_env_remove(n->unit, key);
      }
      return;
    }
  }
}

void node_env_set_node(struct node *n_, const char *key) {
  if (key[0] == '_' && key[1] == '\0') {
    // Skip on special '_' key
    return;
  }
  for (struct node *n = n_; n; n = n->parent) {
    if (n->unit) {
      unit_env_set_node(n->unit, key, n_);
      return;
    }
  }
}

void node_product_add(struct node *n, const char *prod, char pathbuf[PATH_MAX]) {
  char buf[PATH_MAX];
  if (!pathbuf) {
    pathbuf = buf;
  }
  if (is_vlist(prod)) {
    struct vlist_iter iter;
    vlist_iter_init(prod, &iter);
    while (vlist_iter_next(&iter)) {
      char path[PATH_MAX];
      utils_strnncpy(path, iter.item, iter.len, sizeof(path));
      prod = path_normalize(path, pathbuf);
      node_product_add_raw(n, prod);
    }
  } else {
    prod = path_normalize(prod, pathbuf);
    node_product_add_raw(n, prod);
  }
}

void node_product_add_raw(struct node *n, const char *prod) {
  char buf[PATH_MAX];
  struct sctx *s = n->ctx;
  struct node *nn = map_get(s->products, prod);
  if (nn) {
    if (nn == n) {
      return;
    }
    node_fatal(AK_ERROR_FAIL, n, "Product: '%s' was registered by other rule: %s", prod, nn->name);
  }
  utils_strncpy(buf, prod, sizeof(buf));
  char *dir = path_dirname(buf);
  if (dir) {
    path_mkdirs(dir);
  }
  map_put_str(s->products, prod, n);
}

struct node* node_by_product(struct node *n, const char *prod, char pathbuf[PATH_MAX]) {
  struct sctx *s = n->ctx;
  char buf[PATH_MAX];
  if (!pathbuf) {
    pathbuf = buf;
  }
  prod = path_normalize(prod, pathbuf);
  if (!prod) {
    node_fatal(errno, n, 0);
  }
  struct node *nn = map_get(s->products, prod);
  return nn;
}

struct node* node_by_product_raw(struct node *n, const char *prod) {
  struct sctx *s = n->ctx;
  return map_get(s->products, prod);
}

struct node* node_find_direct_child(struct node *n, int type, const char *val) {
  if (n) {
    for (struct node *nn = n->child; nn; nn = nn->next) {
      if (type == 0 || nn->type == type) {
        if (val == 0 || strcmp(nn->value, val) == 0) {
          return nn;
        }
      }
    }
  }
  return 0;
}

struct node* node_find_prev_sibling(struct node *n) {
  if (n) {
    struct node *p = n->parent;
    if (p) {
      for (struct node *prev = 0, *c = p->child; c; prev = c, c = c->next) {
        if (c == n) {
          return prev;
        }
      }
    }
  }
  return 0;
}

struct  node* node_find_parent_of_type(struct node *n, int type) {
  for (struct node *nn = n->parent; nn; nn = nn->parent) {
    if (type == 0 || nn->type == type) {
      return nn;
    }
  }
  return 0;
}

struct node_foreach* node_find_parent_foreach(struct node *n) {
  struct node *nn = node_find_parent_of_type(n, NODE_TYPE_FOREACH);
  if (nn) {
    return nn->impl;
  } else {
    return 0;
  }
}

bool node_is_value_may_be_dep_saved(struct node *n) {
  if (!n || n->type == NODE_TYPE_VALUE) { // Hardcoded value
    return false;
  }
  if (node_is_can_be_value(n)) {
    struct node_foreach *fe = node_find_parent_foreach(n);
    if (fe) {
      fe->access_cnt = 0;
      node_value(n);
      if (fe->access_cnt > 0) {
        return false;
      }
    }
    return true;
  }
  return false;
}

struct node* node_consumes_resolve(
  struct node *n,
  struct node *nn,
  struct ulist *paths,
  void (*on_resolved)(const char *path, void*),
  void *opq) {
  char prevcwd[PATH_MAX];
  char pathbuf[PATH_MAX];
  struct unit *unit = unit_peek();
  struct ulist rlist = { .usize = sizeof(char*) };
  struct pool *pool = pool_create_empty();

  if (nn || paths) {
    for ( ; nn; nn = nn->next) {
      const char *cv = node_value(nn);
      if (is_vlist(cv)) {
        struct vlist_iter iter;
        vlist_iter_init(cv, &iter);
        while (vlist_iter_next(&iter)) {
          cv = pool_strndup(pool, iter.item, iter.len);
          ulist_push(&rlist, &cv);
        }
      } else if (cv && *cv != '\0') {
        ulist_push(&rlist, &cv);
      }
    }

    if (paths) {
      for (int i = 0; i < paths->num; ++i) {
        const char *p = *(char**) ulist_get(paths, i);
        if (p && *p != '\0') {
          ulist_push(&rlist, &p);
        }
      }
    }

    unit_ch_cache_dir(unit, prevcwd);
    for (int i = 0; i < rlist.num; ++i) {
      const char *cv = *(char**) ulist_get(&rlist, i);
      struct node *pn = node_by_product(n, cv, pathbuf);
      if (pn) {
        node_build(pn);
      }
      if (path_is_exist(pathbuf)) {
        if (on_resolved) {
          on_resolved(pathbuf, opq);
        }
        ulist_remove(&rlist, i--);
      }
    }
    akcheck(chdir(prevcwd));

    if (rlist.num) {
      unit_ch_src_dir(unit, prevcwd);
      for (int i = 0; i < rlist.num; ++i) {
        const char *cv = *(char**) ulist_get(&rlist, i);
        akassert(path_normalize(cv, pathbuf));
        if (path_is_exist(pathbuf)) {
          if (on_resolved) {
            on_resolved(pathbuf, opq);
          }
        } else {
          node_fatal(AK_ERROR_DEPENDENCY_UNRESOLVED, n, "'%s'", cv);
        }
      }
      akcheck(chdir(prevcwd));
    }
  }

  ulist_destroy_keep(&rlist);
  pool_destroy(pool);
  return nn;
}

void node_add_unit_deps(struct node *n, struct deps *deps) {
  struct unit *prev = 0;
  for (struct node *nn = n; nn; nn = nn->parent) {
    if (nn->unit && nn->unit != prev) {
      prev = nn->unit;
      deps_add(deps, DEPS_TYPE_FILE, 0, nn->unit->source_path, 0);
    }
  }
}

void node_resolve(struct node_resolve *r) {
  akassert(r && r->path && r->n);

  int rc;
  struct deps deps = { 0 };
  struct pool *pool = pool_create_empty();
  struct unit *unit = unit_peek();

  const char *deps_path = pool_printf(pool, "%s/%s.deps", unit->cache_dir, r->path);
  const char *env_path = pool_printf(pool, "%s/%s.env", unit->cache_dir, r->path);
  const char *deps_path_tmp = pool_printf(pool, "%s/%s.deps.tmp", unit->cache_dir, r->path);
  const char *env_path_tmp = pool_printf(pool, "%s/%s.env.tmp", unit->cache_dir, r->path);

  unlink(deps_path_tmp);
  unlink(env_path_tmp);

  r->pool = pool;
  r->num_deps = 0;
  r->deps_path_tmp = deps_path_tmp;
  r->env_path_tmp = env_path_tmp;

  ulist_init(&r->resolve_outdated, 0, sizeof(struct resolve_outdated));

  if (r->on_init) {
    r->on_init(r);
  }

  if (!r->force_outdated && !deps_open(deps_path, DEPS_OPEN_READONLY, &deps)) {
    char *prev_outdated = 0;
    while (deps_cur_next(&deps)) {
      ++r->num_deps;

      bool outdated = false;
      if (prev_outdated && strcmp(prev_outdated, deps.resource) == 0) {
        outdated = false;
      } else if (deps.type == DEPS_TYPE_NODE_VALUE) {
        if (deps.serial >= 0 && deps.serial < r->node_val_deps.num) {
          struct node *nv = *(struct node**) ulist_get(&r->node_val_deps, (unsigned int) deps.serial);
          const char *val = node_value(nv);
          outdated = val == 0 || strcmp(val, deps.resource) != 0;
        } else {
          outdated = true;
        }
      } else {
        outdated = deps_cur_is_outdated(r->n, &deps);
      }

      if (outdated) {
        prev_outdated = pool_strdup(pool, deps.resource);
        if (g_env.check.log && r->n) {
          char buf[PATH_MAX];
          utils_strncpy(buf, prev_outdated, sizeof(buf));
          xstr_printf(g_env.check.log, "%s: outdated %s t=%c f=%c\n",
                      r->n->name, path_basename(buf), deps.type, deps.flags);
        }
        ulist_push(&r->resolve_outdated, &(struct resolve_outdated) {
          .type = deps.type,
          .flags = deps.flags,
          .path = prev_outdated
        });
      }
    }
  }

  if (deps.file) {
    deps_close(&deps);
  }

  bool env_created = false;
  if (r->on_resolve && (r->num_deps == 0 || r->resolve_outdated.num)) {
    if (g_env.check.log && r->n) {
      xstr_printf(g_env.check.log, "%s: resolved outdated outdated=%d\n", r->n->name, r->resolve_outdated.num);
    }
    r->on_resolve(r);
    if (access(deps_path_tmp, F_OK) == 0) {
      rc = utils_rename_file(deps_path_tmp, deps_path);
      if (rc) {
        akfatal(rc, "Rename failed of %s to %s", deps_path_tmp, deps_path);
      }
    }
    if (r->on_env_value && !access(env_path_tmp, F_OK)) {
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

  ulist_destroy_keep(&r->resolve_outdated);
  ulist_destroy_keep(&r->node_val_deps);
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
