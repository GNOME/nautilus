/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.c - implementation of list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>
   
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
	    David Emory Watson <dwatson@cs.ucr.edu>
*/

#include <config.h>
#include "fm-list-view.h"

#include <string.h>
#include "fm-error-reporting.h"
#include "fm-list-model.h"
#include <string.h>
#include <eel/eel-cell-renderer-pixbuf-list.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <libegg/eggtreemultidnd.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extension/nautilus-column-provider.h>
#include <libnautilus-private/nautilus-column-chooser.h>
#include <libnautilus-private/nautilus-column-utilities.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file-dnd.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-dnd.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-tree-view-drag-dest.h>
#include <libnautilus-private/nautilus-view-factory.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-cell-renderer-pixbuf-emblem.h>

/* Included for the typeselect flush delay */
#include <libnautilus-private/nautilus-icon-container.h>

typedef struct {
	char *type_select_pattern;
	guint64 last_typeselect_time;
} TypeSelectState;

struct FMListViewDetails {
	GtkTreeView *tree_view;
	FMListModel *model;
	GtkActionGroup *list_action_group;
	guint list_merge_id;

	GtkTreeViewColumn   *file_name_column;
	int file_name_column_num;

	GtkCellRendererPixbuf *pixbuf_cell;
	GtkCellRendererText   *file_name_cell;
	GList *cells;

	NautilusZoomLevel zoom_level;

	NautilusTreeViewDragDest *drag_dest;

	GtkTargetList *source_target_list;

	GtkTreePath *double_click_path[2]; /* Both clicks in a double click need to be on the same row */
	
	guint drag_button;
	int drag_x;
	int drag_y;

	gboolean drag_started;

	gboolean row_selected_on_button_down;

	/* typeahead selection state */
	TypeSelectState *type_select_state;

	gboolean menus_ready;
	
	GHashTable *columns;
	GtkWidget *column_editor;
};

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	28

static int                      click_policy_auto_value;
static char *              	default_sort_order_auto_value;
static gboolean			default_sort_reversed_auto_value;
static NautilusZoomLevel        default_zoom_level_auto_value;
static GList *                  default_visible_columns_auto_value;
static GList *                  default_column_order_auto_value;

static GList *fm_list_view_get_selection   (FMDirectoryView   *view);
static void   fm_list_view_set_zoom_level  (FMListView        *view,
					    NautilusZoomLevel  new_level,
					    gboolean           always_set_level);
static void   fm_list_view_scale_font_size (FMListView        *view,
					    NautilusZoomLevel  new_level);
static void   fm_list_view_scroll_to_file  (FMListView        *view,
					    NautilusFile      *file);
static void   fm_list_view_iface_init      (NautilusViewIface *iface);



G_DEFINE_TYPE_WITH_CODE (FMListView, fm_list_view, FM_TYPE_DIRECTORY_VIEW, 
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_VIEW,
						fm_list_view_iface_init));

/* for EEL_CALL_PARENT */
#define parent_class fm_list_view_parent_class

static void
list_selection_changed_callback (GtkTreeSelection *selection, gpointer user_data)
{
	FMDirectoryView *view;

	view = FM_DIRECTORY_VIEW (user_data);

	fm_directory_view_notify_selection_changed (view);
}

/* Move these to eel? */

static void
tree_selection_foreach_set_boolean (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    gpointer callback_data)
{
	* (gboolean *) callback_data = TRUE;
}

static gboolean
tree_selection_not_empty (GtkTreeSelection *selection)
{
	gboolean not_empty;

	not_empty = FALSE;
	gtk_tree_selection_selected_foreach (selection,
					     tree_selection_foreach_set_boolean,
					     &not_empty);
	return not_empty;
}

static gboolean
tree_view_has_selection (GtkTreeView *view)
{
	return tree_selection_not_empty (gtk_tree_view_get_selection (view));
}

static void
activate_selected_items (FMListView *view)
{
	GList *file_list;
	
	file_list = fm_list_view_get_selection (FM_DIRECTORY_VIEW (view));
	fm_directory_view_activate_files (FM_DIRECTORY_VIEW (view),
					  file_list,
					  NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
					  0);
	nautilus_file_list_free (file_list);

}

static void
activate_selected_items_alternate (FMListView *view,
				   NautilusFile *file)
{
	GList *file_list;


	if (file != NULL) {
		nautilus_file_ref (file);
		file_list = g_list_prepend (NULL, file);
	} else {
		file_list = fm_list_view_get_selection (FM_DIRECTORY_VIEW (view));
	}
	fm_directory_view_activate_files (FM_DIRECTORY_VIEW (view),
					  file_list,
					  NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
					  NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND);
	nautilus_file_list_free (file_list);

}

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static void
fm_list_view_did_not_drag (FMListView *view,
			   GdkEventButton *event)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	
	tree_view = view->details->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
		if ((event->button == 1 || event->button == 2)
		    && ((event->state & GDK_CONTROL_MASK) != 0 ||
			(event->state & GDK_SHIFT_MASK) == 0)
		    && view->details->row_selected_on_button_down
		    && (click_policy_auto_value == NAUTILUS_CLICK_POLICY_DOUBLE)) {
			if (!button_event_modifies_selection (event)) {
				gtk_tree_selection_unselect_all (selection);
				gtk_tree_selection_select_path (selection, path);
			} else {
				gtk_tree_selection_unselect_path (selection, path);
			}
		}

		if ((click_policy_auto_value == NAUTILUS_CLICK_POLICY_SINGLE)
		    && !button_event_modifies_selection(event)) {
			if (event->button == 1) {
				activate_selected_items (view);
			} else if (event->button == 2) {
				activate_selected_items_alternate (view, NULL);
			}
		}
		gtk_tree_path_free (path);
	}
	
}

static void 
drag_data_get_callback (GtkWidget *widget,
			GdkDragContext *context,
			GtkSelectionData *selection_data,
			guint info,
			guint time)
{
  GtkTreeView *tree_view;
  GtkTreeModel *model;
  GList *ref_list;

  tree_view = GTK_TREE_VIEW (widget);
  
  model = gtk_tree_view_get_model (tree_view);
  
  if (model == NULL) {
	  return;
  }

  ref_list = g_object_get_data (G_OBJECT (context), "drag-info");

  if (ref_list == NULL) {
	  return;
  }

  if (EGG_IS_TREE_MULTI_DRAG_SOURCE (model))
    {
      egg_tree_multi_drag_source_drag_data_get (EGG_TREE_MULTI_DRAG_SOURCE (model),
						ref_list,
						selection_data);
    }
}

static void
selection_foreach (GtkTreeModel *model,
		   GtkTreePath *path,
		   GtkTreeIter *iter,
		   gpointer data)
{
	GList **list;
	
	list = (GList**)data;
	
	*list = g_list_prepend (*list, 
				gtk_tree_row_reference_new (model, path));
	
}

static GList *
get_selection_refs (GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;
	GList *ref_list;
	
	ref_list = NULL;
	
	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_selected_foreach (selection, 
					     selection_foreach, 
					     &ref_list);
	ref_list = g_list_reverse (ref_list);

	return ref_list;
}

static void
ref_list_free (GList *ref_list)
{
	g_list_foreach (ref_list, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free (ref_list);
}

static void
stop_drag_check (FMListView *view)
{		
	view->details->drag_button = 0;
}

static GdkPixbuf *
get_drag_pixbuf (FMListView *view)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkPixbuf *ret;
	GdkRectangle cell_area;
	
	ret = NULL;
	
	if (gtk_tree_view_get_path_at_pos (view->details->tree_view, 
					   view->details->drag_x,
					   view->details->drag_y,
					   &path, NULL, NULL, NULL)) {
		model = gtk_tree_view_get_model (view->details->tree_view);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter,
				    fm_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
				    &ret,
				    -1);

		gtk_tree_view_get_cell_area (view->details->tree_view,
					     path, 
					     view->details->file_name_column, 
					     &cell_area);

		gtk_tree_path_free (path);
	}

	return ret;
}

