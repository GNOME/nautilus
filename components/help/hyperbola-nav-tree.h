#ifndef HYPERBOLA_NAV_TREE_H
#define HYPERBOLA_NAV_TREE_H 1

#include <libnautilus/

#define HYPERBOLA_TYPE_NAVIGATION_TREE (hyperbola_navigation_tree_get_type())
#define HYPERBOLA_NAVIGATION_TREE(obj)	        (GTK_CHECK_CAST ((obj), HYPERBOLA_TYPE_NAVIGATION_TREE, HyperbolaNavigationTree))
#define HYPERBOLA_NAVIGATION_TREE_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), HYPERBOLA_TYPE_NAVIGATION_TREE, HyperbolaNavigationTreeClass))
#define HYPERBOLA_IS_NAVIGATION_TREE(obj)	        (GTK_CHECK_TYPE ((obj), HYPERBOLA_TYPE_NAVIGATION_TREE))
#define HYPERBOLA_IS_NAVIGATION_TREE_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), HYPERBOLA_TYPE_NAVIGATION_TREE))

typedef struct _HyperbolaNavigationTree HyperbolaNavigationTree;

HyperbolaDocTree *hyperbola_doc_tree_new(void);
void hyperbola_doc_tree_destroy(HyperbolaDocTree *tree);
void hyperbola_doc_tree_add(HyperbolaDocTree *tree, HyperbolaTreeNodeType type, const char **path,
			    const char *rel_title, const char *url);

typedef struct {
  HyperbolaNavigationViewClass parent_spot;

  HyperbolaNavigationViewClass *parent_class;
} HyperbolaNavigationTreeClass;

struct _HyperbolaNavigationTree {
  HyperbolaNavigationView parent_object;

  GtkWidget *ctree;
  HyperbolaDocTree *doc_tree;
  guint notify_count;
};

GtkType hyperbola_navigation_tree_get_type(void);
HyperbolaNavigationTree *hyperbola_navigation_tree_new(void);

#endif
