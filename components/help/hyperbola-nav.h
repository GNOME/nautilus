#ifndef HYPERBOLA_NAV_H
#define HYPERBOLA_NAV_H 1

#include "hyperbola-types.h"


/* In hyperbola-nav-tree.c */
BonoboObject *hyperbola_navigation_tree_new (void);
/* in hyperbola-nav-index.c */
BonoboObject *hyperbola_navigation_index_new( void );
/* in hyperbola-nav-search.c */
BonoboObject *hyperbola_navigation_search_new (void);

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
GtkWidget *make_index_page(HyperbolaNavigationIndex *index);
void hyperbola_navigation_index_clist_select_row (GtkWidget * clist, gint row,
                                       gint column, GdkEvent * event,
                                       HyperbolaNavigationIndex * hni);
void hyperbola_navigation_notebook_page_changed (GtkNotebook *notebook,
                                   GtkNotebookPage *page,
                                   gint page_num,
                                   HyperbolaNavigationIndex *index);
void hyperbola_navigation_tree_destroy (GtkCTree * ctree,
                                        HyperbolaNavigationTree *view);
void hyperbola_navigation_tree_select_row ( GtkCTree * ctree,
                                            GtkCTreeNode * node,
                                            gint column,
                                            HyperbolaNavigationTree *view);

void
get_node_icons (HyperbolaNavigationTree *view,
                HyperbolaTreeNode *node,
                GdkPixmap **pixmap_closed,
                GdkBitmap **mask_closed,
                GdkPixmap **pixmap_opened,
                GdkBitmap **mask_opened);
#endif
#endif