static gboolean
motion_notify_callback (GtkWidget *widget,
			GdkEventMotion *event,
			gpointer callback_data)
{
	FMListView *view;
	GdkDragContext *context;
	GList *ref_list;
	GdkPixbuf *pixbuf;	
	
	view = FM_LIST_VIEW (callback_data);
	
	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))) {
		return FALSE;
	}

	if (view->details->drag_button != 0) {
		if (gtk_drag_check_threshold (widget,
					      view->details->drag_x,
					      view->details->drag_y,
					      event->x, 
					      event->y)) {
			context = gtk_drag_begin
				(widget,
				 view->details->source_target_list,
				 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK,
				 view->details->drag_button,
				 (GdkEvent*)event);

			stop_drag_check (view);
			view->details->drag_started = TRUE;
			
			ref_list = get_selection_refs (GTK_TREE_VIEW (widget));
			g_object_set_data_full (G_OBJECT (context),
						"drag-info",
						ref_list,
						(GDestroyNotify)ref_list_free);

			pixbuf = get_drag_pixbuf (view);
			if (pixbuf) {
				gtk_drag_set_icon_pixbuf (context,
							  pixbuf,
							  0, 0);
				g_object_unref (pixbuf);
			} else {
				gtk_drag_set_icon_default (context);
			}
		}		      
	}
	return TRUE;
}

static void
do_popup_menu (GtkWidget *widget, FMListView *view, GdkEventButton *event)
{
 	if (tree_view_has_selection (GTK_TREE_VIEW (widget))) {
		fm_directory_view_pop_up_selection_context_menu (FM_DIRECTORY_VIEW (view), event);
	} else {
                fm_directory_view_pop_up_background_context_menu (FM_DIRECTORY_VIEW (view), event);
	}
}

static gboolean
button_press_callback (GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
	FMListView *view;
	GtkTreeView *tree_view;
	GtkTreePath *path;
	gboolean call_parent;
	gboolean allow_drag;
	GtkTreeSelection *selection;
	GtkWidgetClass *tree_view_class;
	gint64 current_time;
	static gint64 last_click_time = 0;
	static int click_count = 0;
	int double_click_time;

	view = FM_LIST_VIEW (callback_data);
	tree_view = GTK_TREE_VIEW (widget);
	tree_view_class = GTK_WIDGET_GET_CLASS (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
		return FALSE;
	}

	fm_list_model_set_drag_view
		(FM_LIST_MODEL (gtk_tree_view_get_model (tree_view)),
		 tree_view,
		 event->x, event->y);
	
	g_object_get (G_OBJECT (gtk_widget_get_settings (widget)), 
		      "gtk-double-click-time", &double_click_time,
		      NULL);

	/* Determine click count */
	current_time = eel_get_system_time ();
	if (current_time - last_click_time < double_click_time * 1000) {
		click_count++;
	} else {
		click_count = 0;
	}

	/* Stash time for next compare */
	last_click_time = current_time;

	/* Ignore double click if we are in single click mode */
	if (click_policy_auto_value == NAUTILUS_CLICK_POLICY_SINGLE && click_count >= 2) {
		return TRUE;
	}

	
	call_parent = TRUE;
	allow_drag = FALSE;
	if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
		/* Keep track of path of last click so double clicks only happen
		 * on the same item */
		if ((event->button == 1 || event->button == 2)  && 
		    event->type == GDK_BUTTON_PRESS) {
			if (view->details->double_click_path[1]) {
				gtk_tree_path_free (view->details->double_click_path[1]);
			}
			view->details->double_click_path[1] = view->details->double_click_path[0];
			view->details->double_click_path[0] = gtk_tree_path_copy (path);
		}
		if (event->type == GDK_2BUTTON_PRESS) {
			/* Double clicking does not trigger a D&D action. */
			view->details->drag_button = 0;
			if (view->details->double_click_path[1] &&
			    gtk_tree_path_compare (view->details->double_click_path[0], view->details->double_click_path[1]) == 0) {
				/* NOTE: Activation can actually destroy the view if we're switching */
				if (!button_event_modifies_selection (event)) {
					if ((event->button == 1 || event->button == 3)) {
						activate_selected_items (view);
					} else if (event->button == 2) {
						activate_selected_items_alternate (view, NULL);
					}
				} else if (event->button == 1 &&
					   (event->state & GDK_SHIFT_MASK) != 0) {
					NautilusFile *file;
					file = fm_list_model_file_for_path (view->details->model, path);
					if (file != NULL) {
						activate_selected_items_alternate (view, file);
						nautilus_file_unref (file);
					}
				}
			}
			tree_view_class->button_press_event (widget, event);
		} else {
	
			/* We're going to filter out some situations where
			 * we can't let the default code run because all
			 * but one row would be would be deselected. We don't
			 * want that; we want the right click menu or single
			 * click to apply to everything that's currently selected. */
			
			if (event->button == 3 && gtk_tree_selection_path_is_selected (selection, path)) {
				call_parent = FALSE;
			} 
			
			if ((event->button == 1 || event->button == 2) &&
			    ((event->state & GDK_CONTROL_MASK) != 0 ||
			     (event->state & GDK_SHIFT_MASK) == 0)) {
				view->details->row_selected_on_button_down = gtk_tree_selection_path_is_selected (selection, path);
				if (view->details->row_selected_on_button_down) {
					call_parent = FALSE;
				} else if  ((event->state & GDK_CONTROL_MASK) != 0) {
					call_parent = FALSE;
					gtk_tree_selection_select_path (selection, path);
				}
			}
		
			if (call_parent) {
				tree_view_class->button_press_event (widget, event);
			} else if (gtk_tree_selection_path_is_selected (selection, path)) {
				gtk_widget_grab_focus (widget);
			}
			
			if ((event->button == 1 || event->button == 2) &&
			    event->type == GDK_BUTTON_PRESS) {
				view->details->drag_started = FALSE;
				view->details->drag_button = event->button;
				view->details->drag_x = event->x;
				view->details->drag_y = event->y;
			}
			
			if (event->button == 3) {
				do_popup_menu (widget, view, event);
			}
		}

		gtk_tree_path_free (path);
	} else {
		if ((event->button == 1 || event->button == 2)  && 
		    event->type == GDK_BUTTON_PRESS) {
			if (view->details->double_click_path[1]) {
				gtk_tree_path_free (view->details->double_click_path[1]);
			}
			view->details->double_click_path[1] = view->details->double_click_path[0];
			view->details->double_click_path[0] = NULL;
		}
		/* Deselect if people click outside any row. It's OK to
		   let default code run; it won't reselect anything. */
		gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tree_view));

		if (event->button == 3) {
			do_popup_menu (widget, view, event);
		}
	}
	
	/* We chained to the default handler in this method, so never
	 * let the default handler run */ 
	return TRUE;
}

static gboolean
button_release_callback (GtkWidget *widget, 
			 GdkEventButton *event, 
			 gpointer callback_data)
{
	FMListView *view;
	
	view = FM_LIST_VIEW (callback_data);

	if (event->button == view->details->drag_button) {
		stop_drag_check (view);
		if (!view->details->drag_started) {
			fm_list_view_did_not_drag (view, event);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
select_matching_name (FMListView *view,
		      const char *match_name)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean match_found;
	GValue value = { 0 };
	const gchar *file_name;
	int match_name_len, file_name_len;
	
	match_found = FALSE;
	
	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (view->details->model), &iter)) {
		return FALSE;
	}

	match_name_len = strlen (match_name);
	do {
		gtk_tree_model_get_value (GTK_TREE_MODEL (view->details->model), &iter, view->details->file_name_column_num, &value);
		file_name = g_value_get_string (&value);
		file_name_len = strlen (file_name);
		match_found = file_name_len >= match_name_len &&
			g_ascii_strncasecmp (match_name, file_name, MIN (match_name_len, file_name_len)) == 0;
		g_value_unset (&value);

		if (match_found) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->model), &iter);
			gtk_tree_view_set_cursor (view->details->tree_view, path, NULL, FALSE);
			gtk_tree_view_scroll_to_cell (view->details->tree_view, path, NULL, FALSE, 0, 0);
			gtk_tree_path_free (path);
			
			return TRUE;
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (view->details->model), &iter));
	
	return FALSE;
}

