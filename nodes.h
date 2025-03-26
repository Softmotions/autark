#ifndef NODES_H
#define NODES_H

#include "project.h"

int node_script_setup(struct node*);
int node_meta_setup(struct node*);
int node_consumes_setup(struct node*);
int node_check_setup(struct node*);
int node_sources_setup(struct node*);
int node_flags_setup(struct node*);
int node_exec_setup(struct node*);
int node_static_setup(struct node*);
int node_shared_setup(struct node*);
int node_include_setup(struct node*);
int node_if_setup(struct node*);

#endif
