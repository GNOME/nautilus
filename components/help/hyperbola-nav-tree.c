#ifdef ENABLE_SCROLLKEEPER_SUPPORT
#include "hyperbola-types.h"
#else
#include <gtk/gtk.h>
#include <gtk/gtkctree.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus/libnautilus.h>
#endif

#include "hyperbola-filefmt.h"
#include "hyperbola-nav.h"
#include <ctype.h>

#ifndef ENABLE_SCROLLKEEPER_SUPPORT
typedef struct {
        NautilusView *view_frame;

        GtkWidget *ctree;
        HyperbolaDocTree *doc_tree;
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

typedef struct {
        HyperbolaNavigationTree *view;
        GtkWidget *ctree;
        GtkCTreeNode *sibling, *parent;
} PopulateInfo;

static void hyperbola_navigation_tree_destroy (GtkCTree * ctree,
                                               HyperbolaNavigationTree *
                                               view);
static void hyperbola_navigation_tree_select_row (GtkCTree * ctree,
                                                  GtkCTreeNode * node,
                                                  gint column,
                                                  HyperbolaNavigationTree *
                                                  view);
#endif
static void hyperbola_navigation_tree_load_location (NautilusView *
						     view_frame,
						     const char *location_uri,
						     HyperbolaNavigationTree *
						     hview);
#ifdef ENABLE_SCROLLKEEPER_SUPPORT
void set_pending_location (HyperbolaNavigationTree * view, const char *location);
#else
static void set_pending_location (HyperbolaNavigationTree * view, const char *location);
#endif
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

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
void
#else
static void
#endif
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

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
/*
 * make_contents_page
 * @HyperbolaNavigationTree *contents: pointer to the contents page's
 *                                     HyperbolaNavigationTree
 * Creates the widget that will be appended to the GtkNotebook as the
 * contents page.
 * Returns a pointer to the widget. 
 */

static GtkWidget *
make_contents_page(HyperbolaNavigationTree *contents)
{
	GtkWidget *wtmp1, *wtmp2;
	GtkWidget *vbox;
	GtkWidget *label;
	int top_tree_empty;

	contents->top_ctree = gtk_ctree_new(1, 0);
	gtk_container_set_border_width (GTK_CONTAINER (contents->top_ctree), 0);
	gtk_ctree_set_line_style (GTK_CTREE (contents->top_ctree),
				  GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style (GTK_CTREE (contents->top_ctree),
				      GTK_CTREE_EXPANDER_TRIANGLE);
				      
	gtk_clist_freeze (GTK_CLIST (contents->top_ctree));
	gtk_clist_set_selection_mode (GTK_CLIST (contents->top_ctree),
				      GTK_SELECTION_BROWSE);
	gtk_signal_connect (GTK_OBJECT (contents->top_ctree), "tree_select_row",
			    hyperbola_navigation_tree_select_row, contents);
	gtk_signal_connect (GTK_OBJECT (contents->top_ctree), "destroy",
			    hyperbola_navigation_tree_destroy, contents);

	contents->ctree = gtk_ctree_new (1, 0);
	gtk_ctree_set_line_style (GTK_CTREE (contents->ctree),
				  GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style (GTK_CTREE (contents->ctree),
				      GTK_CTREE_EXPANDER_TRIANGLE);

	gtk_clist_freeze (GTK_CLIST (contents->ctree));
	gtk_clist_set_selection_mode (GTK_CLIST (contents->ctree),
				      GTK_SELECTION_BROWSE);
	gtk_signal_connect (GTK_OBJECT (contents->ctree), "tree_select_row",
			    hyperbola_navigation_tree_select_row, contents);
	gtk_signal_connect (GTK_OBJECT (contents->ctree), "destroy",
			    hyperbola_navigation_tree_destroy, contents);

	contents->selected_ctree = NULL;
	contents->selected_node = NULL;
	contents->select = 0;
			    
	contents->doc_tree = hyperbola_doc_tree_new ();
	contents->doc_tree->contents_tree_type = TRUE;
	hyperbola_doc_tree_populate (contents->doc_tree);
	contents->top_doc_tree = hyperbola_doc_tree_new ();
	top_tree_empty = !hyperbola_top_doc_tree_populate(contents->top_doc_tree);

	ctree_populate (contents);	

	wtmp1 = gtk_scrolled_window_new (gtk_clist_get_hadjustment
					 (GTK_CLIST (contents->ctree)),
					 gtk_clist_get_vadjustment (GTK_CLIST (contents-> ctree)));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wtmp1),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (wtmp1);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	
	if (!top_tree_empty) {
		label = gtk_label_new (_("Introductory Documents"));
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_misc_set_padding (GTK_MISC (label), GNOME_PAD_SMALL, 0);
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
		wtmp2 = gtk_scrolled_window_new (gtk_clist_get_hadjustment
					 (GTK_CLIST (contents->top_ctree)),
					 gtk_clist_get_vadjustment(GTK_CLIST(contents->top_ctree)));
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wtmp2),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_widget_show(contents->top_ctree);
		gtk_widget_show (wtmp2);
			
		gtk_box_pack_start ( GTK_BOX(vbox), wtmp2, TRUE, TRUE, 0);
		gtk_container_add (GTK_CONTAINER (wtmp2), contents->top_ctree);
	}
	else {
		gtk_widget_hide (contents->top_ctree);
	}
	
	
	label = gtk_label_new (_("Documents by Subject"));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), GNOME_PAD_SMALL, 0);
	gtk_widget_show (label);
	gtk_box_pack_start ( GTK_BOX(vbox), label, FALSE, FALSE, 0);

	gtk_widget_show (contents->ctree);
	gtk_box_pack_start ( GTK_BOX(vbox), wtmp1, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (wtmp1), contents->ctree);

	gtk_clist_set_column_auto_resize (GTK_CLIST (contents->top_ctree),
					  0 /* column */,
					  TRUE /* auto_resize */);
	gtk_clist_thaw (GTK_CLIST (contents->top_ctree));
	gtk_clist_set_column_auto_resize (GTK_CLIST (contents->ctree),
					  0 /* column */,
					  TRUE /* auto_resize */);
	gtk_clist_thaw (GTK_CLIST (contents->ctree));
	return vbox;
}