static void
fm_list_view_flush_typeselect_state (FMListView *view)
{
	if (view->details->type_select_state == NULL) {
		return;
	}
	
	g_free (view->details->type_select_state->type_select_pattern);
	g_free (view->details->type_select_state);
	view->details->type_select_state = NULL;
}

static gboolean
handle_typeahead (FMListView *view,
		  GdkEventKey *event,
		  gboolean *flush_typeahead)
{
	char *new_pattern;
	gint64 now;
	gint64 time_delta;
	guint32 unichar;
	char unichar_utf8[7];
	int i;

	unichar = gdk_keyval_to_unicode (event->keyval);
	i = g_unichar_to_utf8 (unichar, unichar_utf8);
	unichar_utf8[i] = 0;
	
	*flush_typeahead = FALSE;

	if (*event->string == 0) {
		/* can be an empty string if the modifier was held down, etc. */
		return FALSE;
	}
	
	if (!g_unichar_isprint (unichar)) {
		*flush_typeahead = TRUE;
		return FALSE;
	}
	
	/* lazily allocate the typeahead state */
	if (view->details->type_select_state == NULL) {
		view->details->type_select_state = g_new0 (TypeSelectState, 1);
	}

	/* find out how long since last character was typed */
	now = eel_get_system_time ();
	time_delta = now - view->details->type_select_state->last_typeselect_time;
	if (time_delta < 0 || time_delta > NAUTILUS_ICON_CONTAINER_TYPESELECT_FLUSH_DELAY) {
		/* the typeselect state is too old, start with a fresh one */
		g_free (view->details->type_select_state->type_select_pattern);
		view->details->type_select_state->type_select_pattern = NULL;
	}

	if (view->details->type_select_state->type_select_pattern != NULL) {
		new_pattern = g_strconcat
			(view->details->type_select_state->type_select_pattern,
			 unichar_utf8, NULL);
		g_free (view->details->type_select_state->type_select_pattern);
	} else {
		new_pattern = g_strdup (unichar_utf8);
	}

	view->details->type_select_state->type_select_pattern = new_pattern;
	view->details->type_select_state->last_typeselect_time = now;

	select_matching_name (view, new_pattern);

	return TRUE;
}

static gboolean
popup_menu_callback (GtkWidget *widget, gpointer callback_data)
{
 	FMListView *view;

	view = FM_LIST_VIEW (callback_data);

	do_popup_menu (widget, view, NULL);

	return TRUE;
}

static gboolean
key_press_callback (GtkWidget *widget, GdkEventKey *event, gpointer callback_data)
{
	FMDirectoryView *view;
	GdkEventButton button_event = { 0 };
	gboolean handled;
	gboolean flush_typeahead;

	view = FM_DIRECTORY_VIEW (callback_data);
	handled = FALSE;
	flush_typeahead = TRUE;
	
	switch (event->keyval) {
	case GDK_F10:
		if (event->state & GDK_CONTROL_MASK) {
			fm_directory_view_pop_up_background_context_menu (view, &button_event);
		}
		break;
	case GDK_space:
		if (event->state & GDK_CONTROL_MASK) {
			handled = FALSE;
			break;
		}
		if (!GTK_WIDGET_HAS_FOCUS (GTK_WIDGET (FM_LIST_VIEW (view)->details->tree_view))) {
			handled = FALSE;
			break;
		}
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (FM_LIST_VIEW (view), NULL);
		} else {
			activate_selected_items (FM_LIST_VIEW (view));
		}
		handled = TRUE;
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (FM_LIST_VIEW (view), NULL);
		} else {
			activate_selected_items (FM_LIST_VIEW (view));
		}
		handled = TRUE;
		break;

	default:
		/* Don't use Control or Alt keys for type-selecting, because they
		 * might be used for menus.
		 */
		handled = (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0 &&
			handle_typeahead (FM_LIST_VIEW (view), event, &flush_typeahead);
		break;
	}
	if (flush_typeahead) {
		/* any non-ascii key will force the typeahead state to be forgotten */
		fm_list_view_flush_typeselect_state (FM_LIST_VIEW (view));
	}

	return handled;
}

static void
sort_column_changed_callback (GtkTreeSortable *sortable, 
			      FMListView *view)
{
	NautilusFile *file;
	gint sort_column_id;
	GtkSortType reversed;
	char *sort_attr, *default_sort_attr;
	char *reversed_attr, *default_reversed_attr;

	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));

	gtk_tree_sortable_get_sort_column_id (sortable, &sort_column_id, &reversed);

	sort_attr = fm_list_model_get_attribute_from_sort_column_id (view->details->model, sort_column_id);
	sort_column_id = fm_list_model_get_sort_column_id_from_attribute (view->details->model, default_sort_order_auto_value);
	default_sort_attr = fm_list_model_get_attribute_from_sort_column_id (view->details->model, sort_column_id);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
				    default_sort_attr, sort_attr);
	g_free (default_sort_attr);
	g_free (sort_attr);

	default_reversed_attr = (default_sort_reversed_auto_value ? "true" : "false");
	reversed_attr = (reversed ? "true" : "false");
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
				    default_reversed_attr, reversed_attr);
}

static void
cell_renderer_edited (GtkCellRendererText *cell,
		      const char          *path_str,
		      const char          *new_text,
		      FMListView          *view)
{
	GtkTreePath *path;
	NautilusFile *file;
	GtkTreeIter iter;
	
	/* Don't allow a rename with an empty string. Revert to original 
	 * without notifying the user.
	 */
	if (new_text[0] == '\0') {
		g_object_set (G_OBJECT (view->details->file_name_cell),
			      "editable", FALSE,
			      NULL);
		return;
	}
	
	path = gtk_tree_path_new_from_string (path_str);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
				 &iter, path);

	gtk_tree_path_free (path);
	
	gtk_tree_model_get (GTK_TREE_MODEL (view->details->model),
			    &iter,
			    FM_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	fm_rename_file (file, new_text);
	
	nautilus_file_unref (file);

	/*We're done editing - make the filename-cells readonly again.*/
	g_object_set (G_OBJECT (view->details->file_name_cell),
		      "editable", FALSE,
		      NULL);

}

static char *
get_root_uri_callback (NautilusTreeViewDragDest *dest,
		       gpointer user_data)
{
	FMListView *view;
	
	view = FM_LIST_VIEW (user_data);

	return fm_directory_view_get_uri (FM_DIRECTORY_VIEW (view));
}

static NautilusFile *
get_file_for_path_callback (NautilusTreeViewDragDest *dest,
			    GtkTreePath *path,
			    gpointer user_data)
{
	FMListView *view;
	GtkTreeIter iter;
	NautilusFile *file;
	
	view = FM_LIST_VIEW (user_data);

	file = NULL;

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
				     &iter, path)) {
		gtk_tree_model_get (GTK_TREE_MODEL (view->details->model),
				    &iter,
				    FM_LIST_MODEL_FILE_COLUMN,
				    &file,
				    -1);
	}

	return file;
}

static void
move_copy_items_callback (NautilusTreeViewDragDest *dest,
			  const GList *item_uris,
			  const char *target_uri,
			  guint action,
			  int x, 
			  int y,
			  gpointer user_data)

