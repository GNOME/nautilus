#include <config.h>
#include <libnautilus/libnautilus.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <libnautilus-extensions/nautilus-ctree.h>

#include "hyperbola-nav.h"

#include <ctype.h>

typedef struct {
  NautilusView *view_frame;

  GtkWidget *ctree;
  HyperbolaDocTree *doc_tree;

  char *pending_location;

  gint notify_count;
} HyperbolaNavigationTree;

static void hyperbola_navigation_tree_destroy(GtkCTree *ctree,
					      HyperbolaNavigationTree *view);
static void hyperbola_navigation_tree_select_row(GtkCTree *ctree,
						 GtkCTreeNode *node,
						 gint column,
						 HyperbolaNavigationTree *view);
static void hyperbola_navigation_tree_load_location (NautilusView *view_frame,
						     const char *location_uri,
						     HyperbolaNavigationTree *hview);

typedef struct {
  HyperbolaNavigationTree *view;
  GtkCTreeNode *sibling, *parent;
} PopulateInfo;

static gboolean
ctree_populate_subnode(gpointer key, gpointer value, gpointer user_data)
{
  HyperbolaTreeNode *node = value;
  PopulateInfo *pi = (PopulateInfo *)user_data, subpi;
  gboolean term;
  char *title;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
  /* Get rid of leading numbers used to make sure TOCs are displayed properly */
  if (strstr(node->title, ". ") != NULL)
    {
      char **split;
      
      split = g_strsplit(node->title, ". ", 1);
      title = g_strdup(split[1]);

      /* Clean up the strings a bit.  These modify the string in-place */
      g_strchug(title);
      g_strchomp(title);
      
      g_strfreev(split);
    }
  else 
#endif
    title = node->title;

  term = (node->type == HYP_TREE_NODE_PAGE) || !node->children;
  pi->sibling = gtk_ctree_insert_node(GTK_CTREE(pi->view->ctree), pi->parent, NULL, &title, 5,
				      NULL, NULL, NULL, NULL, term, FALSE);
  node->user_data = pi->sibling;
  
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
  if (title != node->title)   
    g_free(title);            /* We used the copy from the split */
#endif
  
  gtk_ctree_node_set_row_data(GTK_CTREE(pi->view->ctree), pi->sibling, node);

  if(node->children)
    {
      subpi.view = pi->view;
      subpi.sibling = NULL;
      subpi.parent = pi->sibling;
      g_tree_traverse(node->children, ctree_populate_subnode, G_IN_ORDER, &subpi);
    }

  return FALSE;
}

static void
ctree_populate(HyperbolaNavigationTree *view)
{
  PopulateInfo subpi = { NULL, NULL, NULL };

  subpi.view = view;

  g_tree_traverse(view->doc_tree->children, ctree_populate_subnode, G_IN_ORDER, &subpi);
}

BonoboObject *
hyperbola_navigation_tree_new(void)
{
  GtkWidget *wtmp;
  HyperbolaNavigationTree *view;
  
  view = g_new0 (HyperbolaNavigationTree, 1);

  view->ctree = gtk_ctree_new (1, 0);

  gtk_ctree_set_line_style (GTK_CTREE (view->ctree), GTK_CTREE_LINES_NONE);
  gtk_ctree_set_expander_style (GTK_CTREE (view->ctree), GTK_CTREE_EXPANDER_TRIANGLE);

  gtk_clist_freeze (GTK_CLIST(view->ctree));
  gtk_clist_set_selection_mode (GTK_CLIST(view->ctree), GTK_SELECTION_BROWSE);
  gtk_signal_connect (GTK_OBJECT(view->ctree), "tree_select_row", hyperbola_navigation_tree_select_row, view);
  gtk_signal_connect (GTK_OBJECT(view->ctree), "destroy", hyperbola_navigation_tree_destroy, view);
  view->doc_tree = hyperbola_doc_tree_new ();
  hyperbola_doc_tree_populate (view->doc_tree);

  ctree_populate (view);

  wtmp = gtk_scrolled_window_new (gtk_clist_get_hadjustment (GTK_CLIST (view->ctree)),
				  gtk_clist_get_vadjustment (GTK_CLIST (view->ctree)));
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wtmp), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);


  gtk_container_add (GTK_CONTAINER (wtmp), view->ctree);
  gtk_clist_columns_autosize (GTK_CLIST (view->ctree));
  gtk_clist_thaw (GTK_CLIST (view->ctree));
  gtk_widget_show (view->ctree);
  gtk_widget_show (wtmp);

  view->view_frame = nautilus_view_new (wtmp);
  gtk_signal_connect (GTK_OBJECT (view->view_frame), "load_location", 
		      hyperbola_navigation_tree_load_location,
		      view);

  return BONOBO_OBJECT (view->view_frame);
}

static void
set_pending_location (HyperbolaNavigationTree *view, const char *location)
{
	g_free (view->pending_location);
	view->pending_location = g_strdup (location);
}

static void
hyperbola_navigation_tree_load_location (NautilusView *view_frame,
					 const char *location_uri,
					 HyperbolaNavigationTree *hview)
{
  HyperbolaTreeNode *tnode;

  set_pending_location (hview, NULL);

  if(hview->notify_count > 0)
    return;

  hview->notify_count++;

  tnode = g_hash_table_lookup(hview->doc_tree->global_by_uri, location_uri);

  if(tnode)
    gtk_ctree_select(GTK_CTREE(hview->ctree), tnode->user_data);

  hview->notify_count--;
}

static void hyperbola_navigation_tree_select_row(GtkCTree *ctree, GtkCTreeNode *node,
						 gint column, HyperbolaNavigationTree *view)
{
  HyperbolaTreeNode *tnode;

  if(view->notify_count > 0)
    return;

  tnode = gtk_ctree_node_get_row_data(ctree, node);

  if(!tnode || !tnode->uri)
    return;

  /* Don't start another load of the location you just started a load on. This
   * could happen on double-click and causes trouble in the Nautilus window/view 
   * state machine. See bug 5197.
   */
  if (view->pending_location != NULL && strcmp (view->pending_location, tnode->uri) == 0)
    return;

  view->notify_count++;

  set_pending_location (view, tnode->uri);
  nautilus_view_open_location_in_this_window (view->view_frame, tnode->uri);

  view->notify_count--;
}

static void hyperbola_navigation_tree_destroy (GtkCTree *ctree, HyperbolaNavigationTree *view)
{
  set_pending_location (view, NULL);
}
