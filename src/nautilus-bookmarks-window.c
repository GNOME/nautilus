/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-bookmarks-window.c - implementation of bookmark-editing window.
 */

#include <config.h>
#include "nautilus-bookmarks-window.h"
#include <libnautilus/nautilus-undo.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-gtk-extensions.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#include <gnome.h>


/* Static variables to keep track of window state. If there were
 * more than one bookmark-editing window, these would be struct or
 * class fields. 
 */
static int		     bookmark_list_changed_signal_id;
static NautilusBookmarkList *bookmarks = NULL;
static GtkTreeView	    *bookmark_list_widget = NULL; /* awkward name to distinguish from NautilusBookmarkList */
static GtkListStore	    *bookmark_list_store = NULL;
static GtkTreeSelection     *bookmark_selection = NULL;
static int                   selection_changed_id = 0;
static GtkWidget	    *name_field = NULL;
static int		     name_field_changed_signal_id;
static GtkWidget	    *remove_button = NULL;
static gboolean		     text_changed = FALSE;
static GtkWidget	    *uri_field = NULL;
static int		     uri_field_changed_signal_id;


/* forward declarations */
static guint    get_selected_row                            (void);
static gboolean get_selection_exists                        (void);
static void     name_or_uri_field_activate                  (NautilusEntry        *entry);
static void     nautilus_bookmarks_window_restore_geometry  (GtkWidget            *window);
static void     on_bookmark_list_changed                    (NautilusBookmarkList *list,
							     gpointer              user_data);
static void     on_name_field_changed                       (GtkEditable          *editable,
							     gpointer              user_data);
static void     on_remove_button_clicked                    (GtkButton            *button,
							     gpointer              user_data);
#if GNOME2_CONVERSION_COMPLETE
static void     on_row_move                                 (GtkCList             *clist,
							     int                   old_row,
							     int                   new_row,
							     gpointer              user_data);
#endif
static void     on_selection_changed                        (GtkTreeSelection     *treeselection,
							     gpointer              user_data);

static gboolean on_text_field_focus_out_event               (GtkWidget            *widget,
							     GdkEventFocus        *event,
							     gpointer              user_data);
static void     on_uri_field_changed                        (GtkEditable          *editable,
							     gpointer              user_data);
static gboolean on_window_delete_event                      (GtkWidget            *widget,
							     GdkEvent             *event,
							     gpointer              user_data);
static void     on_window_hide_event                        (GtkWidget            *widget,
							     gpointer              user_data);
static void     on_window_destroy_event                     (GtkWidget            *widget,
							     gpointer              user_data);
static void     repopulate                                  (void);
static void     set_up_close_accelerator                    (GtkWidget            *window);

#define BOOKMARK_LIST_COLUMN_ICON		0
#define BOOKMARK_LIST_COLUMN_NAME		1
#define BOOKMARK_LIST_COLUMN_COUNT		2

/* layout constants */

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define BOOKMARKS_WINDOW_MIN_WIDTH	300
#define BOOKMARKS_WINDOW_MIN_HEIGHT	100

/* Larger size initially; user can stretch or shrink (but not shrink below min) */
#define BOOKMARKS_WINDOW_INITIAL_WIDTH	500
#define BOOKMARKS_WINDOW_INITIAL_HEIGHT	200

static void
nautilus_bookmarks_window_response_callback (GtkDialog *dialog,
					     int response_id,
					     gpointer callback_data)
{
	gtk_widget_hide (GTK_WIDGET (dialog));
}

/**
 * create_bookmarks_window:
 * 
 * Create a new bookmark-editing window. 
 * @list: The NautilusBookmarkList that this window will edit.
 *
 * Return value: A pointer to the new window.
 **/