{
	fm_directory_view_move_copy_items (item_uris,
					   NULL,
					   target_uri,
					   action,
					   x, y,
					   FM_DIRECTORY_VIEW (user_data));
}

static void
apply_visible_columns_foreach (gpointer key, gpointer value, gpointer user_data)
{
	gboolean visible;
	
	visible = (eel_g_str_list_index (user_data, key) != -1);
	
	gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN (value), 
					  visible);
}

static void
apply_visible_columns (FMListView *list_view, GList *visible_columns)
{
	g_hash_table_foreach (list_view->details->columns, 
			      apply_visible_columns_foreach,
			      visible_columns);
	
}

static void
apply_column_order (FMListView *list_view, GList *column_order)
{
	GList *columns;	
	GtkTreeViewColumn *last_view_column;
	GList *l;
	
	columns = nautilus_get_all_columns ();
	columns = nautilus_sort_columns (columns, column_order);

	last_view_column = NULL;
	for (l = columns; l != NULL; l = l->next) {
		GtkTreeViewColumn *view_column;
		char *name;

		g_object_get (G_OBJECT (l->data), "name", &name, NULL);
		
		view_column = g_hash_table_lookup (list_view->details->columns,
						   name);

		g_free (name);
		
		if (view_column) {
			gtk_tree_view_move_column_after 
				(list_view->details->tree_view,
				 view_column,
				 last_view_column);
			
			last_view_column = view_column;
		}
	}
	nautilus_column_list_free (columns);
}

static void
create_and_set_up_tree_view (FMListView *view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTargetEntry *drag_types;
	AtkObject *atk_obj;
	int num_drag_types;	
	GList *nautilus_columns;
	GList *l;
	
	view->details->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	view->details->columns = g_hash_table_new_full (g_str_hash, 
							g_str_equal,
							(GDestroyNotify)g_free,
							NULL);

	fm_list_model_get_drag_types (&drag_types, &num_drag_types);
	
	view->details->drag_dest = 
		nautilus_tree_view_drag_dest_new (view->details->tree_view);

	g_signal_connect_object (view->details->drag_dest,
				 "get_root_uri",
				 G_CALLBACK (get_root_uri_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "get_file_for_path",
				 G_CALLBACK (get_file_for_path_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "move_copy_items",
				 G_CALLBACK (move_copy_items_callback),
				 view, 0);

	g_signal_connect_object (gtk_tree_view_get_selection (view->details->tree_view),
				 "changed",
				 G_CALLBACK (list_selection_changed_callback), view, 0);

	g_signal_connect_object (view->details->tree_view, "drag_data_get",
				 G_CALLBACK (drag_data_get_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "motion_notify_event",
				 G_CALLBACK (motion_notify_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "button_press_event",
				 G_CALLBACK (button_press_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "button_release_event",
				 G_CALLBACK (button_release_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "key_press_event",
				 G_CALLBACK (key_press_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "popup_menu",
                                 G_CALLBACK (popup_menu_callback), view, 0);
	
	view->details->model = g_object_new (FM_TYPE_LIST_MODEL, NULL);
	gtk_tree_view_set_model (view->details->tree_view, GTK_TREE_MODEL (view->details->model));
	/* Need the model for the dnd drop icon "accept" change */
	fm_list_model_set_drag_view (FM_LIST_MODEL (view->details->model),
				     view->details->tree_view,  0, 0);

	g_signal_connect_object (view->details->model, "sort_column_changed",
				 G_CALLBACK (sort_column_changed_callback), view, 0);

	view->details->source_target_list = 
		gtk_target_list_new (drag_types, num_drag_types);
	

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view->details->tree_view), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint (view->details->tree_view, TRUE);

	nautilus_columns = nautilus_get_all_columns ();

	for (l = nautilus_columns; l != NULL; l = l->next) {
		NautilusColumn *nautilus_column;
		int column_num;		
		char *name;
		char *label;
		float xalign;

		nautilus_column = NAUTILUS_COLUMN (l->data);

		g_object_get (nautilus_column, 
			      "name", &name, 
			      "label", &label,
			      "xalign", &xalign, NULL);

		column_num = fm_list_model_add_column (view->details->model,
						       nautilus_column);

		/* Created the name column specially, because it
		 * has the icon in it.*/
		if (!strcmp (name, "name")) {
			/* Create the file name column */
			cell = nautilus_cell_renderer_pixbuf_emblem_new ();
			view->details->pixbuf_cell = (GtkCellRendererPixbuf *)cell;
			
			view->details->file_name_column = gtk_tree_view_column_new ();
			view->details->file_name_column_num = column_num;
			
			g_hash_table_insert (view->details->columns,
					     g_strdup ("name"), 
					     view->details->file_name_column);
			
			gtk_tree_view_column_set_sort_column_id (view->details->file_name_column, column_num);
			gtk_tree_view_column_set_title (view->details->file_name_column, _("Name"));
			gtk_tree_view_column_set_resizable (view->details->file_name_column, TRUE);
			
			gtk_tree_view_column_pack_start (view->details->file_name_column, cell, FALSE);
			gtk_tree_view_column_set_attributes (view->details->file_name_column,
							     cell,
							     "pixbuf", FM_LIST_MODEL_SMALLEST_ICON_COLUMN,
							     "pixbuf_emblem", FM_LIST_MODEL_SMALLEST_EMBLEM_COLUMN,
							     NULL);
			
			cell = gtk_cell_renderer_text_new ();
			view->details->file_name_cell = (GtkCellRendererText *)cell;
			g_signal_connect (cell, "edited", G_CALLBACK (cell_renderer_edited), view);
			
			gtk_tree_view_column_pack_start (view->details->file_name_column, cell, TRUE);
			gtk_tree_view_column_set_attributes (view->details->file_name_column, cell,
							     "text", 
							     column_num, NULL);
			gtk_tree_view_append_column (view->details->tree_view, view->details->file_name_column);
		} else {		
			cell = gtk_cell_renderer_text_new ();
			g_object_set (cell, "xalign", xalign, NULL);
			view->details->cells = g_list_append (view->details->cells,
							      cell);
			column = gtk_tree_view_column_new_with_attributes (label,
									   cell,
									   "text", column_num,
									   NULL);
			gtk_tree_view_column_set_sort_column_id (column, column_num);
			g_hash_table_insert (view->details->columns, 
					     g_strdup (name), 
					     column);
			
			gtk_tree_view_column_set_resizable (column, TRUE);
			gtk_tree_view_column_set_visible (column, TRUE);
			gtk_tree_view_append_column (view->details->tree_view, column);
		}
		g_free (name);
		g_free (label);
	}
	nautilus_column_list_free (nautilus_columns);

	/* Apply the default column order and visible columns, to get it
	 * right most of the time. The metadata will be checked when a 
	 * folder is loaded */
	apply_visible_columns (view, default_visible_columns_auto_value);
	apply_column_order (view, default_column_order_auto_value);

	gtk_widget_show (GTK_WIDGET (view->details->tree_view));
	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (view->details->tree_view));


        atk_obj = gtk_widget_get_accessible (GTK_WIDGET (view->details->tree_view));
        atk_object_set_name (atk_obj, _("List View"));
}

static void
fm_list_view_add_file (FMDirectoryView *view, NautilusFile *file)
{
	fm_list_model_add_file (FM_LIST_VIEW (view)->details->model, file);
}

static GList *
get_visible_columns (FMListView *list_view)
{
	NautilusFile *file;
	GList *visible_columns;
	
	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));

	visible_columns = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
		 NAUTILUS_METADATA_SUBKEY_COLUMNS);

	if (!visible_columns) {
		visible_columns = eel_g_str_list_copy (default_visible_columns_auto_value);
	}

	return visible_columns;
}

static void
set_visible_columns_from_metadata_and_preferences (FMListView *list_view)
{
	GList *visible_columns;

	visible_columns = get_visible_columns (list_view);
	apply_visible_columns (list_view, visible_columns);
	eel_g_list_free_deep (visible_columns);
}

static GList *
get_column_order (FMListView *list_view)
{
	NautilusFile *file;
	GList *column_order;
	
	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));

	column_order = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
		 NAUTILUS_METADATA_SUBKEY_COLUMNS);

	if (!column_order) {
		column_order = eel_g_str_list_copy (default_column_order_auto_value);
	}

	return column_order;
}

