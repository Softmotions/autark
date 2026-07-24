#ifndef _AMALGAMATE_
#include "script.h"
#endif

static void _source_distr_init(struct node *n) {
}

static void _source_distr_setup(struct node *n) {
}

static void _source_distr_build(struct node *n) {
}


//
//  A
//  | autark-cache
//               | .overlay
//                        | extern_iowow
//                                     | autark-cache
//                                                  | extern_mylib
//               extern_iowow
//                          | autark-cache
//                                       | extern_mylub
//

static void _source_distr_post_build(struct node *n) {
}

static void _source_distr_dispose(struct node *n) {
}

int node_source_distr_setup(struct node *n) {
  n->init = _source_distr_init;
  n->setup = _source_distr_setup;
  n->build = _source_distr_build;
  n->post_build = _source_distr_post_build;
  n->dispose = _source_distr_dispose;
  return 0;
}

