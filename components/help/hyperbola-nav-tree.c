#include <config.h>
#include <libnautilus/libnautilus.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libnautilus-extensions/nautilus-ctree.h>
#include <libnautilus-extensions/nautilus-gdk-font-extensions.h>

#include "hyperbola-nav.h"

#include <ctype.h>

typedef struct {
	NautilusView *view_frame;

	GtkWidget *ctree;
	HyperbolaDocTree *doc_tree;
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	GtkWidget *top_ctree;
	HyperbolaDocTree *top_doc_tree;
	GtkWidget *selected_ctree;
	GtkCTreeNode *selected_node;
	int select;
#endif
	char *pending_location;

	gint notify_count;

	GtkStyle *italic_style;
} HyperbolaNavigationTree;

static void hyperbola_navigation_tree_destroy (GtkCTree * ctree,
					       HyperbolaNavigationTree *
					       view);
static void hyperbola_navigation_tree_select_row (GtkCTree * ctree,
						  GtkCTreeNode * node,
						  gint column,
						  HyperbolaNavigationTree *
						  view);
static void hyperbola_navigation_tree_load_location (NautilusView *
						     view_frame,
						     const char *location_uri,
						     HyperbolaNavigationTree *
						     hview);

typedef struct {
	HyperbolaNavigationTree *view;
	GtkWidget *ctree;
	GtkCTreeNode *sibling, *parent;
} PopulateInfo;

static void
ensure_italic_style (HyperbolaNavigationTree *view)
{
	if (view->italic_style == NULL) {
		/* Set italic style for page names */
		GdkFont *font;
		GtkStyle *style = gtk_style_copy
			(gtk_widget_get_style (view->ctree));

		font = style->font;
		style->font = nautilus_gdk_font_get_italic (font);
		gdk_font_unref (font);

		view->italic_style = style;
	}
}

static void
set_node_style (HyperbolaNavigationTree *view,
		GtkCTree *ctree, GtkCTreeNode *node)
{
	HyperbolaTreeNode *tnode;

	g_assert (GTK_IS_CTREE (ctree));

	tnode = gtk_ctree_node_get_row_data (ctree, node);

	if (tnode != NULL
	    && (tnode->type == HYP_TREE_NODE_PAGE
		|| tnode->type == HYP_TREE_NODE_SECTION
		|| tnode->type == HYP_TREE_NODE_BOOK)) {
		ensure_italic_style (view);

		gtk_ctree_node_set_row_style (ctree, node, view->italic_style);
	} else {
		/* no special style */
		gtk_ctree_node_set_row_style (ctree, node, NULL);
	}
}

static gboolean
ctree_populate_subnode (gpointer key, gpointer value, gpointer user_data)
{
	HyperbolaTreeNode *node = value;
	PopulateInfo *pi = (PopulateInfo *) user_data, subpi;
	gboolean term;
	char *title;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	/* Get rid of leading numbers used to make sure TOCs are displayed properly */
	if (strstr (node->title, ". ") != NULL) {
		char **split;

		split = g_strsplit (node->title, ". ", 1);
		title = g_strdup (split[1]);

		/* Clean up the strings a bit.  These modify the string in-place */
		g_strchug (title);
		g_strchomp (title);

		g_strfreev (split);
	} else
#endif
		title = node->title;

	term = (node->type == HYP_TREE_NODE_PAGE) || !node->children;
	pi->sibling =
		gtk_ctree_insert_node (GTK_CTREE (pi->ctree),
				       pi->parent, NULL, &title, 5, NULL,
				       NULL, NULL, NULL, term, FALSE);
	node->user_data = pi->sibling;

	set_node_style (pi->view, GTK_CTREE (pi->ctree), pi->sibling);
	

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	if (title != node->title)
		g_free (title);	/* We used the copy from the split */
#endif

	gtk_ctree_node_set_row_data (GTK_CTREE (pi->ctree), pi->sibling,
				     node);

	if (node->children) {
		subpi.ctree = pi->ctree;
		subpi.sibling = NULL;
		subpi.parent = pi->sibling;
		g_tree_traverse (node->children, ctree_populate_subnode,
				 G_IN_ORDER, &subpi);
	}

	return FALSE;
}

static void
ctree_populate (HyperbolaNavigationTree * view)
{
	PopulateInfo subpi;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	subpi.view = view;
	subpi.ctree = view->top_ctree;
	subpi.sibling = NULL;
	subpi.parent = NULL;

	g_tree_traverse (view->top_doc_tree->children, ctree_populate_subnode,
			 G_IN_ORDER, &subpi);
#endif
	
	subpi.view = view;
	subpi.ctree = view->ctree;
	subpi.sibling = NULL;
	subpi.parent = NULL;

	g_tree_traverse (view->doc_tree->children, ctree_populate_subnode,
			 G_IN_ORDER, &subpi);
}