static void
set_column_order_from_metadata_and_preferences (FMListView *list_view)
{
	GList *column_order;

	column_order = get_column_order (list_view);
	apply_column_order (list_view, column_order);
	eel_g_list_free_deep (column_order);
}

static void
set_sort_order_from_metadata_and_preferences (FMListView *list_view)
{
	char *sort_attribute;
	int sort_column_id;
	NautilusFile *file;
	gboolean sort_reversed;
	
	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));
	sort_attribute = nautilus_file_get_metadata (file,
						     NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
						     NULL);
	sort_column_id = fm_list_model_get_sort_column_id_from_attribute (list_view->details->model, sort_attribute);
	g_free (sort_attribute);
	if (sort_column_id == -1) {
		sort_column_id = fm_list_model_get_sort_column_id_from_attribute (list_view->details->model, default_sort_order_auto_value);
	}

	sort_reversed = nautilus_file_get_boolean_metadata (file,
							    NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
							    default_sort_reversed_auto_value);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_view->details->model),
					      sort_column_id,
					      sort_reversed ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);
}

static gboolean
list_view_changed_foreach (GtkTreeModel *model,
              		   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
	gtk_tree_model_row_changed (model, path, iter);
	return FALSE;
}

static NautilusZoomLevel
get_default_zoom_level (void) {
	NautilusZoomLevel default_zoom_level;

	default_zoom_level = default_zoom_level_auto_value;

	if (default_zoom_level <  NAUTILUS_ZOOM_LEVEL_SMALLEST
	    || NAUTILUS_ZOOM_LEVEL_LARGEST < default_zoom_level) {
		default_zoom_level = NAUTILUS_ZOOM_LEVEL_SMALL;
	}

	return default_zoom_level;
}

static void
set_zoom_level_from_metadata_and_preferences (FMListView *list_view)
{
	NautilusFile *file;
	int level;

	if (fm_directory_view_supports_zooming (FM_DIRECTORY_VIEW (list_view))) {
		file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (list_view));
		level = nautilus_file_get_integer_metadata (file,
							    NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, 
							    get_default_zoom_level ());
		fm_list_view_set_zoom_level (list_view, level, TRUE);
		
		/* updated the rows after updating the font size */
		gtk_tree_model_foreach (GTK_TREE_MODEL (list_view->details->model),
					list_view_changed_foreach, NULL);
	}
}

static void
fm_list_view_begin_loading (FMDirectoryView *view)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (view);

	set_sort_order_from_metadata_and_preferences (list_view);
	set_zoom_level_from_metadata_and_preferences (list_view);
	set_visible_columns_from_metadata_and_preferences (list_view);
	set_column_order_from_metadata_and_preferences (list_view);
}

static void
fm_list_view_clear (FMDirectoryView *view)
{
	FMListView *list_view;
	GtkTreeViewColumn *column;

	list_view = FM_LIST_VIEW (view);

	if (list_view->details->model != NULL) {
		/* Stop an ongoing rename to commit the name changes when the user
		 * changes directories without exiting cell edit mode. It also prevents
		 * the edited handler from being called on the cleared list model.
		 */

		column = list_view->details->file_name_column;
		if (column != NULL && column->editable_widget != NULL &&
			GTK_IS_CELL_EDITABLE (column->editable_widget)) {
			gtk_cell_editable_editing_done (column->editable_widget);
		}

		fm_list_model_clear (list_view->details->model);
	}
}

static void
fm_list_view_file_changed (FMDirectoryView *view, NautilusFile *file)
{
	fm_list_model_file_changed (FM_LIST_VIEW (view)->details->model, file);
}

static GtkWidget *
fm_list_view_get_background_widget (FMDirectoryView *view)
{
	return GTK_WIDGET (view);
}

static void
fm_list_view_get_selection_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GList **list;
	NautilusFile *file;
	
	list = data;

	gtk_tree_model_get (model, iter,
			    FM_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	nautilus_file_ref (file);
	(* list) = g_list_prepend ((* list), file);
}

static GList *
fm_list_view_get_selection (FMDirectoryView *view)
{
	GList *list;

	list = NULL;

	gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (FM_LIST_VIEW (view)->details->tree_view),
					     fm_list_view_get_selection_foreach_func, &list);

	return list;
}


static guint
fm_list_view_get_item_count (FMDirectoryView *view)
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (view), 0);

	return fm_list_model_get_length (FM_LIST_VIEW (view)->details->model);
}

static gboolean
fm_list_view_is_empty (FMDirectoryView *view)
{
	return fm_list_model_is_empty (FM_LIST_VIEW (view)->details->model);
}

static void
fm_list_view_remove_file (FMDirectoryView *view, NautilusFile *file)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeIter temp_iter;
	GtkTreeRowReference* row_reference;
	FMListView *list_view;
	GtkTreeModel* tree_model; 
	
	path = NULL;
	row_reference = NULL;
	list_view = FM_LIST_VIEW (view);
	tree_model = GTK_TREE_MODEL(list_view->details->model);
	
	if(fm_list_model_get_tree_iter_from_file (list_view->details->model, file, &iter)) {
	   temp_iter = iter;
	  
	   /* get reference for next element in the list view. If the element to be deleted is the 
	    * last one, get reference to previous element. If there is only one element in view
            * no need to select anything. */		
	   
	   if(gtk_tree_model_iter_next (tree_model, &iter)) {
              path = gtk_tree_model_get_path (tree_model, &iter);
              row_reference = gtk_tree_row_reference_new (tree_model, path);
           } else {
              path = gtk_tree_model_get_path (tree_model, &temp_iter);
              if(gtk_tree_path_prev (path)) {
                 row_reference = gtk_tree_row_reference_new (tree_model, path);
	       }
	   }
       
	   gtk_tree_path_free (path);
		
	   fm_list_model_remove_file (list_view->details->model, file);
	   
	   if(gtk_tree_row_reference_valid(row_reference)) {
	      path = gtk_tree_row_reference_get_path (row_reference);
	      gtk_tree_view_set_cursor (list_view->details->tree_view, path, NULL, FALSE);
              gtk_tree_path_free (path);
	      
	   }
	  
	   if(row_reference) {
	      gtk_tree_row_reference_free (row_reference);
	   }
	}   
	
	
}

static void
fm_list_view_set_selection (FMDirectoryView *view, GList *selection)
{
	FMListView *list_view;
	GtkTreeSelection *tree_selection;
	GList *node;
	GtkTreeIter iter;
	NautilusFile *file;
	
	list_view = FM_LIST_VIEW (view);
	tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

	g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

	gtk_tree_selection_unselect_all (tree_selection);
	for (node = selection; node != NULL; node = node->next) {
		file = node->data;
		if (fm_list_model_get_tree_iter_from_file (list_view->details->model, file, &iter)) {
			gtk_tree_selection_select_iter (tree_selection, &iter);
		}
	}

	g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
	fm_directory_view_notify_selection_changed (view);
}

static void
fm_list_view_select_all (FMDirectoryView *view)
{
	gtk_tree_selection_select_all (gtk_tree_view_get_selection (FM_LIST_VIEW (view)->details->tree_view));
}

