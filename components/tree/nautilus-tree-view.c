/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: 
 *       Maciej Stachowiak <mjs@eazel.com>
 *       Mathieu Lacage <mathieu@eazel.com> (dnd code)
 */

/* nautilus-tree-view.c - tree content view
   component. This component displays a simple label of the URI
   and demonstrates merging menu items & toolbar buttons. 
   It should be a good basis for writing out-of-proc content views.
 */

#include <config.h>
#include "nautilus-tree-view.h"

#include "nautilus-tree-model.h"
#include "nautilus-tree-expansion-state.h"

#include <bonobo/bonobo-control.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkdnd.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-drag.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-queue.h>
#include <libnautilus-extensions/nautilus-drag.h>
#include <libnautilus-extensions/nautilus-file-operations.h>
#include <libgnomevfs/gnome-vfs.h>


#include <stdio.h>

/* timeout for the automatic expand in tree view */
#define EXPAND_TIMEOUT 800


#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500

#define DND_DEBUG 1



typedef struct _NautilusTreeViewDndDetails NautilusTreeViewDndDetails;
struct _NautilusTreeViewDndDetails {

	NautilusDragInfo *drag_info;

	/* data setup by button_press signal for dragging */
	int press_x, press_y;
	gboolean pressed_hot_spot;
	gboolean drag_pending;
	guint pressed_button;

	GtkTargetList *target_list;

	/* data used by the drag_motion code */
	int current_x, current_y;
	/* row being highlighted */
	GtkCTreeNode *current_prelighted_node;
	GtkStyle *normal_style;
	GtkStyle *highlight_style;
	/* queue of motion event's y coordinates */
        NautilusQueue *motion_queue;
};


typedef void (*TreeViewCallback) (NautilusTreeView *view);


/* A NautilusContentView's private information. */
struct NautilusTreeViewDetails {
	NautilusView *nautilus_view;

	GtkWidget *tree;

	NautilusTreeModel *model;

	GHashTable *uri_to_node_map;
	GHashTable *uri_to_hack_node_map;

	gboolean show_hidden_files;

	NautilusTreeExpansionState *expansion_state;
	char *selected_uri;

	TreeViewCallback root_seen_callback;
	char *wait_uri;
	NautilusTreeNode *wait_node;
	TreeViewCallback uri_loaded_or_parent_done_loading;
        GList *in_progress_select_uris;
        gboolean root_seen;


	NautilusTreeViewDndDetails *dnd;
};


#define TREE_SPACING 5

static void         notify_done_loading    (NautilusTreeView *view, 
					    NautilusTreeNode *node);
static void         notify_node_seen       (NautilusTreeView *view, 
					    NautilusTreeNode *node);

static char         *nautilus_tree_view_uri_to_name      (const char       *uri);
static GtkCTreeNode *nautilus_tree_view_find_parent_node (NautilusTreeView *view, 
							  const char       *uri);

static gboolean      ctree_is_node_expanded              (GtkCTree     *ctree,
							  GtkCTreeNode *node);
static GtkCTreeNode *uri_to_view_node                    (NautilusTreeView *view,
							  const char *uri);
static GtkCTreeNode *model_node_to_view_node             (NautilusTreeView *view,
							  NautilusTreeNode *node);

static const char   *view_node_to_uri                    (NautilusTreeView *view,
							  GtkCTreeNode *node);


static void reload_node_for_uri                 (NautilusTreeView *view,
						 const char       *uri);
static void expand_node_for_uri                 (NautilusTreeView *view,
						 const char       *uri);
static void tree_load_location_callback         (NautilusView          *nautilus_view,
						 const char            *location,
						 NautilusTreeView      *view);
static void tree_expand_callback                (GtkCTree              *tree,
						 GtkCTreeNode          *node,
						 NautilusTreeView      *view);
static void tree_collapse_callback              (GtkCTree              *tree,
						 GtkCTreeNode          *node,
						 NautilusTreeView      *view);
static void tree_select_row_callback            (GtkCTree              *tree,
						 GtkCTreeNode          *node,
						 gint                   column,
						 NautilusTreeView      *view);
static void nautilus_tree_view_load_uri         (NautilusTreeView      *view,
						 const char            *uri);
static void nautilus_tree_view_update_all_icons (NautilusTreeView *view);


static void     nautilus_tree_view_drag_begin   (GtkWidget            *widget,
						 GdkDragContext       *context,
						 gpointer              user_data);
static void     nautilus_tree_view_drag_end     (GtkWidget            *widget,
						 GdkDragContext       *context,
						 gpointer              user_data);
static void     nautilus_tree_view_drag_leave   (GtkWidget            *widget,
						 GdkDragContext       *context,
						 guint                 time,
						 gpointer              user_data);
static gboolean nautilus_tree_view_drag_motion  (GtkWidget            *widget,
						 GdkDragContext       *context,
						 int                   x,
						 int                   y,
						 guint                 time,
						 gpointer              user_data);
static gboolean nautilus_tree_view_drag_drop    (GtkWidget            *widget,
						 GdkDragContext       *context,
						 int                   x,
						 int                   y,
						 guint                 time,
						 gpointer              user_data);
static void     nautilus_tree_view_drag_data_received   (GtkWidget            *widget,
							 GdkDragContext       *context,
							 gint                   x,
							 gint                   y,
							 GtkSelectionData     *data,
							 guint                 info,
							 guint                 time);

static void     nautilus_tree_view_drag_data_get        (GtkWidget            *widget,
							 GdkDragContext       *context,
							 GtkSelectionData     *data,
							 guint                 info,
							 guint                 time,
							 gpointer              user_data);

static int      nautilus_tree_view_button_release       (GtkWidget            *widget, 
							 GdkEventButton       *event);
static int      nautilus_tree_view_button_press         (GtkWidget            *widget, 
							 GdkEventButton       *event);
static int      nautilus_tree_view_motion_notify        (GtkWidget            *widget, 
							 GdkEventButton       *event);

static char    *nautilus_tree_view_item_at              (NautilusTreeView     *tree_view,
							 int                   x, 
							 int                   y);
static void     nautilus_tree_view_move_copy_files    (NautilusTreeView         *tree_view,
						       GList                    *selection_list,
						       GdkDragContext           *context,
						       const char               *target_uri);
static char    *nautilus_tree_view_find_drop_target   (NautilusTreeView         *tree_view,
						       GList                    *selection_list,
						       GdkDragContext           *context,
						       int                       x, 
						       int                       y);
static char    *nautilus_tree_view_get_drag_uri           (NautilusTreeView      *tree_view);

static void     nautilus_tree_view_expand_or_collapse_row (GtkCTree              *tree, 
							   int                    row);
static void     nautilus_tree_view_expand_row             (GtkCTree              *tree, 
							   int                    row);
static gboolean nautilus_tree_view_is_tree_node_directory (NautilusTreeView      *tree_view,
							   GtkCTreeNode          *node);
static GtkCTreeNode *nautilus_tree_view_tree_node_at      (NautilusTreeView      *tree_view,
							   int                    x, 
							   int                    y);


static void    nautilus_tree_view_start_auto_scroll       (NautilusTreeView      *tree_view);

static void    nautilus_tree_view_stop_auto_scroll        (NautilusTreeView      *tree_view);

static void    nautilus_tree_view_real_scroll             (NautilusTreeView      *tree_view, 
							   float                  x_delta, 
							   float                  y_delta);

static void    nautilus_tree_view_free_drag_data          (NautilusTreeView      *tree_view);



static void nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass);
static void nautilus_tree_view_initialize       (NautilusTreeView      *view);
static void nautilus_tree_view_destroy          (GtkObject             *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusTreeView, nautilus_tree_view, GTK_TYPE_SCROLLED_WINDOW)
     