/* Callback functions for keyboard acceleration */
static void
switch_notebook_cb(BonoboUIComponent *com, gpointer data, const char *verb) {

	HyperbolaNavigationIndex *indx = (HyperbolaNavigationIndex *)data;
	
	if(!strcmp("Switch Contents", verb))	
		gtk_notebook_set_page(GTK_NOTEBOOK(indx->notebook),0 );	
	else
		gtk_notebook_set_page(GTK_NOTEBOOK(indx->notebook),1 );	
}

static void
focus_scrolled_window_cb(BonoboUIComponent *com, gpointer data,const char *verb)
{

	HyperbolaNavigationIndex *indx = (HyperbolaNavigationIndex *)data;

	if(strcmp("Select", verb)) {	
		gtk_clist_select_row(GTK_CLIST(indx->index_contents->ctree), 0,0);
		gtk_widget_grab_focus(GTK_WIDGET(indx->index_contents->ctree));
	}
	else {
		gtk_clist_select_row(GTK_CLIST(indx->clist), 0,0);
		gtk_widget_grab_focus(GTK_WIDGET(indx->clist));
		
	}	
					
}

static void 
button_selected_cb(BonoboUIComponent *com, gpointer data, const char *verb){
		
	HyperbolaNavigationIndex *indx = (HyperbolaNavigationIndex *)data;
	
	if(!strcmp("All", verb))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
								indx->all_rbutton), TRUE);
	else if(!strcmp("Contents Tab", verb))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
								indx->contents_rbutton), TRUE);
	else if(!strcmp("Specific",verb))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
								indx->specific_rbutton), TRUE);
	else if(!strcmp("Show All Terms", verb))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
								indx->all_terms_rbutton), TRUE);
	else if(!strcmp("Show Specific Terms", verb))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
								indx->specific_terms_rbutton), TRUE);
	else if(!strcmp("Show", verb))
		gtk_button_pressed(GTK_BUTTON(indx->show_button));
	else if(!strcmp("Select All", verb))
		gtk_button_pressed(GTK_BUTTON(indx->all_button));
	else if(!strcmp("Select None", verb))
		gtk_button_pressed(GTK_BUTTON(indx->none_button));
	
}
/* Callback to setup keyboard accelerator for hyperbola view when activated
 */

