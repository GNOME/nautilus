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
 *       Mathieu Lacage <mathieu@eazel.com> (evil dnd code)
 */

/* nautilus-tree-view.c - tree sidebar panel
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
#include <libnautilus-extensions/nautilus-drag.h>
#include <libnautilus-extensions/nautilus-file-operations.h>
#include <libgnomevfs/gnome-vfs.h>


#include <stdio.h>

/* timeout for the automatic expand in tree view */
#define EXPAND_TIMEOUT 800
#define COLLAPSE_TIMEOUT 1200

#define DISPLAY_TIMEOUT_INTERVAL_MSECS 500

#define DND_DEBUG 1

#define	ROW_ELEMENT(clist, row)	(((row) == (clist)->rows - 1) ? \
				 (clist)->row_list_end : \
				 g_list_nth ((clist)->row_list, (row)))
				 
typedef struct _NautilusTreeViewDndDetails NautilusTreeViewDndDetails;
struct _NautilusTreeViewDndDetails {

	NautilusDragInfo *drag_info;

	/* data setup by button_press signal for dragging */

	/* coordinates of the begening of a drag/press event */
	int press_x, press_y;
	/* used to remember between press/release that we pressed 
	   a hot spot. */
	gboolean pressed_hot_spot;
	/* used to remember between motion events that we are 
	   tracking a drag. */
	gboolean drag_pending;
	/* used to remmeber in motion_notify events which buton 
	   was pressed. */
	guint pressed_button;

	/* data used by the drag_motion code */
	GSList *expanded_nodes;

	/* row being highlighted */
	NautilusCTreeNode *current_prelighted_node;
	GtkStyle *normal_style;
	GtkStyle *highlight_style;
};


typedef void (*TreeViewCallback) (NautilusTreeView *view);


/* A NautilusContentView's private information. */
struct NautilusTreeViewDetails {
	NautilusView *nautilus_view;

	GtkWidget *tree;

	NautilusTreeModel *model;

	GHashTable *file_to_node_map;
	GHashTable *file_to_hack_node_map;

	gboolean show_hidden_files;

