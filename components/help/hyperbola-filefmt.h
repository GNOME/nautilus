#ifndef HYPERBOLA_FILEFMT_H
#define HYPERBOLA_FILEFMT_H 1

#include "hyperbola-types.h"

void hyperbola_doc_tree_populate (HyperbolaDocTree * tree);
int hyperbola_top_doc_tree_populate (HyperbolaDocTree * tree);
HyperbolaDocTree *hyperbola_doc_tree_new (void);
void hyperbola_doc_tree_destroy (HyperbolaDocTree * tree);
void hyperbola_doc_tree_add (HyperbolaDocTree * tree,
			     HyperbolaTreeNodeType type, const char **path,
			     const char *title, const char *uri);

#endif
