#ifndef HYPERBOLA_TYPES_H
#define HYPERBOLA_TYPES_H 1

#include <gtk/gtkwidget.h>
#include <stdio.h>

/* Be a URL, for now */
typedef const char * HyperbolaLocationReference;

typedef struct _HyperbolaTreeNode HyperbolaTreeNode;

typedef enum {
  HYP_TREE_NODE_FOLDER,
  HYP_TREE_NODE_SECTION,
  HYP_TREE_NODE_BOOK,
  HYP_TREE_NODE_PAGE
} HyperbolaTreeNodeType;

struct _HyperbolaTreeNode {
  HyperbolaTreeNode *up;

  HyperbolaTreeNodeType type;

  char *title;
  char *uri;

  GTree *children; /* By title (relative to parent) */

  gpointer user_data;
};

typedef struct {
  GTree *children; /* By title */

  GHashTable *global_by_uri;

  gpointer user_data;
} HyperbolaDocTree;

#ifndef _
#define _(x) x
#define N_(x) x
#endif

#endif