static void
nautilus_tree_view_initialize_class (NautilusTreeViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_tree_view_destroy;
}


static gboolean
nautilus_tree_view_should_skip_file (NautilusTreeView *view,
				     NautilusFile *file)
{
	char *name;
	gboolean should_skip;

	should_skip = FALSE;

	/* FIXME bugzilla.eazel.com 2419: maybe this should track the "show hidden files" preference? */

	if (!view->details->show_hidden_files) {

		/* FIXME bugzilla.eazel.com 653: 
		 * Eventually this should become a generic filtering thingy. 
		 */
			
		name = nautilus_file_get_name (file);
			
		g_assert (name != NULL);
			
		if (nautilus_str_has_prefix (name, ".")) {
			should_skip = TRUE;
		}
		
		g_free (name);
	}
		
	return should_skip;
}


static void
insert_hack_node (NautilusTreeView *view, const char *uri)
{
 	GtkCTreeNode *view_node;
	GtkCTreeNode *hack_node;
	char *text[2];

#ifdef DEBUG_TREE
	printf ("XXX: possibly adding hack node for %s\n", uri);
#endif

	hack_node = g_hash_table_lookup (view->details->uri_to_hack_node_map, uri);

	if (hack_node == NULL) {
		text[0] = "...HACK...";
		text[1] = NULL;

#ifdef DEBUG_TREE
		printf ("XXX: actually adding hack node for %s\n", uri);
#endif

		view_node = uri_to_view_node (view, uri);

		hack_node = gtk_ctree_insert_node (GTK_CTREE (view->details->tree),
						   view_node, 
						   NULL,
						   text,
						   TREE_SPACING,
						   NULL, NULL, NULL, NULL,
						   FALSE,
						   FALSE);

		g_hash_table_insert (view->details->uri_to_hack_node_map, 
				     (char *) uri, hack_node);
	}
}


static void
remove_hack_node (NautilusTreeView *view, const char *uri)
{
	GtkCTreeNode *hack_node;

#ifdef DEBUG_TREE
	printf ("XXX: removing hack node for %s\n", uri);
#endif

	hack_node = g_hash_table_lookup (view->details->uri_to_hack_node_map, uri);
       
	if (hack_node != NULL) {
		gtk_ctree_remove_node (GTK_CTREE (view->details->tree),
				       hack_node);

		g_hash_table_remove (view->details->uri_to_hack_node_map, uri);

		gtk_clist_thaw (GTK_CLIST (view->details->tree));
#ifdef DEBUG_TREE
		printf ("XXX: actually thawing (%d)\n", GTK_CLIST (view->details->tree)->freeze_count);
#endif
	}
}


static void
freeze_if_have_hack_node (NautilusTreeView *view, const char *uri)
{
	GtkCTreeNode *hack_node;

#ifdef DEBUG_TREE
	puts ("XXX: freezing if hack node");
#endif

	hack_node = g_hash_table_lookup (view->details->uri_to_hack_node_map, uri);

	if (hack_node != NULL) {
		gtk_clist_freeze (GTK_CLIST (view->details->tree));
#ifdef DEBUG_TREE
		printf ("XXX: actually freezing (%d)\n", GTK_CLIST (view->details->tree)->freeze_count);
#endif
	}
}

static void
nautilus_tree_view_insert_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	GtkCTreeNode *parent_view_node;
 	GtkCTreeNode *view_node;
	NautilusFile *file;
	char *uri;
	char *text[2];
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	file = nautilus_tree_node_get_file (node);

	if (nautilus_tree_view_should_skip_file (view, file)) {
		return;
	}

	uri = nautilus_tree_node_get_uri (node);
	
#ifdef DEBUG_TREE
	printf ("Inserting URI into tree: %s\n", uri);
#endif

	parent_view_node = nautilus_tree_view_find_parent_node (view, uri);


#ifdef DEBUG_TREE
	printf ("parent_view_node 0x%x (%s)\n", (unsigned) parent_view_node, 
		view_node_to_uri (view, node);
#endif


	text[0] = nautilus_file_get_name (file);
	text[1] = NULL;

	if (model_node_to_view_node (view, node) == NULL) {
		
		nautilus_icon_factory_get_pixmap_and_mask_for_file (file,
								    NULL,
								    NAUTILUS_ICON_SIZE_FOR_MENUS,
								    &pixmap,
								    &mask);

		view_node = gtk_ctree_insert_node (GTK_CTREE (view->details->tree),
						   parent_view_node, 
						   NULL,
						   text,
						   TREE_SPACING,
						   pixmap, mask, pixmap, mask,
						   FALSE,
						   FALSE);
		
		gtk_ctree_node_set_row_data (GTK_CTREE (view->details->tree),
					     view_node,
					     node);

		g_hash_table_insert (view->details->uri_to_node_map, uri, view_node); 
		
		if (nautilus_file_is_directory (file)) {
			/* Gratuitous hack so node can be expandable w/o
			   immediately inserting all the real children. */

			if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
				gtk_ctree_expand (GTK_CTREE (GTK_CTREE (view->details->tree)),
						  view_node);
			} else {
				insert_hack_node (view, uri);
			}
		}
	}

	g_free (text[0]);

	if (parent_view_node != NULL) {
		remove_hack_node (view, view_node_to_uri (view, parent_view_node));
	}
}



static void
nautilus_tree_view_remove_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	GtkCTreeNode *view_node;
	char *uri;

	nautilus_tree_model_stop_monitoring_node (view->details->model, node, view);

	uri = nautilus_tree_node_get_uri (node);
	
	view_node = model_node_to_view_node (view, node); 
	
	if (view_node != NULL) {
		gtk_ctree_remove_node (GTK_CTREE (view->details->tree),
				       view_node);
		
		/* FIXME bugzilla.eazel.com 2420: free the original key */
		g_hash_table_remove (view->details->uri_to_node_map, uri); 
	}

	nautilus_tree_expansion_state_remove_node (view->details->expansion_state,
						   uri);

	g_free (uri);
}




static gboolean
ctree_is_node_expanded (GtkCTree     *ctree,
			GtkCTreeNode *node)
{
	gchar     *text;
	guint8     spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean   is_leaf;
	gboolean   expanded;

	gtk_ctree_get_node_info (ctree, node,
				 &text, &spacing,
				 &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened,
				 &is_leaf, &expanded);
	return expanded;
}

static void
nautilus_tree_view_update_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	GtkCTreeNode *view_node;
	NautilusFile *file;
	char *uri;
	char *name;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	file = nautilus_tree_node_get_file (node);

	uri = nautilus_tree_node_get_uri (node);

	view_node = model_node_to_view_node (view, node);

	if (view_node != NULL) {

		name = nautilus_tree_view_uri_to_name (uri);
	
		nautilus_icon_factory_get_pixmap_and_mask_for_file (file,
								    NULL,
								    NAUTILUS_ICON_SIZE_FOR_MENUS,
								    &pixmap,
								    &mask);
		
		gtk_ctree_node_set_pixtext (GTK_CTREE (view->details->tree),
					    view_node,
					    0,
					    name,
					    TREE_SPACING,
					    pixmap,
					    mask);
		
#if 0
		/* FIXME bugzilla.eazel.com 2421: 
		 * should switch to this call so we can set open/closed pixamps */
		void gtk_ctree_set_node_info  (GtkCTree     *ctree,
					       GtkCTreeNode *node,
					       const gchar  *text,
					       guint8        spacing,
					       GdkPixmap    *pixmap_closed,
					       GdkBitmap    *mask_closed,
					       GdkPixmap    *pixmap_opened,
					       GdkBitmap    *mask_opened,
					       gboolean      is_leaf,
					       gboolean      expanded);
#endif	

		if (nautilus_file_is_directory (nautilus_tree_node_get_file (node))) {
			if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
				if (!ctree_is_node_expanded (GTK_CTREE (view->details->tree),
							     view_node)) {
					gtk_ctree_expand (GTK_CTREE (view->details->tree),
							  view_node);
				} 
			} else {
				if (ctree_is_node_expanded (GTK_CTREE (view->details->tree),
								       view_node)) {
					gtk_ctree_collapse (GTK_CTREE (view->details->tree),
							    view_node);
				} else {
					insert_hack_node (view, uri);
				}
			}
		}
	} else {
		g_free (uri);
	}
}