static void
fm_list_view_reveal_selection (FMDirectoryView *view)
{
	GList *selection;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

        selection = fm_directory_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		FMListView *list_view;
		NautilusFile *file;
		GtkTreeIter iter;
		GtkTreePath *path;
		
		list_view = FM_LIST_VIEW (view);
		file = selection->data;
		if (!fm_list_model_get_tree_iter_from_file (list_view->details->model, file, &iter)) {
			return;
		}
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

		gtk_tree_view_scroll_to_cell (list_view->details->tree_view, path, NULL, FALSE, 0.0, 0.0);

		gtk_tree_path_free (path);
		path = NULL;
	}

        nautilus_file_list_free (selection);
}

static void
column_editor_response_callback (GtkWidget *dialog, 
				 int response_id,
				 gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
column_chooser_changed_callback (NautilusColumnChooser *chooser,
				 FMListView *view)
{
	NautilusFile *file;
	GList *visible_columns;
	GList *column_order;
	
	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));

	nautilus_column_chooser_get_settings (chooser,
					      &visible_columns,
					      &column_order);

	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NAUTILUS_METADATA_SUBKEY_COLUMNS, visible_columns);
	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NAUTILUS_METADATA_SUBKEY_COLUMNS, column_order);

	eel_g_list_free_deep (visible_columns);
	eel_g_list_free_deep (column_order);

	set_visible_columns_from_metadata_and_preferences (view);
	set_column_order_from_metadata_and_preferences (view);
}

static void
column_chooser_set_from_settings (NautilusColumnChooser *chooser,
				  FMListView *view)
{
	GList *visible_columns;
	GList *column_order;

	g_signal_handlers_block_by_func 
		(chooser, G_CALLBACK (column_chooser_changed_callback), view);

	visible_columns = get_visible_columns (view);
	column_order = get_column_order (view);

	nautilus_column_chooser_set_settings (chooser,
					      visible_columns, 
					      column_order);

	eel_g_list_free_deep (visible_columns);
	eel_g_list_free_deep (column_order);
	
	g_signal_handlers_unblock_by_func 
		(chooser, G_CALLBACK (column_chooser_changed_callback), view);
}

static void
column_chooser_use_default_callback (NautilusColumnChooser *chooser,
				     FMListView *view)
{
	NautilusFile *file;

	file = fm_directory_view_get_directory_as_file 
		(FM_DIRECTORY_VIEW (view));

	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NAUTILUS_METADATA_SUBKEY_COLUMNS, NULL);
	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NAUTILUS_METADATA_SUBKEY_COLUMNS, NULL);

	set_visible_columns_from_metadata_and_preferences (FM_LIST_VIEW (view));
	set_column_order_from_metadata_and_preferences (FM_LIST_VIEW (view));
	column_chooser_set_from_settings (chooser, view);
}

static GtkWidget *
create_column_editor (FMListView *view)
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *column_chooser;
	NautilusFile *file;
	char *title;
	char *name;
	
	file = fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view));
	name = nautilus_file_get_display_name (file);
	title = g_strdup_printf (_("%s Visible Columns"), name);
	g_free (name);

	window = gtk_dialog_new_with_buttons (title, 
					      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
					      NULL);
	g_free (title);
	g_signal_connect (window, "response", 
			  G_CALLBACK (column_editor_response_callback), NULL);
	
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);

	box = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 12);
	gtk_widget_show (box);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (window)->vbox), box);

	label = gtk_label_new (_("Choose the order of information to appear in this folder."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	column_chooser = nautilus_column_chooser_new ();
	gtk_widget_show (column_chooser);
	gtk_box_pack_start (GTK_BOX (box), column_chooser, TRUE, TRUE, 0);

	g_signal_connect (column_chooser, "changed",
			  G_CALLBACK (column_chooser_changed_callback),
			  view);
	g_signal_connect (column_chooser, "use_default",
			  G_CALLBACK (column_chooser_use_default_callback),
			  view);

	column_chooser_set_from_settings 
			  (NAUTILUS_COLUMN_CHOOSER (column_chooser), view);

	return window;
}

static void
action_visible_columns_callback (GtkAction *action,
				 gpointer callback_data)
{
	FMListView *list_view;
	
	list_view = FM_LIST_VIEW (callback_data);

	if (list_view->details->column_editor) {
		gtk_window_present (GTK_WINDOW (list_view->details->column_editor));
	} else {
		list_view->details->column_editor = create_column_editor (list_view);
		eel_add_weak_pointer (&list_view->details->column_editor);
		
		gtk_widget_show (list_view->details->column_editor);
	}
}

static GtkActionEntry list_view_entries[] = {
  { "Visible Columns", NULL,                  /* name, stock id */
    N_("Visible _Columns..."), NULL,                /* label, accelerator */
    N_("Select the columns visible in this folder"),                   /* tooltip */ 
    G_CALLBACK (action_visible_columns_callback) },
};

static void
fm_list_view_merge_menus (FMDirectoryView *view)
{
	FMListView *list_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	const char *ui;

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, merge_menus, (view));

	list_view = FM_LIST_VIEW (view);

	ui_manager = fm_directory_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("ListViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	list_view->details->list_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      list_view_entries, G_N_ELEMENTS (list_view_entries),
				      list_view);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	ui = nautilus_ui_string_get ("nautilus-list-view-ui.xml");
	list_view->details->list_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	list_view->details->menus_ready = TRUE;
}

static void
fm_list_view_update_menus (FMDirectoryView *view)
{
	FMListView *list_view;

        list_view = FM_LIST_VIEW (view);

	/* don't update if the menus aren't ready */
	if (!list_view->details->menus_ready) {
		return;
	}

	EEL_CALL_PARENT (FM_DIRECTORY_VIEW_CLASS, update_menus, (view));
}

/* Reset sort criteria and zoom level to match defaults */
static void
fm_list_view_reset_to_defaults (FMDirectoryView *view)
{
	NautilusFile *file;

	file = fm_directory_view_get_directory_as_file (view);

	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN, NULL, NULL);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED, NULL, NULL);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, NULL, NULL);
	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NAUTILUS_METADATA_SUBKEY_COLUMNS, NULL);
	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NAUTILUS_METADATA_SUBKEY_COLUMNS, NULL);

	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (FM_LIST_VIEW (view)->details->model),
		 fm_list_model_get_sort_column_id_from_attribute (FM_LIST_VIEW (view)->details->model, default_sort_order_auto_value),
		 default_sort_reversed_auto_value ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);

	fm_list_view_set_zoom_level (FM_LIST_VIEW (view), get_default_zoom_level (), FALSE);
	set_visible_columns_from_metadata_and_preferences (FM_LIST_VIEW (view));
	set_column_order_from_metadata_and_preferences (FM_LIST_VIEW (view));
}

static void
fm_list_view_scale_font_size (FMListView *view, 
			      NautilusZoomLevel new_level)
{
	GList *l;
	static gboolean first_time = TRUE;
	static double pango_scale[7];
	int medium;
	int i;

	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (first_time) {
		first_time = FALSE;
		medium = NAUTILUS_ZOOM_LEVEL_SMALLER;
		pango_scale[medium] = PANGO_SCALE_MEDIUM;
		for (i = medium; i > NAUTILUS_ZOOM_LEVEL_SMALLEST; i--) {
			pango_scale[i - 1] = (1 / 1.2) * pango_scale[i];
		}
		for (i = medium; i < NAUTILUS_ZOOM_LEVEL_LARGEST; i++) {
			pango_scale[i + 1] = 1.2 * pango_scale[i];
		}
	}
					 
	g_object_set (G_OBJECT (view->details->file_name_cell),
		      "scale", pango_scale[new_level],
		      NULL);
	for (l = view->details->cells; l != NULL; l = l->next) {
		g_object_set (G_OBJECT (l->data),
			      "scale", pango_scale[new_level],
			      NULL);
	}
}