GtkWindow *
create_bookmarks_window (NautilusBookmarkList *list, GObject *undo_manager_source)
{
	GtkWidget         *window;
	GtkWidget         *content_area;
	GtkWidget         *list_scroller;
	GtkWidget         *right_side;
	GtkWidget         *vbox3;
	GtkWidget         *name_label;
	GtkWidget         *vbox4;
	GtkWidget         *url_label;
	GtkWidget         *hbox2;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *rend;

	bookmarks = list;

	window = gtk_dialog_new_with_buttons (_("Bookmarks"), NULL, 0,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
					      NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (window), FALSE);
	set_up_close_accelerator (window);
	nautilus_undo_share_undo_manager (G_OBJECT (window), undo_manager_source);
	gtk_window_set_wmclass (GTK_WINDOW (window), "bookmarks", "Nautilus");
	nautilus_bookmarks_window_restore_geometry (window);
	
	content_area = gtk_hbox_new (TRUE, GNOME_PAD);
	gtk_widget_show (content_area);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), content_area, TRUE, TRUE, 0);

	list_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (list_scroller);
	gtk_box_pack_start (GTK_BOX (content_area), list_scroller, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (list_scroller), GTK_SHADOW_IN);

	bookmark_list_widget = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_widget_show (GTK_WIDGET (bookmark_list_widget));
	gtk_container_add (GTK_CONTAINER (list_scroller), 
			   GTK_WIDGET (bookmark_list_widget));
	gtk_tree_view_set_headers_visible (bookmark_list_widget, FALSE);
	gtk_tree_view_set_reorderable (bookmark_list_widget, TRUE);
		
	rend = gtk_cell_renderer_pixbuf_new ();
	col = gtk_tree_view_column_new_with_attributes ("Icon", 
							rend,
							"pixbuf", 
							BOOKMARK_LIST_COLUMN_ICON,
							NULL);
	gtk_tree_view_append_column (bookmark_list_widget,
				     GTK_TREE_VIEW_COLUMN (col));
	gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (col),
					      NAUTILUS_ICON_SIZE_SMALLER);
	
	rend = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new_with_attributes ("Icon", 
							rend,
							"text", 
							BOOKMARK_LIST_COLUMN_NAME,
							NULL);
	gtk_tree_view_append_column (bookmark_list_widget,
				     GTK_TREE_VIEW_COLUMN (col));
	
	bookmark_list_store = gtk_list_store_new (BOOKMARK_LIST_COLUMN_COUNT,
						  GDK_TYPE_PIXBUF,
						  G_TYPE_STRING,
						  G_TYPE_INT);
	gtk_tree_view_set_model (bookmark_list_widget,
				 GTK_TREE_MODEL (bookmark_list_store));
	
	bookmark_selection =
		GTK_TREE_SELECTION (gtk_tree_view_get_selection (bookmark_list_widget));
	
	right_side = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_widget_show (right_side);
	gtk_box_pack_start (GTK_BOX (content_area), right_side, TRUE, TRUE, 0);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (right_side), vbox3, FALSE, FALSE, 0);

	name_label = gtk_label_new_with_mnemonic (_("_Name"));
	gtk_widget_show (name_label);
	gtk_box_pack_start (GTK_BOX (vbox3), name_label, FALSE, FALSE, 0);

	name_field = nautilus_entry_new ();
	gtk_widget_show (name_field);
	gtk_box_pack_start (GTK_BOX (vbox3), name_field, FALSE, FALSE, 0);
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (name_field), TRUE);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (name_label), name_field);

	vbox4 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox4);
	gtk_box_pack_start (GTK_BOX (right_side), vbox4, FALSE, FALSE, 0);

	url_label = gtk_label_new_with_mnemonic (_("_Location"));
	gtk_widget_show (url_label);
	gtk_box_pack_start (GTK_BOX (vbox4), url_label, FALSE, FALSE, 0);

	uri_field = nautilus_entry_new ();
	gtk_widget_show (uri_field);
	gtk_box_pack_start (GTK_BOX (vbox4), uri_field, FALSE, FALSE, 0);	
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (uri_field), TRUE);

	gtk_label_set_mnemonic_widget (GTK_LABEL (url_label), uri_field);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (right_side), hbox2, FALSE, FALSE, 0);

	remove_button = gtk_button_new_with_mnemonic (_("_Remove"));

	gtk_widget_show (remove_button);
	gtk_box_pack_start (GTK_BOX (hbox2), remove_button, TRUE, FALSE, 0);

	bookmark_list_changed_signal_id =
		g_signal_connect (bookmarks, "contents_changed",
				  G_CALLBACK (on_bookmark_list_changed), NULL);
#if GNOME2_CONVERSION_COMPLETE
	g_signal_connect (bookmark_list_widget, "row_move",
			  G_CALLBACK (on_row_move), NULL);
