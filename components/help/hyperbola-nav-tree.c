#include <config.h>

#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <gtk/gtkctree.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus/libnautilus.h>

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

	GdkPixmap *folder_open_pixmap;
	GdkBitmap *folder_open_mask;
	GdkPixmap *folder_closed_pixmap;
	GdkBitmap *folder_closed_mask;

	GdkPixmap *book_open_pixmap;
	GdkBitmap *book_open_mask;
	GdkPixmap *book_closed_pixmap;
	GdkBitmap *book_closed_mask;

	GdkPixmap *section_open_pixmap;
	GdkBitmap *section_open_mask;
	GdkPixmap *section_closed_pixmap;
	GdkBitmap *section_closed_mask;
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
ensure_pixmap_and_mask (GdkPixmap **pixmap, GdkBitmap **mask, const char *name)
{
	char *fullname;
	GdkPixbuf *pixbuf;

	if (*pixmap != NULL) {
		return;
	}

	fullname = nautilus_theme_get_image_path (name);
	if (fullname == NULL) {
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (fullname);
	if (pixbuf == NULL) {
		return;
	}

	gdk_pixbuf_render_pixmap_and_mask (pixbuf,
					   pixmap,
					   mask,
					   127 /* alpha_threshold */);

	g_free (fullname);
}

static void
ensure_icons (HyperbolaNavigationTree *view)
{
	ensure_pixmap_and_mask (&view->folder_open_pixmap,
				&view->folder_open_mask,
				"hyperbola-folder-open.png");
	ensure_pixmap_and_mask (&view->folder_closed_pixmap,
				&view->folder_closed_mask,
				"hyperbola-folder-closed.png");

	ensure_pixmap_and_mask (&view->book_open_pixmap,
				&view->book_open_mask,
				"hyperbola-book-open.png");
	ensure_pixmap_and_mask (&view->book_closed_pixmap,
				&view->book_closed_mask,
				"hyperbola-book-closed.png");

	ensure_pixmap_and_mask (&view->section_open_pixmap,
				&view->section_open_mask,
				"hyperbola-section-open.png");
	ensure_pixmap_and_mask (&view->section_closed_pixmap,
				&view->section_closed_mask,
				"hyperbola-section-closed.png");
}

static void
get_node_icons (HyperbolaNavigationTree *view,
		HyperbolaTreeNode *node,
		GdkPixmap **pixmap_closed,
		GdkBitmap **mask_closed,
		GdkPixmap **pixmap_opened,
		GdkBitmap **mask_opened)
{
	ensure_icons (view);

	if (node->type == HYP_TREE_NODE_FOLDER) {
		*pixmap_opened = view->folder_open_pixmap;
		*mask_opened = view->folder_open_mask;
		*pixmap_closed = view->folder_closed_pixmap;
		*mask_closed = view->folder_closed_mask;
	} else if (node->type == HYP_TREE_NODE_BOOK) {
		*pixmap_opened = view->book_open_pixmap;
		*mask_opened = view->book_open_mask;
		*pixmap_closed = view->book_closed_pixmap;
		*mask_closed = view->book_closed_mask;
	} else if (node->type == HYP_TREE_NODE_SECTION
		   || node->type == HYP_TREE_NODE_PAGE) {
		*pixmap_opened = view->section_open_pixmap;
		*mask_opened = view->section_open_mask;
		*pixmap_closed = view->section_closed_pixmap;
		*mask_closed = view->section_closed_mask;
	}
}

static gboolean
ctree_populate_subnode (gpointer key, gpointer value, gpointer user_data)
{
	HyperbolaTreeNode *node = value;
	PopulateInfo *pi = (PopulateInfo *) user_data, subpi;
	gboolean term;
	char *title;
	GdkPixmap *pixmap_closed = NULL;
	GdkBitmap *mask_closed = NULL;
	GdkPixmap *pixmap_opened = NULL;
	GdkBitmap *mask_opened = NULL;

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

	get_node_icons (pi->view, node,
			&pixmap_closed, &mask_closed,
			&pixmap_opened, &mask_opened);

	term = (node->type == HYP_TREE_NODE_PAGE) || !node->children;
	pi->sibling =
		gtk_ctree_insert_node (GTK_CTREE (pi->ctree),
				       pi->parent, NULL, &title, 5, 
				       pixmap_closed, mask_closed,
				       pixmap_opened, mask_opened,
				       term, FALSE);
	node->user_data = pi->sibling;

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
	if (title != node->title)
		g_free (title);	/* We used the copy from the split */
#endif

	gtk_ctree_node_set_row_data (GTK_CTREE (pi->ctree), pi->sibling,
				     node);

	if (node->children) {
		subpi.view = pi->view;
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
        	label = gtk_label_new (_("Introductory Documents"));
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_misc_set_padding (GTK_MISC (label), GNOME_PAD_SMALL, 0);
        	gtk_widget_show (label);
        	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
			
		gtk_widget_show (view->top_ctree);
		
	}
	else {
		gtk_widget_hide (view->top_ctree);
	}
	
	gtk_box_pack_start ( GTK_BOX(box), view->top_ctree, FALSE, FALSE, 0);
	
	label = gtk_label_new (_("Documents by Subject"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), GNOME_PAD_SMALL, 0);
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
pixmap_unref_and_null (GdkPixmap **pixmap)
{
	if (*pixmap != NULL) {
		gdk_pixmap_unref (*pixmap);
		*pixmap = NULL;
	}
}

static void
bitmap_unref_and_null (GdkPixmap **bitmap)
{
	if (*bitmap != NULL) {
		gdk_bitmap_unref (*bitmap);
		*bitmap = NULL;
	}
}

static void
hyperbola_navigation_tree_destroy (GtkCTree * ctree,
				   HyperbolaNavigationTree * view)
{
	set_pending_location (view, NULL);

	pixmap_unref_and_null (&view->folder_open_pixmap);
	bitmap_unref_and_null (&view->folder_open_mask);
	pixmap_unref_and_null (&view->folder_closed_pixmap);
	bitmap_unref_and_null (&view->folder_closed_mask);

	pixmap_unref_and_null (&view->book_open_pixmap);
	bitmap_unref_and_null (&view->book_open_mask);
	pixmap_unref_and_null (&view->book_closed_pixmap);
	bitmap_unref_and_null (&view->book_closed_mask);

	pixmap_unref_and_null (&view->section_open_pixmap);
	bitmap_unref_and_null (&view->section_open_mask);
	pixmap_unref_and_null (&view->section_closed_pixmap);
	bitmap_unref_and_null (&view->section_closed_mask);
}