static void
fm_list_view_set_zoom_level (FMListView *view,
			     NautilusZoomLevel new_level,
			     gboolean always_emit)
{
	int icon_size;
	int column, emblem_column;

	g_return_if_fail (FM_IS_LIST_VIEW (view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (view->details->zoom_level == new_level) {
		if (always_emit) {
			g_signal_emit_by_name (FM_DIRECTORY_VIEW(view), "zoom_level_changed");
		}
		return;
	}

	view->details->zoom_level = new_level;
	g_signal_emit_by_name (FM_DIRECTORY_VIEW(view), "zoom_level_changed");

	nautilus_file_set_integer_metadata
		(fm_directory_view_get_directory_as_file (FM_DIRECTORY_VIEW (view)), 
		 NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, 
		 get_default_zoom_level (),
		 new_level);

	/* Select correctly scaled icons. */
	column = fm_list_model_get_column_id_from_zoom_level (new_level);
	emblem_column = fm_list_model_get_emblem_column_id_from_zoom_level (new_level);
	gtk_tree_view_column_set_attributes (view->details->file_name_column,
					     GTK_CELL_RENDERER (view->details->pixbuf_cell),
					     "pixbuf", column,
					     "pixbuf_emblem", emblem_column,
					     NULL);

	/* Scale text. */
	fm_list_view_scale_font_size (view, new_level);

	/* Make all rows the same size. */
	icon_size = nautilus_get_icon_size_for_zoom_level (new_level);
	gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (view->details->pixbuf_cell),
					  -1, icon_size);

	fm_directory_view_update_menus (FM_DIRECTORY_VIEW (view));
}

static void
fm_list_view_bump_zoom_level (FMDirectoryView *view, int zoom_increment)
{
	FMListView *list_view;
	NautilusZoomLevel new_level;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);
	new_level = list_view->details->zoom_level + zoom_increment;

	if (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST) {
		fm_list_view_set_zoom_level (list_view, new_level, FALSE);
	}
}

static NautilusZoomLevel
fm_list_view_get_zoom_level (FMDirectoryView *view)
{
	FMListView *list_view;

	g_return_val_if_fail (FM_IS_LIST_VIEW (view), NAUTILUS_ZOOM_LEVEL_STANDARD);

	list_view = FM_LIST_VIEW (view);

	return list_view->details->zoom_level;
}

static void
fm_list_view_zoom_to_level (FMDirectoryView *view,
			    NautilusZoomLevel zoom_level)
{
	FMListView *list_view;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);

	fm_list_view_set_zoom_level (list_view, zoom_level, FALSE);
}

static void
fm_list_view_restore_default_zoom_level (FMDirectoryView *view)
{
	FMListView *list_view;

	g_return_if_fail (FM_IS_LIST_VIEW (view));

	list_view = FM_LIST_VIEW (view);

	fm_list_view_set_zoom_level (list_view, get_default_zoom_level (), FALSE);
}

static gboolean 
fm_list_view_can_zoom_in (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

	return FM_LIST_VIEW (view)->details->zoom_level	< NAUTILUS_ZOOM_LEVEL_LARGEST;
}

static gboolean 
fm_list_view_can_zoom_out (FMDirectoryView *view) 
{
	g_return_val_if_fail (FM_IS_LIST_VIEW (view), FALSE);

	return FM_LIST_VIEW (view)->details->zoom_level > NAUTILUS_ZOOM_LEVEL_SMALLEST;
}

static void
fm_list_view_start_renaming_file (FMDirectoryView *view, NautilusFile *file)
{
	FMListView *list_view;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkEntry *entry;
	int start_offset, end_offset;
	
	list_view = FM_LIST_VIEW (view);
	
	/* Don't start renaming if another rename in this listview is
	 * already in progress. */
	if (list_view->details->file_name_column && list_view->details->file_name_column->editable_widget) {
		return;
	}

	if (!fm_list_model_get_tree_iter_from_file (list_view->details->model, file, &iter)) {
		return;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

	/*Make filename-cells editable.*/
	g_object_set (G_OBJECT (list_view->details->file_name_cell),
		      "editable", TRUE,
		      NULL);

	
	gtk_tree_view_scroll_to_cell (list_view->details->tree_view,
				      path,
				      list_view->details->file_name_column,
				      FALSE, 0.0, 1.0);
	gtk_tree_view_set_cursor (list_view->details->tree_view,
				  path,
				  list_view->details->file_name_column,
				  TRUE);

	entry = GTK_ENTRY (list_view->details->file_name_column->editable_widget);
	eel_filename_get_rename_region (gtk_entry_get_text (entry),
					&start_offset, &end_offset);
	gtk_editable_select_region (GTK_EDITABLE (entry), start_offset, end_offset);
	
	gtk_tree_path_free (path);

	nautilus_clipboard_set_up_editable
		(GTK_EDITABLE (entry),
		 fm_directory_view_get_ui_manager (view),
		 FALSE);
}

static void
fm_list_view_click_policy_changed (FMDirectoryView *directory_view)
{
	FMListView *view;

	view = FM_LIST_VIEW (directory_view);

	if (click_policy_auto_value == NAUTILUS_CLICK_POLICY_SINGLE) {
		g_object_set (G_OBJECT (view->details->file_name_cell),
			      "underline", PANGO_UNDERLINE_SINGLE,
			      NULL);
	} else {
		g_object_set (G_OBJECT (view->details->file_name_cell),
			      "underline", PANGO_UNDERLINE_NONE,
			      NULL);
	}

	gtk_tree_model_foreach (GTK_TREE_MODEL (view->details->model),
				list_view_changed_foreach, NULL);
}

static void
icons_changed_callback (GObject *icon_factory,
			gpointer callback_data)
{
	FMListView *view;

	view = FM_LIST_VIEW (callback_data);

	gtk_tree_model_foreach (GTK_TREE_MODEL (view->details->model),
				list_view_changed_foreach, NULL);
}


static void
default_sort_order_changed_callback (gpointer callback_data)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (callback_data);

	set_sort_order_from_metadata_and_preferences (list_view);
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (callback_data);

	set_zoom_level_from_metadata_and_preferences (list_view);
}

static void
default_visible_columns_changed_callback (gpointer callback_data)
{
	FMListView *list_view;
	
	list_view = FM_LIST_VIEW (callback_data);
	
	set_visible_columns_from_metadata_and_preferences (list_view);
}

static void
default_column_order_changed_callback (gpointer callback_data)
{
	FMListView *list_view;
	
	list_view = FM_LIST_VIEW (callback_data);
	
	set_column_order_from_metadata_and_preferences (list_view);
}

static void
fm_list_view_sort_directories_first_changed (FMDirectoryView *view)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (view);

	fm_list_model_set_should_sort_directories_first (list_view->details->model,
							 fm_directory_view_should_sort_directories_first (view));
}

static void
fm_list_view_sort_files (FMDirectoryView *view, GList **files)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (view);

	fm_list_model_sort_files (list_view->details->model, files);
}