static void
reset_style_for_node (GtkCTree *ctree, GtkCTreeNode *node, gpointer data)
{
	HyperbolaNavigationTree *view = data;

	set_node_style (view, ctree, node);
}

static void
reset_styles (GtkWidget *widget, GtkStyle *old_style, gpointer data)
{
	HyperbolaNavigationTree *view = data;

	g_assert (GTK_IS_CTREE (widget));

	if (view->italic_style != NULL) {
		gtk_style_unref (view->italic_style);
		view->italic_style = NULL;
	}

	gtk_clist_freeze (GTK_CLIST (widget));
	gtk_ctree_post_recursive (GTK_CTREE (widget),
				  NULL,
				  reset_style_for_node,
				  view);
	gtk_clist_thaw (GTK_CLIST (widget));
}

BonoboObject *
hyperbola_navigation_tree_new (void)
{
	GtkWidget *wtmp;
	HyperbolaNavigationTree *view;
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	GtkWidget *box, *label;
	int top_tree_empty;
#endif

	view = g_new0 (HyperbolaNavigationTree, 1);

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	view->top_ctree = gtk_ctree_new(1, 0);
	gtk_container_set_border_width (GTK_CONTAINER (view->top_ctree), 0);
	gtk_ctree_set_line_style (GTK_CTREE (view->top_ctree),
				  GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style (GTK_CTREE (view->top_ctree),
				      GTK_CTREE_EXPANDER_TRIANGLE);
				      
	gtk_clist_freeze (GTK_CLIST (view->top_ctree));
	gtk_clist_set_selection_mode (GTK_CLIST (view->top_ctree),
				      GTK_SELECTION_BROWSE);
	gtk_signal_connect (GTK_OBJECT (view->top_ctree), "tree_select_row",
			    hyperbola_navigation_tree_select_row, view);
	gtk_signal_connect (GTK_OBJECT (view->top_ctree), "destroy",
			    hyperbola_navigation_tree_destroy, view);
	gtk_signal_connect (GTK_OBJECT (view->top_ctree), "style_set",
			    reset_styles, view);
#endif

	view->ctree = gtk_ctree_new (1, 0);
	gtk_ctree_set_line_style (GTK_CTREE (view->ctree),
				  GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style (GTK_CTREE (view->ctree),
				      GTK_CTREE_EXPANDER_TRIANGLE);

	gtk_clist_freeze (GTK_CLIST (view->ctree));
	gtk_clist_set_selection_mode (GTK_CLIST (view->ctree),
				      GTK_SELECTION_BROWSE);
	gtk_signal_connect (GTK_OBJECT (view->ctree), "tree_select_row",
			    hyperbola_navigation_tree_select_row, view);
	gtk_signal_connect (GTK_OBJECT (view->ctree), "destroy",
			    hyperbola_navigation_tree_destroy, view);
	gtk_signal_connect (GTK_OBJECT (view->ctree), "style_set",
			    reset_styles, view);

	view->italic_style = NULL;
			    
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	view->selected_ctree = NULL;
	view->selected_node = NULL;
	view->select = 0;
#endif
			    
	view->doc_tree = hyperbola_doc_tree_new ();
	hyperbola_doc_tree_populate (view->doc_tree);
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	view->top_doc_tree = hyperbola_doc_tree_new ();
	top_tree_empty = !hyperbola_top_doc_tree_populate(view->top_doc_tree);
#endif

	ctree_populate (view);	

	wtmp =
		gtk_scrolled_window_new (gtk_clist_get_hadjustment
					 (GTK_CLIST (view->ctree)),
					 gtk_clist_get_vadjustment (GTK_CLIST
								    (view->
								     ctree)));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wtmp),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (wtmp);
		
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (box);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (wtmp), box);
	
	if (!top_tree_empty) {
        	label = gtk_label_new (_("Introductory Documents:"));
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        	gtk_widget_show (label);
        	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
			
		gtk_widget_show (view->top_ctree);
		
	}
	else {
		gtk_widget_hide (view->top_ctree);
	}
	
	gtk_box_pack_start ( GTK_BOX(box), view->top_ctree, FALSE, FALSE, 0);
	
	label = gtk_label_new (_("Documents by Subject:"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_widget_show (label);
	gtk_box_pack_start ( GTK_BOX(box), label, FALSE, FALSE, 0);
#endif 

	gtk_widget_show (view->ctree);
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	gtk_box_pack_start ( GTK_BOX(box), view->ctree, TRUE, TRUE, 0);
#else
	gtk_container_add (GTK_CONTAINER (wtmp), view->ctree);
#endif

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	gtk_clist_set_column_auto_resize (GTK_CLIST (view->top_ctree),
					  0 /* column */,
					  TRUE /* auto_resize */);
	gtk_clist_thaw (GTK_CLIST (view->top_ctree));
#endif
	gtk_clist_set_column_auto_resize (GTK_CLIST (view->ctree),
					  0 /* column */,
					  TRUE /* auto_resize */);
	gtk_clist_thaw (GTK_CLIST (view->ctree));
		
	view->view_frame = nautilus_view_new (wtmp);
	
	gtk_signal_connect (GTK_OBJECT (view->view_frame), "load_location",
			    hyperbola_navigation_tree_load_location, view);

	return BONOBO_OBJECT (view->view_frame);
}

static void
set_pending_location (HyperbolaNavigationTree * view, const char *location)
{
	g_free (view->pending_location);
	view->pending_location = g_strdup (location);
}


static void
hyperbola_navigation_tree_load_location (NautilusView * view_frame,
					 const char *location_uri,
					 HyperbolaNavigationTree * hview)
{
	HyperbolaTreeNode *tnode;
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	HyperbolaDocTree *doc_tree1, *doc_tree2;
	GtkWidget *ctree1, *ctree2;
#endif
	
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	if (hview->select) /* if we come here because of a mouse select then get out */
	{
		hview->select = 0;
		return;
	}
#endif

	set_pending_location (hview, NULL);

	if (hview->notify_count > 0)
		return;

	hview->notify_count++;
	
#ifdef ENABLE_SCROLLKEEPER_SUPPORT

	if (hview->selected_ctree == NULL) {
		ctree1 = hview->top_ctree;
		ctree2 = hview->ctree;
		doc_tree1 = hview->top_doc_tree;
		doc_tree2 = hview->doc_tree;
	}
	else {
		ctree1 = hview->selected_ctree;
		
		if (ctree1 == hview->top_ctree) {
			ctree2 = hview->ctree;
			doc_tree1 = hview->top_doc_tree;
			doc_tree2 = hview->doc_tree;
		}
		else {
			ctree2 = hview->top_ctree;
			doc_tree2 = hview->top_doc_tree;
			doc_tree1 = hview->doc_tree;
		}
	}

	if (hview->selected_ctree != NULL &&
	    hview->selected_node != NULL)
		gtk_ctree_unselect(GTK_CTREE (hview->selected_ctree), hview->selected_node);

	tnode =
		g_hash_table_lookup (doc_tree1->global_by_uri,
				  	location_uri);
	if (tnode != NULL) {
		gtk_ctree_select (GTK_CTREE (ctree1), tnode->user_data);
		hview->selected_ctree = ctree1;
		hview->selected_node = tnode->user_data;
	}
	else {
		tnode =
			g_hash_table_lookup (doc_tree2->global_by_uri,
				     		location_uri);
		if (tnode != NULL) {
			gtk_ctree_select (GTK_CTREE (ctree2), tnode->user_data);	
			hview->selected_ctree = ctree2;
			hview->selected_node = tnode->user_data;
		}
		else {
			hview->selected_ctree = NULL;
			hview->selected_node = NULL;
		}
	}
#else
	tnode =
		g_hash_table_lookup (hview->doc_tree->global_by_uri,
				     		location_uri);
						
	if (tnode != NULL)
		gtk_ctree_select (GTK_CTREE (hview->ctree), tnode->user_data);	

#endif

	hview->notify_count--;
}

static void
hyperbola_navigation_tree_select_row (GtkCTree * ctree, GtkCTreeNode * node,
				      gint column,
				      HyperbolaNavigationTree * view)
{
	HyperbolaTreeNode *tnode;

	if (view->notify_count > 0)
		return;

	tnode = gtk_ctree_node_get_row_data (ctree, node);

	if (!tnode || !tnode->uri)
		return;

	/* Don't start another load of the location you just started a load on. This
	 * could happen on double-click and causes trouble in the Nautilus window/view 
	 * state machine. See bug 5197.
	 */
	if (view->pending_location != NULL &&
	    strcmp (view->pending_location, tnode->uri) == 0)
		return;

	view->notify_count++;

	set_pending_location (view, tnode->uri);
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	if (view->selected_ctree != NULL &&
	    view->selected_node != NULL)
		gtk_ctree_unselect(GTK_CTREE (view->selected_ctree), view->selected_node);
	view->selected_ctree = GTK_WIDGET (ctree);
	view->selected_node = node;
	view->select = 1;
#endif
	nautilus_view_open_location_in_this_window (view->view_frame,
						    tnode->uri);

	view->notify_count--;
}

static void
hyperbola_navigation_tree_destroy (GtkCTree * ctree,
				   HyperbolaNavigationTree * view)
{
	set_pending_location (view, NULL);

	if (view->italic_style != NULL) {
		gtk_style_unref (view->italic_style);
		view->italic_style = NULL;
	}
}