#endif	
	selection_changed_id =
		g_signal_connect (bookmark_selection, "changed",
				  G_CALLBACK (on_selection_changed), NULL);	

	g_signal_connect (window, "delete_event",
			  G_CALLBACK (on_window_delete_event), NULL);
	g_signal_connect (window, "hide",
			  G_CALLBACK (on_window_hide_event), NULL);                    	    
	g_signal_connect (window, "destroy",
			  G_CALLBACK (on_window_destroy_event), NULL);
	g_signal_connect (window, "response",
			  G_CALLBACK (nautilus_bookmarks_window_response_callback), NULL);

	name_field_changed_signal_id =
		g_signal_connect (name_field, "changed",
				  G_CALLBACK (on_name_field_changed), NULL);
                      		    
	g_signal_connect (name_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), NULL);                            
	g_signal_connect (name_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), NULL);

	uri_field_changed_signal_id = 
		g_signal_connect (uri_field, "changed",
				  G_CALLBACK (on_uri_field_changed), NULL);
                      		    
	g_signal_connect (uri_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), NULL);
	g_signal_connect (uri_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), NULL);
	g_signal_connect (remove_button, "clicked",
			  G_CALLBACK (on_remove_button_clicked), NULL);

	/* Register to find out about icon theme changes */
	g_signal_connect_object (nautilus_icon_factory_get (), "icons_changed",
				 G_CALLBACK (repopulate), window,
				 G_CONNECT_SWAPPED);
                      	    
	gtk_tree_selection_set_mode (bookmark_selection, GTK_SELECTION_BROWSE);
	
	/* Fill in list widget with bookmarks, must be after signals are wired up. */
	repopulate();

	return GTK_WINDOW (window);
}

static NautilusBookmark *
get_selected_bookmark (void)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK_LIST(bookmarks), NULL);

	return nautilus_bookmark_list_item_at(bookmarks, get_selected_row ());
}

static guint
get_selected_row (void)
{
	GtkTreeIter       iter;
	GtkTreePath      *path;
	GtkTreeModel     *model;
	
	g_assert (get_selection_exists());
	
	model = GTK_TREE_MODEL (bookmark_list_store);
	gtk_tree_selection_get_selected (bookmark_selection,
					 &model,
					 &iter);
	
	path = gtk_tree_model_get_path (model, &iter);
	return atoi (gtk_tree_path_to_string (path));
}

static gboolean
get_selection_exists (void)
{
	return gtk_tree_selection_get_selected (bookmark_selection, NULL, NULL);
}

static void
nautilus_bookmarks_window_restore_geometry (GtkWidget *window)
{
	const char *window_geometry;
	
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));

	window_geometry = nautilus_bookmark_list_get_window_geometry (bookmarks);

	if (window_geometry != NULL) {	
		eel_gtk_window_set_initial_geometry_from_string 
			(GTK_WINDOW (window), window_geometry, 
			 BOOKMARKS_WINDOW_MIN_WIDTH, BOOKMARKS_WINDOW_MIN_HEIGHT);

	} else {
		/* use default since there was no stored geometry */
		gtk_window_set_default_size (GTK_WINDOW (window), 
					     BOOKMARKS_WINDOW_INITIAL_WIDTH, 
					     BOOKMARKS_WINDOW_INITIAL_HEIGHT);

		/* Let window manager handle default position if no position stored */
	}
}

/**
 * nautilus_bookmarks_window_save_geometry:
 * 
 * Save window size & position to disk.
 * @window: The bookmarks window whose geometry should be saved.
 **/
void
nautilus_bookmarks_window_save_geometry (GtkWindow *window)
{
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));

	/* Don't bother if window is already closed */
	if (GTK_WIDGET_VISIBLE (window)) {
		char *geometry_string;
		
		geometry_string = eel_gtk_window_get_geometry_string (window);

		nautilus_bookmark_list_set_window_geometry (bookmarks, geometry_string);
		g_free (geometry_string);
	}
}

static void
on_bookmark_list_changed (NautilusBookmarkList *bookmarks, gpointer data)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));

	/* maybe add logic here or in repopulate to save/restore selection */
	repopulate ();
}

static void
on_name_field_changed (GtkEditable *editable,
		       gpointer     user_data)
{
	GtkTreeIter   iter;
	g_return_if_fail(GTK_IS_TREE_VIEW(bookmark_list_widget));
	g_return_if_fail(GTK_IS_ENTRY(name_field));
	g_return_if_fail(get_selection_exists());

	/* Update text displayed in list instantly. Also remember that 
	 * user has changed text so we update real bookmark later. 
	 */
	gtk_tree_selection_get_selected (bookmark_selection,
					 NULL,
					 &iter);
	
	gtk_list_store_set (bookmark_list_store, 
			    &iter, BOOKMARK_LIST_COLUMN_NAME, 
			    gtk_entry_get_text (GTK_ENTRY (name_field)),
			    -1);
	text_changed = TRUE;
}