static void
fm_list_view_dispose (GObject *object)
{
	FMListView *list_view;
	GtkUIManager *ui_manager;

	list_view = FM_LIST_VIEW (object);

	if (list_view->details->model) {
		g_object_unref (list_view->details->model);
		list_view->details->model = NULL;
	}

	if (list_view->details->drag_dest) {
		g_object_unref (list_view->details->drag_dest);
		list_view->details->drag_dest = NULL;
	}

	ui_manager = fm_directory_view_get_ui_manager (FM_DIRECTORY_VIEW (list_view));
	if (ui_manager != NULL) {
		nautilus_ui_unmerge_ui (ui_manager,
					&list_view->details->list_merge_id,
					&list_view->details->list_action_group);
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
fm_list_view_finalize (GObject *object)
{
	FMListView *list_view;

	list_view = FM_LIST_VIEW (object);

	if (list_view->details->double_click_path[0]) {
		gtk_tree_path_free (list_view->details->double_click_path[0]);
	}	
	if (list_view->details->double_click_path[1]) {
		gtk_tree_path_free (list_view->details->double_click_path[1]);
	}

	gtk_target_list_unref (list_view->details->source_target_list);
	
	fm_list_view_flush_typeselect_state (list_view);

	g_list_free (list_view->details->cells);
	g_hash_table_destroy (list_view->details->columns);

	g_free (list_view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
fm_list_view_emblems_changed (FMDirectoryView *directory_view)
{
	g_assert (FM_IS_LIST_VIEW (directory_view));

#if GNOME2_CONVERSION_COMPLETE
	/* FIXME: This needs to update the emblems of the icons, since
	 * relative emblems may have changed.
	 */
#endif
}

static char *
fm_list_view_get_first_visible_file (NautilusView *view)
{
	NautilusFile *file;
	GtkTreePath *path;
	GtkTreeIter iter;
	FMListView *list_view;

	list_view = FM_LIST_VIEW (view);

	if (gtk_tree_view_get_path_at_pos (list_view->details->tree_view,
					   0, 0,
					   &path, NULL, NULL, NULL)) {
		gtk_tree_model_get_iter (GTK_TREE_MODEL (list_view->details->model),
					 &iter, path);

		gtk_tree_path_free (path);
	
		gtk_tree_model_get (GTK_TREE_MODEL (list_view->details->model),
				    &iter,
				    FM_LIST_MODEL_FILE_COLUMN, &file,
				    -1);
		if (file) {
			char *uri;
			
			uri = nautilus_file_get_uri (file);
			
			nautilus_file_unref (file);
			
			return uri;
		}
	}
	
	return NULL;
}

static void
fm_list_view_scroll_to_file (FMListView *view,
			     NautilusFile *file)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!fm_list_model_get_tree_iter_from_file (view->details->model, file, &iter)) {
		return;
	}
		
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->model), &iter);

	gtk_tree_view_scroll_to_cell (view->details->tree_view,
				      path, NULL,
				      TRUE, 0.0, 0.0);
	
	gtk_tree_path_free (path);
}

static void
list_view_scroll_to_file (NautilusView *view,
			  const char *uri)
{
	NautilusFile *file;

	if (uri != NULL) {
		file = nautilus_file_get (uri);
		fm_list_view_scroll_to_file (FM_LIST_VIEW (view), file);
		nautilus_file_unref (file);
	}
}


static void
fm_list_view_class_init (FMListViewClass *class)
{
	FMDirectoryViewClass *fm_directory_view_class;

	fm_directory_view_class = FM_DIRECTORY_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = fm_list_view_dispose;
	G_OBJECT_CLASS (class)->finalize = fm_list_view_finalize;

	fm_directory_view_class->add_file = fm_list_view_add_file;
	fm_directory_view_class->begin_loading = fm_list_view_begin_loading;
	fm_directory_view_class->bump_zoom_level = fm_list_view_bump_zoom_level;
	fm_directory_view_class->can_zoom_in = fm_list_view_can_zoom_in;
	fm_directory_view_class->can_zoom_out = fm_list_view_can_zoom_out;
        fm_directory_view_class->click_policy_changed = fm_list_view_click_policy_changed;
	fm_directory_view_class->clear = fm_list_view_clear;
	fm_directory_view_class->file_changed = fm_list_view_file_changed;
	fm_directory_view_class->get_background_widget = fm_list_view_get_background_widget;
	fm_directory_view_class->get_selection = fm_list_view_get_selection;
	fm_directory_view_class->get_item_count = fm_list_view_get_item_count;
	fm_directory_view_class->is_empty = fm_list_view_is_empty;
	fm_directory_view_class->remove_file = fm_list_view_remove_file;
	fm_directory_view_class->merge_menus = fm_list_view_merge_menus;
	fm_directory_view_class->update_menus = fm_list_view_update_menus;
	fm_directory_view_class->reset_to_defaults = fm_list_view_reset_to_defaults;
	fm_directory_view_class->restore_default_zoom_level = fm_list_view_restore_default_zoom_level;
	fm_directory_view_class->reveal_selection = fm_list_view_reveal_selection;
	fm_directory_view_class->select_all = fm_list_view_select_all;
	fm_directory_view_class->set_selection = fm_list_view_set_selection;
	fm_directory_view_class->sort_files = fm_list_view_sort_files;
	fm_directory_view_class->sort_directories_first_changed = fm_list_view_sort_directories_first_changed;
	fm_directory_view_class->start_renaming_file = fm_list_view_start_renaming_file;
	fm_directory_view_class->get_zoom_level = fm_list_view_get_zoom_level;
	fm_directory_view_class->zoom_to_level = fm_list_view_zoom_to_level;
        fm_directory_view_class->emblems_changed = fm_list_view_emblems_changed;

	eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_CLICK_POLICY,
				       &click_policy_auto_value);
	eel_preferences_add_auto_string (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
					 (const char **) &default_sort_order_auto_value);
	eel_preferences_add_auto_boolean (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
					  &default_sort_reversed_auto_value);
	eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
				       (int *) &default_zoom_level_auto_value);
	eel_preferences_add_auto_string_glist (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
					       (const GList **) &default_visible_columns_auto_value);
	eel_preferences_add_auto_string_glist (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
					       (const GList **) &default_column_order_auto_value);
}

static const char *
fm_list_view_get_id (NautilusView *view)
{
	return FM_LIST_VIEW_ID;
}


static void
fm_list_view_iface_init (NautilusViewIface *iface)
{
	fm_directory_view_init_view_iface (iface);
	
	iface->get_view_id = fm_list_view_get_id;
	iface->get_first_visible_file = fm_list_view_get_first_visible_file;
	iface->scroll_to_file = list_view_scroll_to_file;
	iface->get_title = NULL;
}


static void
fm_list_view_init (FMListView *list_view)
{
	list_view->details = g_new0 (FMListViewDetails, 1);

	create_and_set_up_tree_view (list_view);

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
						  default_sort_order_changed_callback,
						  list_view, G_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
						  default_sort_order_changed_callback,
						  list_view, G_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
						  default_zoom_level_changed_callback,
						  list_view, G_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
						  default_visible_columns_changed_callback,
						  list_view, G_OBJECT (list_view));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
						  default_column_order_changed_callback,
						  list_view, G_OBJECT (list_view));

	fm_list_view_click_policy_changed (FM_DIRECTORY_VIEW (list_view));
	
	fm_list_view_sort_directories_first_changed (FM_DIRECTORY_VIEW (list_view));
	
	g_signal_connect_object 
		(nautilus_icon_factory_get (),
		 "icons_changed",
		 G_CALLBACK (icons_changed_callback),
		 list_view, 0);
	
	list_view->details->type_select_state = NULL;
}

static NautilusView *
fm_list_view_create (NautilusWindowInfo *window)
{
	FMListView *view;

	view = g_object_new (FM_TYPE_LIST_VIEW, "window", window, NULL);
	g_object_ref (view);
	gtk_object_sink (GTK_OBJECT (view));
	return NAUTILUS_VIEW (view);
}

static gboolean
fm_list_view_supports_uri (const char *uri,
			   GnomeVFSFileType file_type,
			   const char *mime_type)
{
	if (file_type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (g_str_has_prefix (uri, "trash:")) {
		return TRUE;
	}

	return FALSE;
}

static NautilusViewInfo fm_list_view = {
	FM_LIST_VIEW_ID,
	N_("List"),
	N_("_List"),
	fm_list_view_create,
	fm_list_view_supports_uri
};

void
fm_list_view_register (void)
{
	fm_list_view.label = _(fm_list_view.label);
	fm_list_view.label_with_mnemonic = _(fm_list_view.label_with_mnemonic);
	
	nautilus_view_factory_register (&fm_list_view);
}
