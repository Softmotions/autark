#ifndef NODES_H
#define NODES_H

#include "script.h"

int node_script_setup(struct node*);
int node_meta_setup(struct node*);
int node_check_setup(struct node*);
int node_set_setup(struct node*);
int node_include_setup(struct node*);
int node_if_setup(struct node*);
int node_subst_setup(struct node*);
int node_run_setup(struct node*);
int node_echo_setup(struct node*);
int node_join_setup(struct node *n);
int node_cc_setup(struct node *n);

#endif
