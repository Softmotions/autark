#ifndef NODES_H
#define NODES_H

#ifndef _AMALGAMATE_
#include "script.h"
#endif

int node_script_setup(struct node*);
int node_meta_setup(struct node*);
int node_check_setup(struct node*);
int node_set_setup(struct node*);
int node_include_setup(struct node*);
int node_if_setup(struct node*);
int node_subst_setup(struct node*);
int node_run_setup(struct node*);
int node_echo_setup(struct node*);
int node_join_setup(struct node*);
int node_cc_setup(struct node*);
int node_configure_setup(struct node*);
int node_basename_setup(struct node*);
int node_foreach_setup(struct node*);
int node_in_sources_setup(struct node*);
int node_dir_setup(struct node*);
int node_option_setup(struct node*);
int node_error_setup(struct node*);
int node_echo_setup(struct node*);
int node_install_setup(struct node*);
int node_find_setup(struct node*);

#endif