static void
merge_items_callback(BonoboControl *control, gboolean state, gpointer user_data)
{
	NautilusView *view;
	HyperbolaNavigationIndex *indx = (HyperbolaNavigationIndex *) user_data;
	BonoboUIComponent *ui;
	
	BonoboUIVerb verbs[] = {
		BONOBO_UI_VERB("Switch Contents", switch_notebook_cb),
		BONOBO_UI_VERB("Switch Index", switch_notebook_cb),
		BONOBO_UI_VERB("Contents Tab", button_selected_cb),
		BONOBO_UI_VERB("All", button_selected_cb),
		BONOBO_UI_VERB("Specific", button_selected_cb),
		BONOBO_UI_VERB("Show All Terms", button_selected_cb),
		BONOBO_UI_VERB("Show Specific Terms", button_selected_cb),
		BONOBO_UI_VERB("Show", button_selected_cb),
		BONOBO_UI_VERB("Index", focus_scrolled_window_cb),
		BONOBO_UI_VERB("Select", focus_scrolled_window_cb),
		BONOBO_UI_VERB("Select All", button_selected_cb),
		BONOBO_UI_VERB("Select None", button_selected_cb),
		BONOBO_UI_VERB_END
	};

	g_assert(BONOBO_IS_CONTROL(control));
	view = NAUTILUS_VIEW(indx->index_view_frame);

	if (state) {
		ui = nautilus_view_set_up_ui(view, HYPERBOLA_DATADIR, "nautilus-hyperbola-ui.xml", "hyperbola"); 

		bonobo_ui_component_add_verb_list_with_data(ui, verbs, indx);
	}

}

/*
 * hyperbola_navigation_tree_new.
 * Creates a GtkNotebook with two pages:
 * - contents page displays the list of contents;
 * - index page will display alternatively the 
 *   index_display_page or the index_select page.
 *   The index_display page displays an index, the
 *   index_select_page display a contents list to 
 *   allow the user to select documents whose indexes
 *   can then be displayed on the index_display_page.
 * Function then creates a BonoboObject from the notebook
 * Returns the BonoboObject pointer.
 */
BonoboObject *
hyperbola_navigation_tree_new (void)
{
	NautilusView *view_frame;
	GtkWidget *notebook, *index_page, *page_label, *contents_page;
	HyperbolaNavigationTree *contents;
	HyperbolaNavigationIndex *index;

	notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook),
                                  GTK_POS_TOP);
	
	contents = g_new0 (HyperbolaNavigationTree, 1);
	index = g_new0 (HyperbolaNavigationIndex, 1);
	
	/*
	 * the index page requires a HyperbolaNavigationTree that
	 * will be displayed on the index_select_page
	 */
	index->index_contents = g_new0 (HyperbolaNavigationTree, 1);

	contents_page = make_contents_page(contents);
	page_label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(page_label), _("_Contents"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), contents_page, page_label);
	
	
	index->notebook = notebook;
	
	/*
	 * The index display will need access to the contents page's
	 * ctree and top_ctree as the user is allowed to select documents
	 * here whose indexes will be displayed on the index page
	 */

	index->contents_ctree = contents->ctree;
	index->contents_top_ctree = contents->top_ctree;
	
	index_page = make_index_page(index);
	page_label = gtk_label_new("");
	gtk_label_parse_uline(GTK_LABEL(page_label), _("_Index"));
	
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), index_page, page_label);
	gtk_signal_connect(GTK_OBJECT(notebook), "switch_page",
				hyperbola_navigation_notebook_page_changed, index);
	gtk_widget_show (notebook);
	gtk_signal_connect (GTK_OBJECT (index->clist), "select_row",
                            hyperbola_navigation_index_clist_select_row, index);

	view_frame = nautilus_view_new (notebook);
	
	index->index_view_frame = view_frame;	
	contents->view_frame = view_frame;
	
	gtk_signal_connect(GTK_OBJECT(nautilus_view_get_bonobo_control(
									NAUTILUS_VIEW(view_frame))), "activate",
				   					merge_items_callback, index);
	gtk_signal_connect (GTK_OBJECT (contents->view_frame), "load_location",
			    hyperbola_navigation_tree_load_location, contents);

	return BONOBO_OBJECT (view_frame);

}
#else
BonoboObject *
hyperbola_navigation_tree_new (void)
{
        GtkWidget *wtmp;
        HyperbolaNavigationTree *view;

        view = g_new0 (HyperbolaNavigationTree, 1);

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


        view->doc_tree = hyperbola_doc_tree_new ();
        hyperbola_doc_tree_populate (view->doc_tree);

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

        gtk_widget_show (view->ctree);

        gtk_container_add (GTK_CONTAINER (wtmp), view->ctree);

        gtk_clist_set_column_auto_resize (GTK_CLIST (view->ctree),
                                          0 /* column */,
                                          TRUE /* auto_resize */);
        gtk_clist_thaw (GTK_CLIST (view->ctree));

        view->view_frame = nautilus_view_new (wtmp);

        gtk_signal_connect (GTK_OBJECT (view->view_frame), "load_location",
                            hyperbola_navigation_tree_load_location, view);

        return BONOBO_OBJECT (view->view_frame);
}
#endif

#ifdef ENABLE_SCROLLKEEPER_SUPPORT
void
#else
static void
#endif
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
#ifndef ENABLE_SCROLLKEEPER_SUPPORT
static void
#else
void
#endif
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

void
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