	NautilusTreeExpansionState *expansion_state;
	char *selected_uri;
	char *current_main_view_uri;

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

static NautilusCTreeNode *nautilus_tree_view_find_parent_node (NautilusTreeView *view, 
							       NautilusFile     *file);

static gboolean           ctree_is_node_expanded              (NautilusCTree     *ctree,
							       NautilusCTreeNode *node);
static NautilusCTreeNode *file_to_view_node                   (NautilusTreeView *view,
							       NautilusFile     *file);
static NautilusCTreeNode *model_node_to_view_node             (NautilusTreeView *view,
							       NautilusTreeNode *node);

static NautilusFile      *view_node_to_file                   (NautilusTreeView *view,
							       NautilusCTreeNode *node);


static void reload_node_for_file                (NautilusTreeView      *view,
						 NautilusFile          *file);
static void expand_node_for_file                (NautilusTreeView      *view,
						 NautilusFile          *file);
static void tree_load_location_callback         (NautilusView          *nautilus_view,
						 const char            *location,
						 NautilusTreeView      *view);
static void tree_expand_callback                (NautilusCTree         *tree,
						 NautilusCTreeNode     *node,
						 NautilusTreeView      *view);
static void tree_collapse_callback              (NautilusCTree         *tree,
						 NautilusCTreeNode     *node,
						 NautilusTreeView      *view);
static void tree_select_row_callback            (NautilusCTree         *tree,
						 NautilusCTreeNode     *node,
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
						       int                       x, 
						       int                       y);
static char    *nautilus_tree_view_get_drag_uri           (NautilusTreeView      *tree_view);

static void     nautilus_tree_view_expand_or_collapse_row (NautilusCTree         *tree, 
							   int                    row);
static gboolean     nautilus_tree_view_expand_node        (NautilusCTree         *tree, 
							   NautilusCTreeNode     *node);
static gboolean nautilus_tree_view_is_tree_node_directory (NautilusTreeView      *tree_view,
							   NautilusCTreeNode     *node);
static NautilusCTreeNode *nautilus_tree_view_tree_node_at (NautilusTreeView      *tree_view,
							   int                    x, 
							   int                    y);


static void    nautilus_tree_view_start_auto_scroll       (NautilusTreeView      *tree_view);

static void    nautilus_tree_view_stop_auto_scroll        (NautilusTreeView      *tree_view);

static void    nautilus_tree_view_real_scroll             (NautilusTreeView      *tree_view, 
							   float                  x_delta, 
							   float                  y_delta);

static void    nautilus_tree_view_free_drag_data          (NautilusTreeView      *tree_view);
static void    nautilus_tree_view_receive_dropped_icons   (NautilusTreeView      *view,
							   GdkDragContext        *context,
							   int                    x, 
							   int                    y);

static void  nautilus_tree_view_make_prelight_if_file_operation (NautilusTreeView *tree_view, 
								 int x, 
								 int y);
static void  nautilus_tree_view_ensure_drag_data (NautilusTreeView *tree_view,
						  GdkDragContext *context,
						  guint32 time);
static void  nautilus_tree_view_expand_maybe_later (NautilusTreeView *tree_view,
						    int x, 
						    int y,
						    gpointer user_data);
static void  nautilus_tree_view_get_drop_action (NautilusTreeView *tree_view, 
						 GdkDragContext *context,
						 int x, int y,
						 int *default_action,
						 int *non_default_action);
static void 
nautilus_tree_view_collapse_all (NautilusTreeView *tree_view,
				 NautilusCTreeNode *current_node);


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
insert_hack_node (NautilusTreeView *view, NautilusFile *file)
{
 	NautilusCTreeNode *view_node;
	NautilusCTreeNode *hack_node;
	char *text[2];

#ifdef DEBUG_TREE
	printf ("XXX: possibly adding hack node for %s\n", nautilus_file_get_uri (file));
#endif

	hack_node = g_hash_table_lookup (view->details->file_to_hack_node_map, file);

	if (hack_node == NULL) {
		text[0] = "...HACK...";
		text[1] = NULL;

#ifdef DEBUG_TREE
		printf ("XXX: actually adding hack node for %s\n", nautilus_file_get_uri (file));
#endif

		view_node = file_to_view_node (view, file);

		hack_node = nautilus_ctree_insert_node (NAUTILUS_CTREE (view->details->tree),
							view_node, 
							NULL,
							text,
							TREE_SPACING,
							NULL, NULL, NULL, NULL,
							FALSE,
							FALSE);

		g_assert (g_hash_table_lookup (view->details->file_to_hack_node_map, file) == NULL);
		nautilus_file_ref (file);
		g_hash_table_insert (view->details->file_to_hack_node_map, 
				     file, hack_node);
	}
}


static void
remove_hack_node (NautilusTreeView *view, NautilusFile *file)
{
	gpointer key, value;
	NautilusCTreeNode *hack_node;

#ifdef DEBUG_TREE
	printf ("XXX: removing hack node for %s\n", nautilus_file_get_uri (file));
#endif

	if (g_hash_table_lookup_extended (view->details->file_to_hack_node_map,
					  file, &key, &value)) {
		hack_node = value;

		nautilus_ctree_remove_node (NAUTILUS_CTREE (view->details->tree),
					    hack_node);
		g_hash_table_remove (view->details->file_to_hack_node_map, file);
		
		nautilus_file_unref (file);

		gtk_clist_thaw (GTK_CLIST (view->details->tree));

#ifdef DEBUG_TREE
		printf ("XXX: actually thawing (%d)\n", GTK_CLIST (view->details->tree)->freeze_count);
#endif
	}
}


static void
freeze_if_have_hack_node (NautilusTreeView *view, NautilusFile *file)
{
	NautilusCTreeNode *hack_node;

#ifdef DEBUG_TREE
	puts ("XXX: freezing if hack node");
#endif

	hack_node = g_hash_table_lookup (view->details->file_to_hack_node_map, file);

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
	NautilusCTreeNode *parent_view_node;
 	NautilusCTreeNode *view_node;
	NautilusFile *file;
	NautilusFile *parent_file;
	char *uri;

	char *text[2];
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	file = nautilus_tree_node_get_file (node);

	if (nautilus_tree_view_should_skip_file (view, file)) {
		return;
	}

#ifdef DEBUG_TREE
	printf ("Inserting URI into tree: %s\n", nautilus_file_get_uri (file));
#endif

	parent_view_node = nautilus_tree_view_find_parent_node (view, file);


#ifdef DEBUG_TREE
	printf ("parent_view_node 0x%x (%s)\n", (unsigned) parent_view_node, 
		nautilus_file_get_uri (view_node_to_file (view, node)));
#endif


	text[0] = nautilus_file_get_name (file);
	text[1] = NULL;

	if (model_node_to_view_node (view, node) == NULL) {
		
		nautilus_icon_factory_get_pixmap_and_mask_for_file (file,
								    NULL,
								    NAUTILUS_ICON_SIZE_FOR_MENUS,
								    &pixmap,
								    &mask);

		view_node = nautilus_ctree_insert_node (NAUTILUS_CTREE (view->details->tree),
						   parent_view_node, 
						   NULL,
						   text,
						   TREE_SPACING,
						   pixmap, mask, pixmap, mask,
						   FALSE,
						   FALSE);

		gdk_pixmap_unref (pixmap);
		if (mask != NULL) {
			gdk_bitmap_unref (mask);
		}

		
		
		nautilus_ctree_node_set_row_data (NAUTILUS_CTREE (view->details->tree),
						  view_node,
						  node);

		g_assert (g_hash_table_lookup (view->details->file_to_node_map, file) == NULL);

		nautilus_file_ref (file);
		g_hash_table_insert (view->details->file_to_node_map, file, view_node); 
		
		if (nautilus_file_is_directory (file)) {
			/* Gratuitous hack so node can be expandable w/o
			   immediately inserting all the real children. */

			uri = nautilus_file_get_uri (file);
			if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
				nautilus_ctree_expand (NAUTILUS_CTREE (NAUTILUS_CTREE (view->details->tree)),
						  view_node);
			} else {
				insert_hack_node (view, file);
			}
			
			g_free (uri);
		}
	}

	g_free (text[0]);

	if (parent_view_node != NULL) {
		parent_file = view_node_to_file (view, parent_view_node);
		remove_hack_node (view, parent_file);
	}
}

static void
forget_view_node (NautilusTreeView *view,
		  NautilusCTreeNode *view_node)
{
	NautilusFile *file;
	char *uri;

	file = view_node_to_file (view, view_node);

	/* We get NULL when we visit hack nodes, and we visit the hack
	 * nodes before we remove them.
	 */
	if (file == NULL) {
		return;
	}

	remove_hack_node (view, file);

	uri = nautilus_file_get_uri (file);
	nautilus_tree_expansion_state_remove_node
		(view->details->expansion_state, uri);
	g_free (uri);

	g_hash_table_remove (view->details->file_to_node_map, file);
	nautilus_file_unref (file);
}

static void
forget_view_node_and_children (NautilusTreeView *view,
			       NautilusCTreeNode *view_node)
{
	NautilusCTreeNode *child;

	for (child = NAUTILUS_CTREE_ROW (view_node)->children;
	     child != NULL;
	     child = NAUTILUS_CTREE_ROW (child)->sibling) {
		forget_view_node_and_children (view, child);
	}

	forget_view_node (view, view_node);
}