static void
notify_done_loading (NautilusTreeView *view,
		     NautilusTreeNode *node)
{
	TreeViewCallback callback;

	if (view->details->uri_loaded_or_parent_done_loading != NULL &&
	    view->details->wait_node == node) {
		callback = view->details->uri_loaded_or_parent_done_loading;
		view->details->wait_node = NULL;
		g_free (view->details->wait_uri);
		view->details->wait_uri = NULL;

		(*callback) (view);
	}
}

static void
notify_node_seen (NautilusTreeView *view,
		  NautilusTreeNode *node)
{
	TreeViewCallback root_callback;
	TreeViewCallback callback;
	char *uri;

	if (!view->details->root_seen) {
		view->details->root_seen = TRUE;

		if (view->details->root_seen_callback != NULL) {
			root_callback = view->details->root_seen_callback;
			view->details->root_seen_callback = NULL;
			
			(*root_callback) (view);
		}
	}

	uri = nautilus_tree_node_get_uri (node);
	
	if (nautilus_strcmp (uri, view->details->wait_uri) == 0) {
		callback = view->details->uri_loaded_or_parent_done_loading;
		view->details->wait_node = NULL;
		g_free (view->details->wait_uri);
		view->details->wait_uri = NULL;

		(*callback) (view);
	}
}





static void
nautilus_tree_view_model_node_added_callback (NautilusTreeModel *model,
					      NautilusTreeNode  *node,
					      gpointer           callback_data)
{
	NautilusTreeView *view;

#ifdef DEBUG_TREE
	puts ("XXX: added");
#endif

	view = NAUTILUS_TREE_VIEW (callback_data);

	nautilus_tree_view_insert_model_node (view, node);
	notify_node_seen (view, node);
}





static void
nautilus_tree_view_model_node_changed_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	NautilusTreeView *view;
	char *uri;

	uri = nautilus_tree_node_get_uri (node);

#ifdef DEBUG_TREE
	printf ("XXX: changed %s\n", uri);
#endif

	g_free (uri);

	view = NAUTILUS_TREE_VIEW (callback_data);

	/* Assume file did not change location - we will model that as a remove + an add */
	nautilus_tree_view_update_model_node (view, node);
}

static void
nautilus_tree_view_model_node_removed_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	NautilusTreeView *view;

#ifdef DEBUG_TREE
	puts ("XXX: removed");
#endif

	view = NAUTILUS_TREE_VIEW (callback_data);

	nautilus_tree_view_remove_model_node (view, node);
}


static void
nautilus_tree_view_model_done_loading_callback (NautilusTreeModel *model,
						NautilusTreeNode  *node,
						gpointer           callback_data)
{
	NautilusTreeView *view;
	char *uri;

#ifdef DEBUG_TREE
	puts ("XXX: done loading");
#endif

	view = NAUTILUS_TREE_VIEW (callback_data);

	uri = nautilus_tree_node_get_uri (node);

	remove_hack_node (view, uri);

	g_free (uri);

	notify_done_loading (view, node);
}

static void
nautilus_tree_view_load_from_filesystem (NautilusTreeView *view)
{
	view->details->model = nautilus_tree_model_new ("file:///");

	nautilus_tree_model_monitor_add (view->details->model,
					 view,
					 nautilus_tree_view_model_node_added_callback,
					 view);

	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "node_added",
			    nautilus_tree_view_model_node_added_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "node_changed",
			    nautilus_tree_view_model_node_changed_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "node_removed",
			    nautilus_tree_view_model_node_removed_callback,
			    view);
	gtk_signal_connect (GTK_OBJECT (view->details->model),
			    "done_loading_children",
			    nautilus_tree_view_model_done_loading_callback,
			    view);

}

static GtkTargetEntry nautilus_tree_view_dnd_target_table[] = {
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
	{ NAUTILUS_ICON_DND_COLOR_TYPE, 0, NAUTILUS_ICON_DND_COLOR },
	{ NAUTILUS_ICON_DND_BGIMAGE_TYPE, 0, NAUTILUS_ICON_DND_BGIMAGE },
	{ NAUTILUS_ICON_DND_KEYWORD_TYPE, 0, NAUTILUS_ICON_DND_KEYWORD }

};


static void 
tree_view_realize_callback (GtkWidget *widget, gpointer user_data) 
{
	GtkStyle *style, *new_style;
	NautilusTreeView *tree_view;
	GdkColor new_prelight_color;
	int i;

	tree_view = NAUTILUS_TREE_VIEW (user_data);	
	style = gtk_widget_get_style (widget);
	tree_view->details->dnd->normal_style = style;
	gtk_style_ref (style);

	new_style = gtk_style_copy (style);
        gtk_style_ref(new_style);

	/* calculate a new prelighting color */
	nautilus_gtk_style_shade (&style->bg[GTK_STATE_SELECTED], 
				  &new_prelight_color, 
				  1.35);
	/* set the new color to our special prelighting Style. */
        for (i = 0; i < 5; i++) {
		new_style->bg[i].red = new_prelight_color.red;
		new_style->bg[i].green = new_prelight_color.green;
		new_style->bg[i].blue = new_prelight_color.blue;
		new_style->base[i].red = new_prelight_color.red;
		new_style->base[i].green = new_prelight_color.green;
		new_style->base[i].blue = new_prelight_color.blue;
		new_style->fg[i].red = style->fg[GTK_STATE_SELECTED].red;
		new_style->fg[i].green = style->fg[GTK_STATE_SELECTED].green;
		new_style->fg[i].blue = style->fg[GTK_STATE_SELECTED].blue;
        }

	tree_view->details->dnd->highlight_style = new_style;
}


static void
nautilus_tree_view_init_dnd (NautilusTreeView *view)
{
	view->details->dnd = g_new0 (NautilusTreeViewDndDetails, 1);
	view->details->dnd->motion_queue = nautilus_queue_new ();
	view->details->dnd->target_list = gtk_target_list_new (nautilus_tree_view_dnd_target_table, 
							       NAUTILUS_N_ELEMENTS (nautilus_tree_view_dnd_target_table));

	view->details->dnd->drag_info = g_new0 (NautilusDragInfo, 1);
	nautilus_drag_init (view->details->dnd->drag_info,
			    nautilus_tree_view_dnd_target_table,
			    NAUTILUS_N_ELEMENTS (nautilus_tree_view_dnd_target_table),
			    NULL);


	gtk_drag_dest_set (GTK_WIDGET (view->details->tree), 
			   0,
			   nautilus_tree_view_dnd_target_table,
			   NAUTILUS_N_ELEMENTS (nautilus_tree_view_dnd_target_table),
			   GDK_ACTION_COPY 
			   | GDK_ACTION_MOVE 
			   | GDK_ACTION_LINK 
			   | GDK_ACTION_ASK);


	gtk_signal_connect (GTK_OBJECT (view->details->tree), 
			    "drag_begin", 
			    GTK_SIGNAL_FUNC(nautilus_tree_view_drag_begin), 
			    view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->tree), 
			    "drag_end", 
			    GTK_SIGNAL_FUNC(nautilus_tree_view_drag_end), 
			    view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->tree), 
			    "drag_leave", 
			    GTK_SIGNAL_FUNC(nautilus_tree_view_drag_leave), 
			    view);

	gtk_signal_connect (GTK_OBJECT (view->details->tree), 
			    "drag_motion", 
			    GTK_SIGNAL_FUNC(nautilus_tree_view_drag_motion), 
			    view);

	gtk_signal_connect (GTK_OBJECT (view->details->tree), 
			    "drag_drop", 
			    GTK_SIGNAL_FUNC(nautilus_tree_view_drag_drop), 
			    view);

	gtk_signal_connect (GTK_OBJECT (view->details->tree), 
			    "drag_data_received", 
			    GTK_SIGNAL_FUNC(nautilus_tree_view_drag_data_received), 
			    view);

	gtk_signal_connect (GTK_OBJECT (view->details->tree), 
			    "drag_data_get", 
			    GTK_SIGNAL_FUNC(nautilus_tree_view_drag_data_get), 
			    view);


}