static void
on_remove_button_clicked (GtkButton *button,
                          gpointer   user_data)
{
	GtkTreeIter iter;
	guint       selected_row;
	guint       list_length;
	gchar      *row_path;

	g_assert (GTK_IS_TREE_VIEW (bookmark_list_widget));
	

	/* Turn off list updating since we're handling the list widget explicitly.
	 * This allows the selection to move to the next row, instead of leaping
	 * back to the top.
	 */
	g_signal_handler_block(bookmarks,
			       bookmark_list_changed_signal_id);
	selected_row = get_selected_row ();
	nautilus_bookmark_list_delete_item_at(bookmarks, get_selected_row());
	g_signal_handler_unblock(bookmarks,
				 bookmark_list_changed_signal_id);

	/*
	 * If removing the selected row selected the next row, then we'll
	 * get a callback. But if the list is now empty, we won't get a
	 * callback, which will leave the Remove button and text fields
	 * in the wrong state unless we fix them explicitly here.
	 */
	list_length = nautilus_bookmark_list_length (bookmarks);
	if (!list_length) {
		repopulate ();
		return;
	}
	gtk_tree_selection_get_selected (bookmark_selection,
					 NULL,
					 &iter);

	/* Block signals, so our on_selection_changed won't be called */
	g_signal_handler_block (bookmark_selection,
				selection_changed_id);
	
	gtk_list_store_remove (bookmark_list_store, &iter);

	g_signal_handler_unblock (bookmark_selection,
				  selection_changed_id);

	/* If the last item was just removed,
	 * we need to select the previous one
	 */
	if (selected_row >= list_length) {
		row_path = g_strdup_printf ("%d", selected_row-1);
		gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (bookmark_list_store),
						     &iter,
						     row_path);
		g_free (row_path);
	}
	
	gtk_tree_selection_select_iter (bookmark_selection, &iter);
	
}

#if GNOME2_CONVERSION_COMPLETE
static void
on_row_move (GtkCList *clist,
	     int      old_row,
	     int      new_row,
	     gpointer  user_data)
{
	NautilusBookmark *bookmark;

	bookmark = nautilus_bookmark_copy(
		nautilus_bookmark_list_item_at(bookmarks, old_row));

	/* turn off list updating 'cuz otherwise the list-reordering code runs
	 * after repopulate(), thus reordering the correctly-ordered list.
	 */
	g_signal_handler_block(bookmarks,
			       bookmark_list_changed_signal_id);
	nautilus_bookmark_list_delete_item_at(bookmarks, old_row);
	nautilus_bookmark_list_insert_item(bookmarks, bookmark, new_row);
	g_signal_handler_unblock(bookmarks, 
				 bookmark_list_changed_signal_id);

	g_object_unref (bookmark);
}
#endif

static void
on_selection_changed (GtkTreeSelection *treeselection,
		      gpointer user_data)
{
	NautilusBookmark *selected;
	char *name, *uri;
	
	g_assert (GTK_IS_ENTRY (name_field));
	g_assert (GTK_IS_ENTRY (uri_field));

	selected = get_selected_bookmark ();
	name = nautilus_bookmark_get_name (selected);
	uri = nautilus_bookmark_get_uri (selected);
	
	nautilus_entry_set_text (NAUTILUS_ENTRY (name_field), name);
	nautilus_entry_set_text (NAUTILUS_ENTRY (uri_field), uri);

	g_free (name);
	g_free (uri);
}


static void
update_bookmark_from_text (void)
{
	if (text_changed) {
		NautilusBookmark *bookmark;
		guint selected_row;

		g_assert (GTK_IS_ENTRY (name_field));
		g_assert (GTK_IS_ENTRY (uri_field));

		bookmark = nautilus_bookmark_new
			(gtk_entry_get_text (GTK_ENTRY (uri_field)),
			 gtk_entry_get_text (GTK_ENTRY (name_field)));
		selected_row = get_selected_row ();

		/* turn off list updating 'cuz otherwise the list-reordering code runs
		 * after repopulate(), thus reordering the correctly-ordered list.
		 */
		g_signal_handler_block (bookmarks, 
					bookmark_list_changed_signal_id);
		nautilus_bookmark_list_delete_item_at (bookmarks, selected_row);
		nautilus_bookmark_list_insert_item (bookmarks, bookmark, selected_row);
		g_signal_handler_unblock (bookmarks, 
					  bookmark_list_changed_signal_id);

		g_object_unref (bookmark);
	}
}

static gboolean
on_text_field_focus_out_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
	g_assert (NAUTILUS_IS_ENTRY (widget));

	update_bookmark_from_text ();
	return FALSE;
}

static void
name_or_uri_field_activate (NautilusEntry *entry)
{
	g_assert (NAUTILUS_IS_ENTRY (entry));

	update_bookmark_from_text ();
	nautilus_entry_select_all_at_idle (entry);
}