static void
nautilus_tree_view_remove_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	NautilusCTreeNode *view_node;
	
	nautilus_tree_model_stop_monitoring_node (view->details->model, node, view);

	view_node = model_node_to_view_node (view, node);
	if (view_node != NULL) {
		forget_view_node_and_children (view, view_node);
		nautilus_ctree_remove_node (NAUTILUS_CTREE (view->details->tree),
					    view_node);
	}
}




static gboolean
ctree_is_node_expanded (NautilusCTree     *ctree,
			NautilusCTreeNode *node)
{
	gchar     *text;
	guint8     spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean   is_leaf;
	gboolean   expanded;

	nautilus_ctree_get_node_info (ctree, node,
				 &text, &spacing,
				 &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened,
				 &is_leaf, &expanded);
	return expanded;
}

static void
nautilus_tree_view_update_model_node (NautilusTreeView *view, NautilusTreeNode *node)
{
	NautilusCTreeNode *view_node;
	NautilusFile *file;
	char *uri;
	char *name;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	file = nautilus_tree_node_get_file (node);

	view_node = model_node_to_view_node (view, node);

	if (view_node != NULL) {

		name = nautilus_file_get_name (file);
	
		nautilus_icon_factory_get_pixmap_and_mask_for_file (file,
								    NULL,
								    NAUTILUS_ICON_SIZE_FOR_MENUS,
								    &pixmap,
								    &mask);
		
		nautilus_ctree_node_set_pixtext (NAUTILUS_CTREE (view->details->tree),
						 view_node,
						 0,
						 name,
						 TREE_SPACING,
						 pixmap,
						 mask);

		gdk_pixmap_unref (pixmap);
		if (mask != NULL) {
			gdk_bitmap_unref (mask);
		}

		g_free (name);
		
#if 0
		/* FIXME bugzilla.eazel.com 2421: 
		 * should switch to this call so we can set open/closed pixamps */
		void nautilus_ctree_set_node_info  (NautilusCTree     *ctree,
					       NautilusCTreeNode *node,
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
			uri = nautilus_file_get_uri (file);

			if (nautilus_tree_expansion_state_is_node_expanded (view->details->expansion_state, uri)) {
				if (!ctree_is_node_expanded (NAUTILUS_CTREE (view->details->tree),
							     view_node)) {
					nautilus_ctree_expand (NAUTILUS_CTREE (view->details->tree),
							       view_node);
				} 
			} else {
				if (ctree_is_node_expanded (NAUTILUS_CTREE (view->details->tree),
							    view_node)) {
					nautilus_ctree_collapse (NAUTILUS_CTREE (view->details->tree),
								 view_node);
				} else {
					insert_hack_node (view, file);
				}
			}

			g_free (uri);
		}
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

	uri = nautilus_file_get_uri (nautilus_tree_node_get_file (node));
	
	if (nautilus_strcmp (uri, view->details->wait_uri) == 0) {
		callback = view->details->uri_loaded_or_parent_done_loading;
		view->details->wait_node = NULL;
		g_free (view->details->wait_uri);
		view->details->wait_uri = NULL;

		(*callback) (view);
	}
	
	g_free (uri);
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

#ifdef DEBUG_TREE
	printf ("XXX: changed %s\n", uri);
#endif

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

	g_return_if_fail (NAUTILUS_IS_TREE_MODEL (model));
	g_return_if_fail (NAUTILUS_IS_TREE_NODE (node));

#ifdef DEBUG_TREE
	puts ("XXX: done loading");
#endif

	view = NAUTILUS_TREE_VIEW (callback_data);

	remove_hack_node (view, nautilus_tree_node_get_file (node));

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
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST }
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
        gtk_style_ref (new_style);

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
	view->details->dnd->expanded_nodes = NULL;

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
	view->details->tree = nautilus_ctree_new (1, 0);

	gtk_object_set_data (GTK_OBJECT(view->details->tree), "tree_view", (gpointer) view);
	gtk_widget_add_events (GTK_WIDGET(view->details->tree), GDK_POINTER_MOTION_MASK);
	
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

	
        gtk_clist_set_selection_mode (GTK_CLIST (view->details->tree), GTK_SELECTION_SINGLE);
	gtk_clist_set_auto_sort (GTK_CLIST (view->details->tree), TRUE);
	gtk_clist_set_sort_type (GTK_CLIST (view->details->tree), GTK_SORT_ASCENDING);
	gtk_clist_set_column_auto_resize (GTK_CLIST (view->details->tree), 0, TRUE);
	gtk_clist_columns_autosize (GTK_CLIST (view->details->tree));

	gtk_clist_set_reorderable (GTK_CLIST (view->details->tree), FALSE);

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

	view->details->file_to_node_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	view->details->file_to_hack_node_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	
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
free_file_to_node_map_entry (gpointer key, gpointer value, gpointer callback_data)
{
	g_assert (callback_data == NULL);

	nautilus_file_unref (NAUTILUS_FILE (key));
}

static void
free_file_to_hack_node_map_entry (gpointer key, gpointer value, gpointer callback_data)
{
	g_assert (callback_data == NULL);

	nautilus_file_unref (NAUTILUS_FILE (key));
}

static void
nautilus_tree_view_destroy (GtkObject *object)
{
	NautilusTreeView *view;
	
	view = NAUTILUS_TREE_VIEW (object);

	g_hash_table_foreach (view->details->file_to_node_map,
			      free_file_to_node_map_entry,
			      NULL);
	g_hash_table_destroy (view->details->file_to_node_map);
	
	g_hash_table_foreach (view->details->file_to_hack_node_map,
			      free_file_to_hack_node_map_entry,
			      NULL);
	g_hash_table_destroy (view->details->file_to_hack_node_map);
	
	/* you do not need to unref the normal style */
	if (view->details->dnd->highlight_style != NULL) {
		gtk_style_unref (view->details->dnd->highlight_style);
	}
	nautilus_drag_finalize (view->details->dnd->drag_info);
	g_free (view->details->dnd);
	/* FIXME bugzilla.eazel.com 2422: destroy drag_info */

	disconnect_model_handlers (view);
	gtk_object_unref (GTK_OBJECT (view->details->model));

	nautilus_tree_expansion_state_save (view->details->expansion_state);
	gtk_object_unref (GTK_OBJECT (view->details->expansion_state));

	g_free (view->details->current_main_view_uri);
	g_free (view->details->selected_uri);

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


static NautilusCTreeNode *
file_to_view_node (NautilusTreeView *view,
		   NautilusFile     *file)
{
	return g_hash_table_lookup (view->details->file_to_node_map, file);
}


static NautilusCTreeNode *
model_node_to_view_node (NautilusTreeView *view,
			 NautilusTreeNode *node)
{
	NautilusCTreeNode *view_node;
	NautilusFile *file;

	if (node == NULL) {
		return NULL;
	}

	file = nautilus_tree_node_get_file (node);
	view_node = file_to_view_node (view, file);

	return view_node;
}

static NautilusTreeNode *
view_node_to_model_node (NautilusTreeView *view,
			 NautilusCTreeNode *node)
{
	NautilusTreeNode *tree_node;

	tree_node = (NautilusTreeNode *) nautilus_ctree_node_get_row_data (NAUTILUS_CTREE (view->details->tree),
									   node);

	return tree_node;
}

static NautilusFile *
view_node_to_file (NautilusTreeView *view,
		   NautilusCTreeNode *node)
{
	NautilusTreeNode *tree_node;

	tree_node = view_node_to_model_node (view, node);

	if (tree_node == NULL) {
		return NULL;
	}

	return nautilus_tree_node_get_file (tree_node);
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
	NautilusCTreeNode *view_node;
	gboolean at_least_one_found;
	NautilusFile *file;

	at_least_one_found = FALSE;
	uri = NULL;

	if (!view->details->root_seen) {
		call_when_root_seen (view, expand_uri_sequence_and_select_end);
		return;
	}

	for (p = view->details->in_progress_select_uris; p != NULL; p = p->next) {
		uri = (char *) p->data;

		file = nautilus_file_get (uri);
		view_node = file_to_view_node (view, file);
		nautilus_file_unref (file);

		if (view_node == NULL) {
			break;
		}

		at_least_one_found = TRUE;

		if (p->next != NULL) {
			nautilus_ctree_expand (NAUTILUS_CTREE (view->details->tree),
					  view_node);
		} else {
			g_free (view->details->selected_uri);
			view->details->selected_uri = g_strdup (uri);
			nautilus_ctree_select (NAUTILUS_CTREE (view->details->tree),
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

	g_free (view->details->current_main_view_uri);
	view->details->current_main_view_uri = g_strdup (canonical_uri);

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
reload_node_for_file (NautilusTreeView *view,
		      NautilusFile     *file)
{
	GList *p;
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node_from_file (view->details->model, file);
	g_return_if_fail (node != NULL);

	nautilus_tree_model_monitor_node (view->details->model, node, view);

	for (p = nautilus_tree_node_get_children (node); p != NULL; p = p->next) {
		nautilus_tree_view_update_model_node (view, (NautilusTreeNode *) p->data);
	}
}

static void
expand_node_for_file (NautilusTreeView *view,
		      NautilusFile      *file)
{
	char *uri;

	freeze_if_have_hack_node (view, file);

	uri = nautilus_file_get_uri (file);
	nautilus_tree_expansion_state_expand_node (view->details->expansion_state,
						   uri);
	g_free (uri);

	reload_node_for_file (view, file);
}

static void
tree_expand_callback (NautilusCTree         *ctree,
		      NautilusCTreeNode     *node,
		      NautilusTreeView      *view)
{
	NautilusFile *file;

	file = view_node_to_file (view, node);

	expand_node_for_file (view, file);
}



static void
tree_collapse_callback (NautilusCTree         *ctree,
			NautilusCTreeNode     *node,
			NautilusTreeView      *view)
{
	char *uri;
	
	uri = nautilus_file_get_uri (view_node_to_file (view, node));
	nautilus_tree_expansion_state_collapse_node (view->details->expansion_state,
						     uri);
	g_free (uri);
	
	nautilus_tree_model_stop_monitoring_node_recursive (view->details->model,
							    view_node_to_model_node (view, node),
							    view);
}




static void
tree_select_row_callback (NautilusCTree              *tree,
			  NautilusCTreeNode          *node,
			  gint                   column,
			  NautilusTreeView      *view)
{
	char *uri;
	
	uri = nautilus_file_get_uri (view_node_to_file (view, node));
	
	if (uri != NULL &&
	    nautilus_strcmp (view->details->current_main_view_uri, uri) != 0) {
		nautilus_view_open_location (NAUTILUS_VIEW (view->details->nautilus_view), uri);
		
		g_free (view->details->selected_uri);
		view->details->selected_uri = g_strdup (uri);
	}

	g_free (uri);

}


static void 
nautilus_tree_view_update_all_icons (NautilusTreeView *view)
{
	nautilus_tree_model_monitor_add (view->details->model,
					 view,
					 nautilus_tree_view_model_node_changed_callback,
					 view);
}


static NautilusCTreeNode *
nautilus_tree_view_find_parent_node (NautilusTreeView *view, 
				     NautilusFile     *file)
{
	NautilusTreeNode *node;

	node = nautilus_tree_model_get_node_from_file (view->details->model, 
						       file);

	if (node == NULL) {
		g_print ("You've run into an intermittent tree view bug.\n");
		g_print ("Running Nautilus again will probably not hit this bug.\n");
		g_print ("The tree view didn't have a node for %s\n", nautilus_file_get_uri (file));
		g_print ("The tree view had the following nodes:\n\n");
		nautilus_tree_model_dump_files (view->details->model);

		/* NOW die. */
		g_assert_not_reached ();
	}

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

	dnd->drag_info->got_drop_data_type = FALSE;
}

static void
nautilus_tree_view_drag_end (GtkWidget *widget, GdkDragContext *context,
			     gpointer user_data)
{
	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_end");

}


/* FIXME:
   drag_leave is emitted when you leave the area _OR_ when you do a drop.
   So, the thing is when you drop, since we are now in a in-process case, 
   the _leave signal is emited before the final drag_data_received is 
   actually called. This is _bad_.
   I need to proofread the code to make sure nothing really strange 
   happen here 
   -- Mathieu
*/

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
		nautilus_ctree_node_set_row_style (NAUTILUS_CTREE (tree_view->details->tree), 
					      dnd->current_prelighted_node,
					      tree_view->details->dnd->normal_style);
	}
	dnd->current_prelighted_node = NULL;

	/* stop autoscroll */
	nautilus_tree_view_stop_auto_scroll (tree_view);

	gtk_signal_emit_stop_by_name (GTK_OBJECT (widget),
				      "drag_leave");
}

static gboolean 
nautilus_tree_view_drag_motion (GtkWidget *widget, GdkDragContext *context,
				int x, int y, guint time, gpointer user_data)
{
	NautilusTreeView *tree_view;
	NautilusTreeViewDndDetails *dnd;
	NautilusDragInfo *drag_info;

	int resulting_action, default_action, non_default_action;

	tree_view = NAUTILUS_TREE_VIEW (user_data);
	dnd = (NautilusTreeViewDndDetails *) (tree_view->details->dnd);
	drag_info = dnd->drag_info;

	/* get the data from the other side of the dnd */
	nautilus_tree_view_ensure_drag_data (tree_view, context, time);	


	/* prelight depending on the type of drop. */
	if (drag_info->got_drop_data_type) {
		switch (drag_info->data_type) {
		case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		case NAUTILUS_ICON_DND_URI_LIST:
			nautilus_tree_view_expand_maybe_later (NAUTILUS_TREE_VIEW (tree_view), 
							       x, y, user_data);
			nautilus_tree_view_make_prelight_if_file_operation (NAUTILUS_TREE_VIEW (tree_view), 
									    x, y);
			break;
		case NAUTILUS_ICON_DND_KEYWORD:	
		case NAUTILUS_ICON_DND_COLOR:
		case NAUTILUS_ICON_DND_BGIMAGE:	
		default:
			break;
		}
	}

	/* auto scroll */
	nautilus_tree_view_start_auto_scroll (tree_view);

	/* update dragging cursor. */
	nautilus_tree_view_get_drop_action  (tree_view, context, x, y, 
					     &default_action, 
					     &non_default_action);
	resulting_action = nautilus_drag_modifier_based_action (default_action,
								non_default_action);
	gdk_drag_status (context, resulting_action, time);



	/* make sure no one will ever get this event except us */
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

	if (!drag_info->got_drop_data_type) {
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
		case NAUTILUS_ICON_DND_BGIMAGE:	
		case NAUTILUS_ICON_DND_KEYWORD:	
		default:
			/* we do not want to support any of the 3 above */
			break;
		}
	} else if (drag_info->drop_occured) {
		/* drop occured: do actual operations on the data */
		switch (info) {
		case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
		case NAUTILUS_ICON_DND_URI_LIST:
			nautilus_tree_view_receive_dropped_icons
				(NAUTILUS_TREE_VIEW (tree_view),
				 context, x, y);
			gtk_drag_finish (context, TRUE, FALSE, time);
			break;
		case NAUTILUS_ICON_DND_COLOR:
		case NAUTILUS_ICON_DND_BGIMAGE:
		case NAUTILUS_ICON_DND_KEYWORD:
		default:
			gtk_drag_finish (context, FALSE, FALSE, time);
		}
		
		nautilus_tree_view_free_drag_data (NAUTILUS_TREE_VIEW (tree_view));

		/* reinitialise it for the next dnd */
		drag_info->drop_occured = FALSE;
		drag_info->got_drop_data_type = FALSE;
		g_slist_free (dnd->expanded_nodes);
		dnd->expanded_nodes = NULL;
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

	if (nautilus_ctree_is_hot_spot (NAUTILUS_CTREE (widget), event->x, event->y)) {		
		NautilusCTreeRow *ctree_row;
		NautilusCTreeNode *node;
		
		tree_view->details->dnd->press_x = event->x;
		tree_view->details->dnd->press_y = event->y;
		tree_view->details->dnd->pressed_button = event->button;
		tree_view->details->dnd->pressed_hot_spot = TRUE;

		/* Clicking in the expander should not start a drag */
		tree_view->details->dnd->drag_pending = FALSE;
	
		ctree_row = ROW_ELEMENT (clist, press_row)->data;
		if (ctree_row != NULL) {
			ctree_row->mouse_down = TRUE;
			ctree_row->in_hotspot = TRUE;

			node = nautilus_ctree_find_node_ptr (NAUTILUS_CTREE (widget), ctree_row);
			if (node != NULL) {
				nautilus_ctree_draw_node (NAUTILUS_CTREE (widget), node);
			}
		}
	} else {
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
	int press_row, press_column;
	NautilusCTreeRow *ctree_row;
	NautilusCTreeNode *node;

	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return retval;

	tree_view = NAUTILUS_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (widget), "tree_view"));
	tree_view->details->dnd->drag_pending = FALSE;
	
	/* Set state of spinner.  Use saved dnd x and y as the mouse may have moved out
	 * of the original row */	
	on_row = gtk_clist_get_selection_info (GTK_CLIST (tree_view->details->tree), 
					       tree_view->details->dnd->press_x, 
					       tree_view->details->dnd->press_y, 
					       &press_row, &press_column);	
	ctree_row = ROW_ELEMENT (clist, press_row)->data;
	if (ctree_row != NULL) {
		ctree_row->mouse_down = FALSE;
		ctree_row->in_hotspot = FALSE;

		/* Redraw spinner */
		node = nautilus_ctree_find_node_ptr (NAUTILUS_CTREE (widget), ctree_row);
		if (node != NULL) {
			nautilus_ctree_draw_node (NAUTILUS_CTREE (widget), node);
		}
	}

	distance_squared = (event->x - tree_view->details->dnd->press_x)
		* (event->x - tree_view->details->dnd->press_x) +
		(event->y - tree_view->details->dnd->press_y)
		* (event->y - tree_view->details->dnd->press_y);
	is_still_hot_spot = nautilus_ctree_is_hot_spot (NAUTILUS_CTREE(tree_view->details->tree), 
						   event->x, event->y);
	
	on_row = gtk_clist_get_selection_info (GTK_CLIST (tree_view->details->tree), 
					       event->x, 
					       event->y, 
					       &release_row, &release_column);

	if (on_row == 1) {

		if (distance_squared <= RADIUS 
		    && tree_view->details->dnd->pressed_hot_spot
		    && is_still_hot_spot) {
			
			tree_view->details->dnd->pressed_hot_spot = FALSE;
			
			nautilus_tree_view_expand_or_collapse_row (NAUTILUS_CTREE(tree_view->details->tree), 
								   release_row);
		} else if (distance_squared <= RADIUS) {
			/* we are close from the place we clicked */
			/* select current row */

			/* Only button 1 triggers a selection */
			if (event->button == 1) {
				gtk_clist_select_row (GTK_CLIST (tree_view->details->tree), 
						      release_row, release_column); 
			}
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

	if (tree_view->details->dnd->drag_pending) {
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
					tree_view->details->dnd->drag_info->target_list, 
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
nautilus_tree_view_make_prelight_if_file_operation (NautilusTreeView *tree_view, 
						    int x, int y)
{
	NautilusTreeViewDndDetails *dnd;
	NautilusCTreeNode *node;
	gboolean is_directory;

	g_assert (NAUTILUS_IS_TREE_VIEW (tree_view));
	dnd = tree_view->details->dnd;

	/* make the thing under us prelighted. */
	node = nautilus_tree_view_tree_node_at (NAUTILUS_TREE_VIEW (tree_view), x, y);
	if (node == NULL) {
		return;
	}
	is_directory = nautilus_tree_view_is_tree_node_directory (NAUTILUS_TREE_VIEW (tree_view), node);

	if (!is_directory) {
		NautilusTreeNode *parent_node, *current_node;
		current_node = view_node_to_model_node (tree_view, node);
		parent_node = nautilus_tree_node_get_parent (current_node);
		node = model_node_to_view_node (tree_view, parent_node);
	} 

	if (node != dnd->current_prelighted_node 
	    && dnd->current_prelighted_node != NULL) {
		nautilus_ctree_node_set_row_style (NAUTILUS_CTREE (tree_view->details->tree), 
					      dnd->current_prelighted_node,
					      tree_view->details->dnd->normal_style);
	}
	nautilus_ctree_node_set_row_style (NAUTILUS_CTREE (tree_view->details->tree), 
				      node,
				      tree_view->details->dnd->highlight_style);
	dnd->current_prelighted_node = node;

}



/* return if it was expanded or not */
static gboolean
nautilus_tree_view_expand_node (NautilusCTree *tree, NautilusCTreeNode *node)
{
	char *node_text;
	guint8 node_spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean is_leaf;
	gboolean is_expanded;

	nautilus_ctree_get_node_info (NAUTILUS_CTREE(tree), 
				 node, &node_text,
				 &node_spacing, &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened, 
				 &is_leaf, &is_expanded);
	if (!is_expanded) {
				/* expand */
		nautilus_ctree_expand (NAUTILUS_CTREE(tree),
				  node);
	}

	return is_expanded;

}

/* returns if it was expanded or not */
static gboolean
nautilus_tree_view_collapse_node (NautilusCTree *tree, NautilusCTreeNode *node)
{
	char *node_text;
	guint8 node_spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean is_leaf;
	gboolean is_expanded;

	nautilus_ctree_get_node_info (NAUTILUS_CTREE(tree), 
				      node, &node_text,
				      &node_spacing, &pixmap_closed, &mask_closed,
				      &pixmap_opened, &mask_opened, 
				      &is_leaf, &is_expanded);
	if (is_expanded) {
				/* collapse */
		nautilus_ctree_collapse (NAUTILUS_CTREE(tree),
					 node);
	}

	return is_expanded;
}

static gboolean
nautilus_tree_view_is_tree_node_expanded (NautilusCTree *tree, NautilusCTreeNode *node)
{
	char *node_text;
	guint8 node_spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean is_leaf;
	gboolean is_expanded;

	nautilus_ctree_get_node_info (NAUTILUS_CTREE(tree), 
				 node, &node_text,
				 &node_spacing, &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened, 
				 &is_leaf, &is_expanded);

	return is_expanded;
}

static void
nautilus_tree_view_expand_or_collapse_row (NautilusCTree *tree, int row)
{
	NautilusCTreeNode *node;
	char *node_text;
	guint8 node_spacing;
	GdkPixmap *pixmap_closed;
	GdkBitmap *mask_closed;
	GdkPixmap *pixmap_opened;
	GdkBitmap *mask_opened;
	gboolean is_leaf;
	gboolean is_expanded;

	node = nautilus_ctree_node_nth (NAUTILUS_CTREE(tree), row);
	nautilus_ctree_get_node_info (NAUTILUS_CTREE(tree), 
				 node, &node_text,
				 &node_spacing, &pixmap_closed, &mask_closed,
				 &pixmap_opened, &mask_opened, 
				 &is_leaf, &is_expanded);
	if (!is_expanded) {
				/* expand */
		nautilus_ctree_expand (NAUTILUS_CTREE(tree),
				  node);
	} else {
				/* collapse */ 
		nautilus_ctree_collapse (NAUTILUS_CTREE(tree),
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
					    GTK_WIDGET (tree_view->details->tree),
					    NULL, NULL);

	g_list_free (source_uris);
}



static char *
nautilus_tree_view_find_drop_target (NautilusTreeView *tree_view,
				     int x, int y)
{
	char *target_uri;
	NautilusFile *file;
	NautilusCTreeNode *node;
	gboolean is_directory;
	NautilusTreeNode *current_node;
	
	node = nautilus_tree_view_tree_node_at (NAUTILUS_TREE_VIEW (tree_view), x, y);
	if (node == NULL) {
		return NULL;
	}
	is_directory = nautilus_tree_view_is_tree_node_directory (NAUTILUS_TREE_VIEW (tree_view), node);

	current_node = view_node_to_model_node (tree_view, node);

	if (!is_directory) {
		NautilusTreeNode *parent_node;
		parent_node = nautilus_tree_node_get_parent (current_node);
		file = nautilus_tree_node_get_file (parent_node);
	} else {
		file = nautilus_tree_node_get_file (current_node);
	}

	target_uri = nautilus_file_get_uri (file);

	return target_uri;
}


static gboolean
nautilus_tree_view_is_tree_node_directory (NautilusTreeView *tree_view,
					   NautilusCTreeNode *node) 
{
	NautilusTreeNode *model_node;
	NautilusFile *file;
	gboolean is_directory;

	model_node = view_node_to_model_node (tree_view, node);

	file = nautilus_tree_node_get_file (model_node);

	is_directory = nautilus_file_is_directory (file);
	
	return is_directory;
}



static NautilusCTreeNode *
nautilus_tree_view_tree_node_at (NautilusTreeView *tree_view,
				 int x, int y) 
{
	int row, column, on_row;
	NautilusCTreeNode *node;


	on_row = gtk_clist_get_selection_info (GTK_CLIST (tree_view->details->tree), 
					       x, y, &row, &column);

	node = NULL;
	if (on_row == 1) {
		node = nautilus_ctree_node_nth (NAUTILUS_CTREE (tree_view->details->tree),
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
	NautilusCTreeNode *node;

	node = nautilus_tree_view_tree_node_at (tree_view, x, y);
	if (node == NULL) {
		return NULL;
	}

	return nautilus_file_get_uri (view_node_to_file (tree_view, node));
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

static void
nautilus_tree_view_ensure_drag_data (NautilusTreeView *tree_view,
				     GdkDragContext *context,
				     guint32 time)
{
	NautilusDragInfo *drag_info;

	drag_info = tree_view->details->dnd->drag_info;

	if (!drag_info->got_drop_data_type) {
		gtk_drag_get_data (GTK_WIDGET (tree_view->details->tree), context,
				   GPOINTER_TO_INT (context->targets->data),
				   time);
	}
}



/* hacks for automatic expand/collapse of tree view */
typedef struct _NautilusTreeViewExpandHack NautilusTreeViewExpandHack;
struct _NautilusTreeViewExpandHack {
	NautilusCTree *ctree;
	NautilusTreeView *tree_view;
	gboolean is_valid;
	int org_x;
	int org_y;
	int refcount;
};


static int    expand_time_callback (gpointer data);
static void   expand_hack_unref (NautilusTreeViewExpandHack * expand_hack); 
static NautilusTreeViewExpandHack    *expand_hack_new (int x, int y, NautilusTreeView *tree_view);

static char *
nautilus_dump_info (NautilusTreeView *tree_view)
{
	char *retval, *temp;
	GSList *list, *tmp;
	NautilusCTreeNode *node;
	char *uri;

	list = tree_view->details->dnd->expanded_nodes;

	retval = NULL;
	for (tmp = list; tmp != NULL;tmp = tmp->next) {
		node = (NautilusCTreeNode *) tmp->data;
		uri = nautilus_file_get_uri (view_node_to_file (tree_view, 
								node));
		temp = g_strconcat (uri, ", ", retval, NULL);
		g_free (uri);
		g_free (retval);
		retval = temp;
	}

	return retval;
}

static void
expand_hack_unref (NautilusTreeViewExpandHack * expand_hack) 
{
	expand_hack->refcount--;

	if (expand_hack->refcount == 0) {
		g_free (expand_hack);
	}

}

static NautilusTreeViewExpandHack * 
expand_hack_new (int x, int y, NautilusTreeView *tree_view)
{
	NautilusTreeViewExpandHack *expand_hack;

	expand_hack = g_new0 (NautilusTreeViewExpandHack, 1);
	expand_hack->is_valid = TRUE;
	expand_hack->org_x = x;
	expand_hack->org_y = y;
	expand_hack->ctree = (NautilusCTree *)tree_view->details->tree;
	expand_hack->tree_view = tree_view;
	expand_hack->refcount = 2;

	g_timeout_add (EXPAND_TIMEOUT, expand_time_callback, expand_hack);

	return expand_hack;
}

static gint
expand_time_callback (gpointer data)
{
	NautilusTreeViewExpandHack *expand_hack;

	expand_hack = (NautilusTreeViewExpandHack *) data;

	if (expand_hack->is_valid) {
		NautilusCTreeNode *current_node;
		gboolean was_expanded;

		current_node = nautilus_tree_view_tree_node_at (expand_hack->tree_view, 
								expand_hack->org_x, 
								expand_hack->org_y);
		if (current_node == NULL) {
			expand_hack_unref (expand_hack);
			return FALSE;
		}
		was_expanded = nautilus_tree_view_expand_node (NAUTILUS_CTREE (expand_hack->ctree), 
							       current_node);
		if (!was_expanded) {
			GSList *list;
			list = expand_hack->tree_view->details->dnd->expanded_nodes;
			expand_hack->tree_view->details->dnd->expanded_nodes = 
				g_slist_prepend (list, current_node);
			nautilus_dump_info (expand_hack->tree_view);
		}

	}
	expand_hack_unref (expand_hack);

	/* never be called again. EVER */
	return FALSE;
}








static void
nautilus_tree_view_expand_maybe_later (NautilusTreeView *tree_view,
				       int x, int y, gpointer user_data)
{
	static NautilusTreeViewExpandHack *expand_hack = NULL;
	NautilusCTreeNode *current_node;
	gboolean is_directory, is_expanded;
	
	current_node = nautilus_tree_view_tree_node_at (tree_view, 
							x, y);
	if (current_node == NULL) {
		return;
	}
	if (expand_hack == NULL) {
		expand_hack = expand_hack_new (x, y, tree_view);
	}

	is_directory = nautilus_tree_view_is_tree_node_directory (tree_view, current_node);
	is_expanded = nautilus_tree_view_is_tree_node_expanded (NAUTILUS_CTREE (tree_view->details->tree), current_node);

	/* try to expand */
	if (is_directory && !is_expanded) {
		int squared_distance;
		squared_distance = (abs(x - expand_hack->org_x)) + (abs(y - expand_hack->org_y));

		if (squared_distance > 8) {
			expand_hack->is_valid = FALSE;
			expand_hack_unref (expand_hack);
			expand_hack = expand_hack_new (x, y, tree_view);
		}
	}
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

	nautilus_drag_autoscroll_calculate_delta (tree_view->details->tree, &x_scroll_delta, &y_scroll_delta);

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










/******************************************
 * Handle the data dropped on the tree view 
 ******************************************/

static void
nautilus_tree_view_get_drop_action (NautilusTreeView *tree_view, 
				    GdkDragContext *context,
				    int x, int y,
				    int *default_action,
				    int *non_default_action)
{
	NautilusDragInfo *drag_info;
	char *drop_target;

	drag_info = NAUTILUS_TREE_VIEW (tree_view)->details->dnd->drag_info;

	/* FIXME bugzilla.eazel.com 2569: Too much code copied from nautilus-icon-dnd.c. Need to share more. */

	if (!drag_info->got_drop_data_type) {
		/* drag_data_received didn't get called yet */
		return;
	}


	switch (drag_info->data_type) {
	case NAUTILUS_ICON_DND_GNOME_ICON_LIST:
	case NAUTILUS_ICON_DND_URI_LIST:
		if (drag_info->selection_list == NULL) {
			*default_action = 0;
			*non_default_action = 0;
			return;
		}

		/* FIXME bugzilla.eazel.com 2571: */ 
		drop_target = nautilus_tree_view_find_drop_target (tree_view, x, y);
		if (!drop_target) {
			*default_action = 0;
			*non_default_action = 0;
			return;
		}
		nautilus_drag_default_drop_action_for_icons (context, drop_target, 
							     drag_info->selection_list, 
							     default_action, 
							     non_default_action);
		break;
	case NAUTILUS_ICON_DND_COLOR:
	case NAUTILUS_ICON_DND_KEYWORD:	
	case NAUTILUS_ICON_DND_BGIMAGE:	
		/* we handle none of the above */
		*default_action = context->suggested_action;
		*non_default_action = context->suggested_action;
		break;

	default:
	}

}			       

static void 
nautilus_tree_view_collapse_all (NautilusTreeView *tree_view,
				 NautilusCTreeNode *current_node)
{
	GSList *list, *temp_list;

	list = tree_view->details->dnd->expanded_nodes;

	for (temp_list = list; temp_list != NULL; temp_list = temp_list->next) {
		NautilusCTreeNode *expanded_node;
		expanded_node = (NautilusCTreeNode *) temp_list->data;
		if (!nautilus_ctree_is_ancestor (NAUTILUS_CTREE (tree_view->details->tree), 
						 expanded_node, current_node)) {
#if 0
			{
				char *expanded_uri, *current_uri;
				expanded_uri = nautilus_file_get_uri 
					(view_node_to_file (tree_view, expanded_node));
				current_uri = nautilus_file_get_uri 
					(view_node_to_file (tree_view, current_node));

				g_print ("collapsing %s in %s\n", expanded_uri, current_uri);
				g_free (expanded_uri);
				g_free (current_uri);
			}
#endif
			nautilus_tree_view_collapse_node (NAUTILUS_CTREE (tree_view->details->tree), 
							  expanded_node);
		}
	}
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
	NautilusCTreeNode *dropped_node;

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
			/* collapse all expanded directories during drag except the one we 
			   droped into */
			dropped_node = nautilus_tree_view_tree_node_at (view, x, y);
			nautilus_tree_view_collapse_all (view, dropped_node);

		}
		g_free (drop_target_uri);
		nautilus_drag_destroy_selection_list (drag_info->selection_list);
		drag_info->selection_list = NULL;
	}
}