static void
nautilus_tree_view_initialize (NautilusTreeView *view)
{
	/* set up scrolled window */
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
						
	view->details = g_new0 (NautilusTreeViewDetails, 1);

	/* set up expansion state */
	view->details->expansion_state = nautilus_tree_expansion_state_new ();
	

	/* set up ctree */
	view->details->tree = gtk_ctree_new (1, 0);

	gtk_object_set_data (GTK_OBJECT(view->details->tree), "tree_view", (gpointer) view);
	gtk_widget_add_events(GTK_WIDGET(view->details->tree), GDK_POINTER_MOTION_MASK);
	
	/* override the default handlers */
	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "button-press-event", 
			    GTK_SIGNAL_FUNC (nautilus_tree_view_button_press), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "button-release-event", 
			    GTK_SIGNAL_FUNC (nautilus_tree_view_button_release), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "motion-notify-event", 
			    GTK_SIGNAL_FUNC (nautilus_tree_view_motion_notify), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (view->details->tree), "realize", 
			    tree_view_realize_callback, view);

	
        gtk_clist_set_selection_mode (GTK_CLIST (view->details->tree), GTK_SELECTION_BROWSE);
	gtk_clist_set_auto_sort (GTK_CLIST (view->details->tree), TRUE);
	gtk_clist_set_sort_type (GTK_CLIST (view->details->tree), GTK_SORT_ASCENDING);
	gtk_clist_set_column_auto_resize (GTK_CLIST (view->details->tree), 0, TRUE);
	gtk_clist_columns_autosize (GTK_CLIST (view->details->tree));

	gtk_clist_set_reorderable (GTK_CLIST (view->details->tree), FALSE);

	gtk_ctree_set_expander_style (GTK_CTREE (view->details->tree), GTK_CTREE_EXPANDER_TRIANGLE);
	gtk_ctree_set_line_style (GTK_CTREE (view->details->tree), GTK_CTREE_LINES_NONE);

	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "tree_expand",
			    GTK_SIGNAL_FUNC (tree_expand_callback), 
			    view);

	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "tree_collapse",
			    GTK_SIGNAL_FUNC (tree_collapse_callback), 
			    view);
	
	gtk_signal_connect (GTK_OBJECT (view->details->tree),
			    "tree_select_row",
			    GTK_SIGNAL_FUNC (tree_select_row_callback), 
			    view);


	/* init dnd */

	nautilus_tree_view_init_dnd (view);

	/* set up view */
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (tree_load_location_callback), 
			    view);

	view->details->uri_to_node_map = g_hash_table_new (g_str_hash, g_str_equal);
	view->details->uri_to_hack_node_map = g_hash_table_new (g_str_hash, g_str_equal);
	
	nautilus_tree_view_load_from_filesystem (view);

	gtk_signal_connect_object_while_alive
		(nautilus_icon_factory_get (),
		 "icons_changed",
		 nautilus_tree_view_update_all_icons,
		 GTK_OBJECT (view));	

	gtk_widget_show (view->details->tree);

	gtk_container_add (GTK_CONTAINER (view), view->details->tree);

	gtk_widget_show (GTK_WIDGET (view));
}



static void
disconnect_model_handlers (NautilusTreeView *view)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node (view->details->model,
					     "file:///");

	if (node != NULL) {
		nautilus_tree_model_monitor_remove (view->details->model, view);
	}
}


static void
nautilus_tree_view_destroy (GtkObject *object)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (object);
	
	/* you do not need to unref the normal style */
	if (view->details->dnd->highlight_style != NULL) {
		gtk_style_unref(view->details->dnd->highlight_style);
	}
	nautilus_drag_finalize (view->details->dnd->drag_info);
	gtk_target_list_unref (view->details->dnd->target_list);
	g_free (view->details->dnd);
	/* FIXME bugzilla.eazel.com 2422: destroy drag_info */

	disconnect_model_handlers (view);
	gtk_object_unref (GTK_OBJECT (view->details->model));

	nautilus_tree_expansion_state_save (view->details->expansion_state);
	gtk_object_unref (GTK_OBJECT (view->details->expansion_state));

	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/**
 * nautilus_tree_view_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: NautilusTreeView to get the nautilus_view from..
 * 
 **/
NautilusView *
nautilus_tree_view_get_nautilus_view (NautilusTreeView *view)
{
	return view->details->nautilus_view;
}


static GtkCTreeNode *
uri_to_view_node (NautilusTreeView *view,
		  const char *uri)
{
	return g_hash_table_lookup (view->details->uri_to_node_map, uri);
}


static GtkCTreeNode *
model_node_to_view_node (NautilusTreeView *view,
			 NautilusTreeNode *node)
{
	GtkCTreeNode *view_node;
	char *uri;

	if (node == NULL) {
		return NULL;
	}

	uri = nautilus_tree_node_get_uri (node);
	view_node = uri_to_view_node (view, uri);
	g_free (uri);

	return view_node;
}

static NautilusTreeNode *
view_node_to_model_node (NautilusTreeView *view,
			 GtkCTreeNode *node)
{
	NautilusTreeNode *tree_node;

	tree_node = (NautilusTreeNode *) gtk_ctree_node_get_row_data (GTK_CTREE (view->details->tree),
								      node);

	return tree_node;
}

static const char *
view_node_to_uri (NautilusTreeView *view,
		  GtkCTreeNode *node)
{
	NautilusTreeNode *tree_node;
	char *uri;

	tree_node = view_node_to_model_node (view, node);

	if (tree_node == NULL) {
		return NULL;
	}

	uri = nautilus_tree_node_get_uri (tree_node);

	return uri;
}

static GList *
get_uri_sequence_to_root (char *uri_text)
{
	GList *retval;
	GnomeVFSURI *uri;
	GnomeVFSURI *parent_uri;

	retval = NULL;

	uri = gnome_vfs_uri_new (uri_text);

	retval = g_list_prepend (retval, uri_text);
	
	if (uri == NULL) {
		return retval;
	}

	while (1) {
		parent_uri = gnome_vfs_uri_get_parent (uri);
		
		gnome_vfs_uri_unref (uri);

		if  (parent_uri == NULL) {
			return retval;
		}

		uri = parent_uri;
		uri_text = gnome_vfs_uri_to_string (uri, 
						    GNOME_VFS_URI_HIDE_NONE);

		retval = g_list_prepend (retval, uri_text);
	}
}


static void
call_when_root_seen (NautilusTreeView *view, 
		     TreeViewCallback callback)
{
	view->details->root_seen_callback = callback;
}