static void
on_uri_field_changed (GtkEditable *editable,
		      gpointer user_data)
{
	/* Remember that user has changed text so we 
	 * update real bookmark later. 
	 */
	text_changed = TRUE;
}

static gboolean
on_window_delete_event (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_widget_hide (widget);
	return TRUE;
}

static gboolean
restore_geometry (gpointer data)
{
	g_assert (GTK_IS_WINDOW (data));

	nautilus_bookmarks_window_restore_geometry (GTK_WIDGET (data));

	/* Don't call this again */
	return FALSE;
}

static void
on_window_hide_event (GtkWidget *widget,
		      gpointer user_data)
{
	nautilus_bookmarks_window_save_geometry (GTK_WINDOW (widget));

	/* Disable undo for entry widgets */
	nautilus_undo_unregister (G_OBJECT (name_field));
	nautilus_undo_unregister (G_OBJECT (uri_field));

	/* restore_geometry only works after window is hidden */
	gtk_idle_add (restore_geometry, widget);
}

static void
on_window_destroy_event (GtkWidget *widget,
		      	 gpointer user_data)
{
	g_message ("destroying bookmarks window");
	gtk_idle_remove_by_data (widget);
}

static void
repopulate (void)
{
	GtkListStore *store;
	guint         index;
	GtkTreeIter   iter;
	gboolean      selection_exists;
	
	g_assert (GTK_IS_TREE_VIEW (bookmark_list_widget));
	g_assert (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	
	store = GTK_LIST_STORE (bookmark_list_store);

	g_signal_handler_block (bookmark_selection,
				selection_changed_id);
	
	gtk_list_store_clear (store);
	
	g_signal_handler_unblock (bookmark_selection,
				  selection_changed_id);
	
	/* Fill the list in with the bookmark names. */
	for (index = 0; index < nautilus_bookmark_list_length(bookmarks); ++index) {
		NautilusBookmark *bookmark;
		char             *bookmark_name;
		GdkPixbuf        *bookmark_pixbuf;
		GtkTreeIter       iter;

		bookmark = nautilus_bookmark_list_item_at (bookmarks, index);
		bookmark_name = nautilus_bookmark_get_name (bookmark);
		bookmark_pixbuf = nautilus_bookmark_get_pixbuf (bookmark,
								NAUTILUS_ICON_SIZE_SMALLER,
								TRUE);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    BOOKMARK_LIST_COLUMN_ICON, bookmark_pixbuf,
				    BOOKMARK_LIST_COLUMN_NAME, bookmark_name,
				    -1);
		
		g_free (bookmark_name);
		g_object_unref (bookmark_pixbuf);
		
	}

	/* Select the first row on start-up */
	if (index) {
		gtk_tree_model_get_iter_root (GTK_TREE_MODEL (bookmark_list_store), &iter);
		gtk_tree_selection_select_iter (bookmark_selection, &iter);
	}
	
	/* Set the sensitivity of widgets that require a selection */
	selection_exists = get_selection_exists ();
	gtk_widget_set_sensitive (remove_button, selection_exists);
	gtk_widget_set_sensitive (name_field, selection_exists);
	gtk_widget_set_sensitive (uri_field, selection_exists);
	    
	if (!selection_exists) {
		/* Block signals to avoid modifying nonexistent selected item. */
		g_signal_handler_block (name_field, 
					name_field_changed_signal_id);
		nautilus_entry_set_text (NAUTILUS_ENTRY (name_field), "");
		g_signal_handler_unblock (name_field, 
					  name_field_changed_signal_id);

		g_signal_handler_block (uri_field, 
					uri_field_changed_signal_id);
		nautilus_entry_set_text (NAUTILUS_ENTRY (uri_field), "");
		g_signal_handler_unblock (uri_field,
					  uri_field_changed_signal_id);
	}
	  
}

static int
handle_close_accelerator (GtkWindow *window, 
			  GdkEventKey *event, 
			  gpointer user_data)
{
	g_assert (GTK_IS_WINDOW (window));
	g_assert (event != NULL);
	g_assert (user_data == NULL);

	if (eel_gtk_window_event_is_close_accelerator (window, event)) {		
		gtk_widget_hide (GTK_WIDGET (window));
		return TRUE;
	}

	return FALSE;
}

static void
set_up_close_accelerator (GtkWidget *window)
{
	/* Note that we don't call eel_gtk_window_set_up_close_accelerator
	 * here because we have to handle saving geometry before hiding the
	 * window.
	 */
	g_signal_connect (window, "key_press_event",
			  G_CALLBACK (handle_close_accelerator), NULL);
}