static void
call_when_uri_loaded_or_parent_done_loading (NautilusTreeView *view,
					     const char *uri, 
					     NautilusTreeNode *node,
					     TreeViewCallback callback)
{
	view->details->wait_uri = g_strdup (uri);
	view->details->wait_node = node;

	view->details->uri_loaded_or_parent_done_loading = callback;
}


static void
cancel_selection_in_progress (NautilusTreeView *view)
{
	nautilus_g_list_free_deep (view->details->in_progress_select_uris);
	view->details->in_progress_select_uris = NULL;

	view->details->root_seen_callback = NULL;

	g_free (view->details->wait_uri);
	view->details->wait_uri = NULL;
	view->details->wait_node = NULL;

	view->details->uri_loaded_or_parent_done_loading = NULL;
}

static void
expand_uri_sequence_and_select_end (NautilusTreeView *view)
{
	const char *uri;
	GList *p;
	GList *old_sequence;
	GtkCTreeNode *view_node;
	gboolean at_least_one_found;

	at_least_one_found = FALSE;
	uri = NULL;

	if (!view->details->root_seen) {
		call_when_root_seen (view, expand_uri_sequence_and_select_end);
		return;
	}

	for (p = view->details->in_progress_select_uris; p != NULL; p = p->next) {
		uri = (char *) p->data;

		view_node = uri_to_view_node (view, uri);

		if (view_node == NULL) {
			break;
		}

		at_least_one_found = TRUE;

		if (p->next != NULL) {
			gtk_ctree_expand (GTK_CTREE (view->details->tree),
					  view_node);
		} else {
			g_free (view->details->selected_uri);
			view->details->selected_uri = g_strdup (uri);
			gtk_ctree_select (GTK_CTREE (view->details->tree),
					  view_node);
		}
	}
		
	if (!at_least_one_found) {
		/* The target URI just isn't in the tree at all; don't
                   expand, load or select anything */
		
		cancel_selection_in_progress (view);

		return;
	}

	if (p != NULL) {
		/* Not all the nodes existed yet, damn */
		
		old_sequence = view->details->in_progress_select_uris;
		
		view->details->in_progress_select_uris = p;

		call_when_uri_loaded_or_parent_done_loading (view, uri, 
							     nautilus_tree_model_get_node (view->details->model,
											   (char *) p->prev->data),
							     expand_uri_sequence_and_select_end);


		p->prev->next = NULL;
		p->prev = NULL;
		nautilus_g_list_free_deep (old_sequence);

		return;
	}

	/* We're all done, clean up potential remaining junk. */

	cancel_selection_in_progress (view);
}


/**
 * nautilus_tree_view_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * 
 **/
void
nautilus_tree_view_load_uri (NautilusTreeView *view,
			     const char       *uri)
{
	char *canonical_uri;

	cancel_selection_in_progress (view);


	canonical_uri = nautilus_make_uri_canonical (uri);

	if (nautilus_strcmp (canonical_uri, view->details->selected_uri) == 0) {
		g_free (canonical_uri);
		return;
	}

	view->details->in_progress_select_uris = get_uri_sequence_to_root (canonical_uri);
			
	expand_uri_sequence_and_select_end (view);
}
 
static void
tree_load_location_callback (NautilusView *nautilus_view, 
			     const char *location,
			     NautilusTreeView *view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	/* Do the actual load. */
	nautilus_tree_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}


static void
reload_node_for_uri (NautilusTreeView *view,
		     const char *uri)
{
	GList *p;

	nautilus_tree_model_monitor_node (view->details->model,
					  nautilus_tree_model_get_node (view->details->model,
									uri),
					  view);

	for (p = nautilus_tree_node_get_children 
		(nautilus_tree_model_get_node (view->details->model, uri)); p != NULL; p = p->next) {
		nautilus_tree_view_update_model_node (view, (NautilusTreeNode *) p->data);
	}
}

static void
expand_node_for_uri (NautilusTreeView *view,
		     const char *uri)
{

	freeze_if_have_hack_node (view, uri);

	nautilus_tree_expansion_state_expand_node (view->details->expansion_state,
						   uri);

	reload_node_for_uri (view, uri);
}

static void
tree_expand_callback (GtkCTree         *ctree,
		      GtkCTreeNode     *node,
		      NautilusTreeView *view)
{
	const char *uri;

	uri = view_node_to_uri (view, node);

	expand_node_for_uri (view, uri);
}



static void
tree_collapse_callback (GtkCTree         *ctree,
			GtkCTreeNode     *node,
			NautilusTreeView *view)
{
	const char *uri;

	uri = view_node_to_uri (view, node);

	nautilus_tree_expansion_state_collapse_node (view->details->expansion_state,
						     uri);

	nautilus_tree_model_stop_monitoring_node_recursive (view->details->model,
							    nautilus_tree_model_get_node (view->details->model,
											  uri),
							    view);
}




static void
tree_select_row_callback (GtkCTree              *tree,
			  GtkCTreeNode          *node,
			  gint                   column,
			  NautilusTreeView      *view)
{
	const char *uri;
	
	uri = view_node_to_uri (view, node);
	
	if (uri != NULL &&
	    nautilus_strcmp (view->details->selected_uri, uri) != 0) {
		nautilus_view_open_location (NAUTILUS_VIEW (view->details->nautilus_view), uri);
		
		g_free (view->details->selected_uri);
		view->details->selected_uri = g_strdup (uri);
	}

}


static void 
nautilus_tree_view_update_all_icons (NautilusTreeView *view)
{
	nautilus_tree_model_monitor_add (view->details->model,
					 view,
					 nautilus_tree_view_model_node_changed_callback,
					 view);
}


static char *
nautilus_tree_view_uri_to_name (const char *uri)
{
	GnomeVFSURI *gnome_vfs_uri;
	char *name;
	
	gnome_vfs_uri = gnome_vfs_uri_new (uri);
	name = g_strdup (gnome_vfs_uri_extract_short_path_name (gnome_vfs_uri));
	gnome_vfs_uri_unref (gnome_vfs_uri);

	return name;
}

static GtkCTreeNode *
nautilus_tree_view_find_parent_node (NautilusTreeView *view, 
				     const char *uri)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node (view->details->model, uri);

	g_assert (node != NULL);

	return model_node_to_view_node (view, nautilus_tree_node_get_parent (node));
}

static void     
nautilus_tree_view_drag_begin (GtkWidget *widget, GdkDragContext *context,
			       gpointer user_data)
{
	NautilusTreeView *tree_view;
	NautilusTreeViewDndDetails *dnd;

	tree_view = NAUTILUS_TREE_VIEW (user_data);
	dnd = tree_view->details->dnd;

	/* The drag is started. reinit the drag data. */
	dnd->drag_pending = FALSE;

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_begin");
}

static void
nautilus_tree_view_drag_end (GtkWidget *widget, GdkDragContext *context,
			     gpointer user_data)
{
	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_end");

}


static void
nautilus_tree_view_drag_leave (GtkWidget *widget,
			       GdkDragContext *context,
			       guint time,
			       gpointer user_data)
{
	NautilusTreeView *tree_view;
	NautilusTreeViewDndDetails *dnd;

	tree_view = NAUTILUS_TREE_VIEW (user_data);
	dnd = tree_view->details->dnd;

	/* bring the highlighted row back to normal. */
	if (dnd->current_prelighted_node != NULL) {
		gtk_ctree_node_set_row_style (GTK_CTREE (tree_view->details->tree), 
					      dnd->current_prelighted_node,
					      tree_view->details->dnd->normal_style);
	}
	dnd->current_prelighted_node = NULL;

	/* stop autoscroll */
	nautilus_tree_view_stop_auto_scroll (tree_view);

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_leave");

}

static int motion_time_callback (gpointer data);

static gint 
motion_time_callback (gpointer data)
{
	NautilusTreeView *tree_view;
	NautilusQueue *motion_queue;
	int first_y;

	tree_view = NAUTILUS_TREE_VIEW (data);

	motion_queue = tree_view->details->dnd->motion_queue;

	first_y = GPOINTER_TO_INT (nautilus_queue_remove (motion_queue));
	
	if (abs (first_y - tree_view->details->dnd->current_y) <= 10) {
		int row, column;
		/* we have almost not moved. so we can say we want to 
		 expand the place we are in. */
		gtk_clist_get_selection_info (GTK_CLIST (tree_view->details->tree), 
					      tree_view->details->dnd->current_x, 
					      tree_view->details->dnd->current_y, 
					      &row, &column);
		nautilus_tree_view_expand_row (GTK_CTREE (tree_view->details->tree), row);
	}

	/* never be called again for this "y" value so return FALSE */
	return FALSE;
}

static gboolean 
nautilus_tree_view_drag_motion (GtkWidget *widget, GdkDragContext *context,
				int x, int y, guint time, gpointer user_data)
{
	NautilusTreeView *tree_view;
	NautilusTreeViewDndDetails *dnd;
	GtkCTreeNode *node;
	gboolean is_directory;
#if 0
	int resulting_action;
#endif
	tree_view = NAUTILUS_TREE_VIEW (user_data);
	dnd = (NautilusTreeViewDndDetails *) (tree_view->details->dnd);

	/* update for expanding the thing under us if we stay long 
	   enough above it. */
	dnd->current_x = x;
	dnd->current_y = y;
	/* add the current move to the list of moves to be tested by our
	   timeout handler later */
	nautilus_queue_add (dnd->motion_queue, GINT_TO_POINTER(y));
	g_timeout_add (EXPAND_TIMEOUT, motion_time_callback, user_data);



	/* make the thing under us prelighted. */
	/* FIXME: we should do this only for certain kinds
	   of drags. ie: files, keywords... not colors or backgrounds */
	node = nautilus_tree_view_tree_node_at (NAUTILUS_TREE_VIEW (tree_view), x, y);
	is_directory = nautilus_tree_view_is_tree_node_directory (NAUTILUS_TREE_VIEW (tree_view), node);

	if (is_directory == FALSE) {
		NautilusTreeNode *parent_node, *current_node;
		current_node = view_node_to_model_node (tree_view, node);
		parent_node = nautilus_tree_node_get_parent (current_node);
		node = model_node_to_view_node (tree_view, parent_node);
	} 

	/* get the current cell's parent */
	if (node != dnd->current_prelighted_node 
	    && dnd->current_prelighted_node != NULL) {
		gtk_ctree_node_set_row_style (GTK_CTREE (tree_view->details->tree), 
					      dnd->current_prelighted_node,
					      tree_view->details->dnd->normal_style);
	}
	gtk_ctree_node_set_row_style (GTK_CTREE (tree_view->details->tree), 
				      node,
				      tree_view->details->dnd->highlight_style);
	dnd->current_prelighted_node = node;



	/* update dragging cursor. */
	/* FIXME bugzilla.eazel.com 2417: this does not work */
#if 0
	resulting_action = nautilus_drag_modifier_based_action (GDK_ACTION_COPY, 
								GDK_ACTION_MOVE);
#endif
	gdk_drag_status (context, context->suggested_action, time);


	/* auto scroll */
	nautilus_tree_view_start_auto_scroll (tree_view);

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_motion");
	return TRUE;
}


static gboolean 
nautilus_tree_view_drag_drop (GtkWidget *widget,
			      GdkDragContext *context,
			      int x, int y, guint time,
			      gpointer user_data)
{
	NautilusTreeViewDndDetails *dnd;
	NautilusTreeView *tree_view;

	tree_view = NAUTILUS_TREE_VIEW (user_data);
	dnd = (NautilusTreeViewDndDetails *) (tree_view->details->dnd);

	/* critical piece of code. this ensures that our 
	 * drag_data_received callback is going to be called 
	 * soon and it will execute the right actions since the 
	 * drop occured.
	 */
	dnd->drag_info->drop_occured = TRUE;
	gtk_drag_get_data (GTK_WIDGET (widget), context,
			   GPOINTER_TO_INT (context->targets->data),
			   time);


	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_drop");

	return TRUE;
}


static void
nautilus_tree_view_receive_dropped_icons (NautilusTreeView *view,
					  GdkDragContext *context,
					  int x, int y)
{
	NautilusDragInfo *drag_info;
	NautilusTreeView *tree_view;
	gboolean local_move_only;
	char *drop_target_uri;

	tree_view = NAUTILUS_TREE_VIEW (view);
	drag_info = tree_view->details->dnd->drag_info;

	drop_target_uri = NULL;

	if (drag_info->selection_list == NULL) {
		return;
	}

	if (context->action == GDK_ACTION_ASK) {
		context->action = nautilus_drag_drop_action_ask 
			(GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	}

	if (context->action > 0) {
		drop_target_uri = nautilus_tree_view_find_drop_target (tree_view, 
								       drag_info->selection_list,
								       context, 
								       x, y);
		if (drop_target_uri == NULL) {
			nautilus_drag_destroy_selection_list (drag_info->selection_list);
			return;
		}

		local_move_only = FALSE;
		/* calculate if we ended to drop into the orignal source... */
		local_move_only = nautilus_drag_items_local (drop_target_uri, 
							     drag_info->selection_list);
		
		/* do nothing for the local case: we do not reorder. */
		if (!local_move_only) {
			{
				char *action_string;
				switch (context->action) {
				case GDK_ACTION_MOVE:
					action_string = "move";
					break;
				case GDK_ACTION_COPY:
					action_string = "copy";
					break;
				case GDK_ACTION_LINK:
					action_string = "link";
					break;
				default:
					g_assert_not_reached ();
					action_string = "error";
					break;
				}
				
				g_print ("%s selection in %s\n", 
					 action_string, drop_target_uri);
			}
			nautilus_tree_view_move_copy_files (tree_view, drag_info->selection_list, 
							    context, drop_target_uri);
		}
		g_free (drop_target_uri);
		nautilus_drag_destroy_selection_list (drag_info->selection_list);
		drag_info->selection_list = NULL;
	}
}


static void
nautilus_tree_view_dropped_keyword (NautilusTreeView *tree_view, const char *keyword, int x, int y)
{
	/* FIXME: implementing this is going to be some kind of pain.
	   Need to think about it.
	 */
}

static void 
nautilus_tree_view_drag_data_received (GtkWidget *widget,
				       GdkDragContext *context,
				       gint x, gint y,
				       GtkSelectionData *data,
				       guint info, guint time)
{
	NautilusTreeViewDndDetails *dnd;
	NautilusDragInfo *drag_info;
	NautilusTreeView *tree_view;

	tree_view = NAUTILUS_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (widget), "tree_view"));
	dnd = tree_view->details->dnd;
	drag_info = dnd->drag_info;

	drag_info->data_type = info;
	drag_info->got_drop_data_type = TRUE;


	/* save operation for drag motion events */
	switch (info) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		drag_info->selection_list = nautilus_drag_build_selection_list (data);
		break;
	case NAUTILUS_ICON_DND_URI_LIST:
		drag_info->selection_list = nautilus_drag_build_selection_list (data);
		break;
	case NAUTILUS_ICON_DND_COLOR:
		break;
	case NAUTILUS_ICON_DND_BGIMAGE:	
		break;
	case NAUTILUS_ICON_DND_KEYWORD:	
		break;
	default:

		break;
	}


	/* drop occured: do actual operations on the data */
	if (drag_info->drop_occured == TRUE) {
		switch (info) {
		case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
			nautilus_tree_view_receive_dropped_icons
				(NAUTILUS_TREE_VIEW (tree_view),
				 context, x, y);
			gtk_drag_finish (context, TRUE, FALSE, time);
			break;
		case NAUTILUS_ICON_DND_URI_LIST:
			nautilus_tree_view_receive_dropped_icons
				(NAUTILUS_TREE_VIEW (tree_view),
				 context, x, y);
			gtk_drag_finish (context, TRUE, FALSE, time);
			break;
		case NAUTILUS_ICON_DND_COLOR:
			nautilus_background_receive_dropped_color
				(nautilus_get_widget_background (widget),
				 widget, x, y, data);
			gtk_drag_finish (context, TRUE, FALSE, time);
			break;
		case NAUTILUS_ICON_DND_BGIMAGE:
			nautilus_background_receive_dropped_background_image
				(nautilus_get_widget_background (widget),
				 (char *)data);
			gtk_drag_finish (context, FALSE, FALSE, time);
			break;
		case NAUTILUS_ICON_DND_KEYWORD:
			nautilus_tree_view_dropped_keyword
				(NAUTILUS_TREE_VIEW (tree_view),
				 (char *)data, x, y);
			gtk_drag_finish (context, FALSE, FALSE, time);
			break;
		default:
			gtk_drag_finish (context, FALSE, FALSE, time);
		}
		
		nautilus_tree_view_free_drag_data (NAUTILUS_TREE_VIEW (tree_view));
		
		/* reinitialise it for the next dnd */
		drag_info->drop_occured = FALSE;
	}

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_data_received");

}


static void nautilus_tree_view_drag_data_get (GtkWidget *widget,
					      GdkDragContext *context,
					      GtkSelectionData *data,
					      guint info, guint time,
					      gpointer user_data)
{
	NautilusTreeView *tree_view;
	char *uri, *selection_string;

	g_assert (widget != NULL);
	g_return_if_fail (context != NULL);

	tree_view = NAUTILUS_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (widget), "tree_view"));

	uri = nautilus_tree_view_get_drag_uri (tree_view);
	selection_string = g_strconcat (uri, "\r\n", NULL);


	gtk_selection_data_set (data,
				data->target,
				8, selection_string, strlen(selection_string));
	g_free (uri);

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_data_get");

}









/* --------------------------------------------------------------
   standard gtk events: press/release/motion. used to start drags 
   --------------------------------------------------------------
*/









static int 
nautilus_tree_view_button_press (GtkWidget *widget, GdkEventButton *event)
{
	int retval;
	GtkCList *clist;
	NautilusTreeView *tree_view;
	int press_row, press_column, on_row;

	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return retval;

	tree_view = NAUTILUS_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (widget), "tree_view"));

	on_row = gtk_clist_get_selection_info (GTK_CLIST (tree_view->details->tree), 
					       event->x, 
					       event->y, 
					       &press_row, &press_column);
	if (on_row == 0) {
		gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "button-press-event");
		return FALSE;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		tree_view->details->dnd->drag_pending = TRUE;
		tree_view->details->dnd->press_x = event->x;
		tree_view->details->dnd->press_y = event->y;
		tree_view->details->dnd->pressed_button = event->button;
		break;
	case GDK_2BUTTON_PRESS:
	default:
		break;
	}

	if (gtk_ctree_is_hot_spot (GTK_CTREE (widget), event->x, event->y)) {
		tree_view->details->dnd->pressed_hot_spot = TRUE;
	}

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "button-press-event");
	return TRUE;
}

#define RADIUS 200

static int
nautilus_tree_view_button_release (GtkWidget *widget, GdkEventButton *event)
{
	int retval;
	GtkCList *clist;
	NautilusTreeView *tree_view;
	int release_row, release_column, on_row;
	int distance_squared;
	gboolean is_still_hot_spot;

	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return retval;

	tree_view = NAUTILUS_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (widget), "tree_view"));

	tree_view->details->dnd->drag_pending = FALSE;

	distance_squared = (event->x - tree_view->details->dnd->press_x)
		* (event->x - tree_view->details->dnd->press_x) +
		(event->y - tree_view->details->dnd->press_y)
		* (event->y - tree_view->details->dnd->press_y);
	is_still_hot_spot = gtk_ctree_is_hot_spot (GTK_CTREE(tree_view->details->tree), 
						   event->x, event->y);
	
	on_row = gtk_clist_get_selection_info (GTK_CLIST (tree_view->details->tree), 
					       event->x, 
					       event->y, 
					       &release_row, &release_column);

	if (on_row == 1) {

		if (distance_squared <= RADIUS 
		    && tree_view->details->dnd->pressed_hot_spot == TRUE
		    && is_still_hot_spot == TRUE) {
			
			tree_view->details->dnd->pressed_hot_spot = FALSE;
			
			nautilus_tree_view_expand_or_collapse_row (GTK_CTREE(tree_view->details->tree), 
								   release_row);
		} else if (distance_squared <= RADIUS) {
			/* we are close from the place we clicked */
			/* select current row */
			gtk_clist_select_row (GTK_CLIST (tree_view->details->tree), 
					      release_row, release_column);
		}
	}

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "button-release-event");

	return TRUE;

}



static int
nautilus_tree_view_motion_notify (GtkWidget *widget, GdkEventButton *event)
{
	GtkCList *clist;
	NautilusTreeView *tree_view;

	clist = GTK_CLIST (widget);


	if (event->window != clist->clist_window)
		return FALSE;

	tree_view = NAUTILUS_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (widget), "tree_view"));

	if (tree_view->details->dnd->drag_pending == TRUE) {
		int distance_squared;

		distance_squared = (event->x - tree_view->details->dnd->press_x)
			* (event->x - tree_view->details->dnd->press_x) +
			(event->y - tree_view->details->dnd->press_y)
			* (event->y - tree_view->details->dnd->press_y);
		if (distance_squared > RADIUS) {
			/* drag started !! */
			GdkDragAction action;
			
			if (tree_view->details->dnd->pressed_button == 3) {
				action = GDK_ACTION_ASK;
			} else {
				action = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_ASK;
			}
			gtk_drag_begin (tree_view->details->tree, 
					tree_view->details->dnd->target_list, 
					action,
					tree_view->details->dnd->pressed_button,
					(GdkEvent *) event);
		}
	} 

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "motion-notify-event");

	return TRUE;
}



















/* -----------------------------------------------------------------------
   helper functions
   -----------------------------------------------------------------------
*/

static void
nautilus_tree_view_expand_row (GtkCTree *tree, int row)
{
	GtkCTreeNode *node;
	char *node_text;
	guint8 node_spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean is_leaf;
	gboolean is_expanded;

	node = gtk_ctree_node_nth (GTK_CTREE(tree), row);
	gtk_ctree_get_node_info (GTK_CTREE(tree), 
				 node, &node_text,
				 &node_spacing, &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened, 
				 &is_leaf, &is_expanded);
	if (is_expanded == FALSE) {
				/* expand */
		gtk_ctree_expand (GTK_CTREE(tree),
				  node);
	}

}


static void
nautilus_tree_view_expand_or_collapse_row (GtkCTree *tree, int row)
{
	GtkCTreeNode *node;
	char *node_text;
	guint8 node_spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean is_leaf;
	gboolean is_expanded;

	node = gtk_ctree_node_nth (GTK_CTREE(tree), row);
	gtk_ctree_get_node_info (GTK_CTREE(tree), 
				 node, &node_text,
				 &node_spacing, &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened, 
				 &is_leaf, &is_expanded);
	if (is_expanded == FALSE) {
				/* expand */
		gtk_ctree_expand (GTK_CTREE(tree),
				  node);
	} else {
				/* collapse */ 
		gtk_ctree_collapse (GTK_CTREE(tree),
				    node);
	}

}

/* it actually also supports links */
static void
nautilus_tree_view_move_copy_files (NautilusTreeView *tree_view,
				    GList *selection_list,
				    GdkDragContext *context,
				    const char *target_uri)
{
	GList *source_uris, *p;
	
	source_uris = NULL;
	for (p = selection_list; p != NULL; p = p->next) {
		/* do a shallow copy of all the uri strings of the copied files */
		source_uris = g_list_prepend (source_uris, ((DragSelectionItem *)p->data)->uri);
	}
	source_uris = g_list_reverse (source_uris);
	
	/* start the copy */
	nautilus_file_operations_copy_move (source_uris,
					    NULL, 
					    target_uri,
					    context->action,
					    GTK_WIDGET (tree_view->details->tree));

	g_list_free (source_uris);
}



static char *
nautilus_tree_view_find_drop_target (NautilusTreeView *tree_view,
				     GList *selection_list,
				     GdkDragContext *context,
				     int x, int y)
{
	char *target_uri;
	NautilusFile *file;
	GtkCTreeNode *node;
	gboolean is_directory;
	NautilusTreeNode *current_node;
	
	node = nautilus_tree_view_tree_node_at (NAUTILUS_TREE_VIEW (tree_view), x, y);
	is_directory = nautilus_tree_view_is_tree_node_directory (NAUTILUS_TREE_VIEW (tree_view), node);

	if (is_directory == FALSE) {
		NautilusTreeNode *parent_node;
		current_node = view_node_to_model_node (tree_view, node);
		parent_node = nautilus_tree_node_get_parent (current_node);
		file = nautilus_tree_node_get_file (parent_node);
	} else {
		file = nautilus_tree_node_get_file (current_node);
	}


	if ( !nautilus_drag_can_accept_items 
	     (file, selection_list)) {	
		nautilus_file_unref (file);
		return NULL;
	}

	target_uri = nautilus_file_get_uri (file);

	nautilus_file_unref (file);

	return target_uri;
}


static gboolean
nautilus_tree_view_is_tree_node_directory (NautilusTreeView *tree_view,
					   GtkCTreeNode *node) 
{
	NautilusTreeNode *model_node;
	NautilusFile *file;
	gboolean is_directory;

	model_node = view_node_to_model_node (tree_view, node);

	file = nautilus_tree_node_get_file (model_node);

	is_directory = nautilus_file_is_directory (file);
	
	return is_directory;
}



static GtkCTreeNode *
nautilus_tree_view_tree_node_at (NautilusTreeView *tree_view,
				 int x, int y) 
{
	int row, column, on_row;
	GtkCTreeNode *node;


	on_row = gtk_clist_get_selection_info (GTK_CLIST (tree_view->details->tree), 
					       x, y, &row, &column);

	node = NULL;
	if (on_row == 1) {
		node = gtk_ctree_node_nth (GTK_CTREE (tree_view->details->tree),
					   row);
	}

	return node;
}

/**
 * nautilus_tree_view_item_at:
 * @tree_view: 
 * @x: coordinates in pixel units.
 * @y:
 *
 * Return value: uri of the node under the x/y pixel 
 *               unit coordinates in the tree. 
 *               will return NULL if not in tree.
 */
static char *
nautilus_tree_view_item_at (NautilusTreeView *tree_view,
			    int x, int y)
{
	char *retval;
	GtkCTreeNode *node;

	node = NULL;
	node = nautilus_tree_view_tree_node_at (tree_view, x, y);

	retval = NULL;
	if (node != NULL) {
		retval = g_strdup (view_node_to_uri(tree_view, node));
	}
	return retval;
}


/**
 * nautilus_tree_view_get_drag_uri:
 * @tree_view: a %NautilusTreeView.
 *
 * Return value: the uri of the object selected
 *               for drag in the tree.
 */
static char *
nautilus_tree_view_get_drag_uri  (NautilusTreeView *tree_view)
{
	return nautilus_tree_view_item_at (tree_view,
					   tree_view->details->dnd->press_x,
					   tree_view->details->dnd->press_y);
}

















/***********************************************************************
 * scrolling helper code. stolen and modified from nautilus-icon-dnd.c *
 ***********************************************************************/


static int
auto_scroll_timeout_callback (gpointer data)
{
	NautilusDragInfo *drag_info;
	NautilusTreeView *tree_view;
	GtkWidget *widget;
	float x_scroll_delta, y_scroll_delta;

	g_assert (NAUTILUS_IS_TREE_VIEW (data));
	widget = GTK_WIDGET (data);
	tree_view = NAUTILUS_TREE_VIEW (widget);
	drag_info = tree_view->details->dnd->drag_info;

	if (drag_info->waiting_to_autoscroll
		&& drag_info->start_auto_scroll_in < nautilus_get_system_time()) {
		/* not yet */
		return TRUE;
	}

	drag_info->waiting_to_autoscroll = FALSE;

	nautilus_drag_autoscroll_calculate_delta (widget, &x_scroll_delta, &y_scroll_delta);

	/* make the GtkScrolledWindow actually scroll */
	nautilus_tree_view_real_scroll (tree_view, x_scroll_delta, y_scroll_delta);

	return TRUE;
}

static void
nautilus_tree_view_start_auto_scroll (NautilusTreeView *tree_view)
{
	NautilusDragInfo *drag_info;

	g_assert (NAUTILUS_IS_TREE_VIEW (tree_view));
	drag_info = tree_view->details->dnd->drag_info;


	if (drag_info->auto_scroll_timeout_id == 0) {
		drag_info->waiting_to_autoscroll = TRUE;
		drag_info->start_auto_scroll_in = nautilus_get_system_time() 
			+ AUTOSCROLL_INITIAL_DELAY;
		drag_info->auto_scroll_timeout_id = gtk_timeout_add
				(AUTOSCROLL_TIMEOUT_INTERVAL,
				 auto_scroll_timeout_callback,
			 	 tree_view);
	}
}

static void
nautilus_tree_view_stop_auto_scroll (NautilusTreeView *tree_view)
{
	NautilusDragInfo *drag_info;

	g_assert (NAUTILUS_IS_TREE_VIEW (tree_view));
	drag_info = tree_view->details->dnd->drag_info;

	if (drag_info->auto_scroll_timeout_id) {
		gtk_timeout_remove (drag_info->auto_scroll_timeout_id);
		drag_info->auto_scroll_timeout_id = 0;
	}
}



static void
nautilus_tree_view_real_scroll (NautilusTreeView *tree_view, float delta_x, float delta_y)
{
	GtkAdjustment *hadj, *vadj;

	hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (tree_view));
	vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (tree_view));

	nautilus_gtk_adjustment_set_value (hadj, hadj->value + delta_x);
	nautilus_gtk_adjustment_set_value (vadj, vadj->value + delta_y);

}




static void
nautilus_tree_view_free_drag_data (NautilusTreeView *tree_view)
{
	NautilusDragInfo *drag_info;
	
	drag_info = tree_view->details->dnd->drag_info;
	
	drag_info->got_drop_data_type = FALSE;
#if 0	
	if (dnd_info->shadow != NULL) {
		gtk_object_destroy (GTK_OBJECT (dnd_info->shadow));
		dnd_info->shadow = NULL;
	}
#endif /* 0 */

	if (drag_info->selection_data != NULL) {
		nautilus_gtk_selection_data_free_deep (drag_info->selection_data);
		drag_info->selection_data = NULL;
	}
}
