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
static int		     bookmark_list_changed_signalID;
static NautilusBookmarkList *bookmarks = NULL;
static GtkWidget	    *bookmark_list_widget = NULL; /* awkward name to distinguish from NautilusBookmarkList */
static GtkWidget	    *name_field = NULL;
static int		     name_field_changed_signalID;
static GtkWidget	    *remove_button = NULL;
static gboolean		     text_changed = FALSE;
static GtkWidget	    *uri_field = NULL;
static int		     uri_field_changed_signalID;

/* forward declarations */
static NautilusBookmark *get_selected_bookmark (void);
static guint	get_selected_row	      (void);
static gboolean get_selection_exists 	      (void);
static void     name_or_uri_field_activate    (NautilusEntry *entry);
static void	nautilus_bookmarks_window_restore_geometry
					      (GtkWidget *window);
static void	on_bookmark_list_changed       (NautilusBookmarkList *, 
					       gpointer user_data);
static void	on_name_field_changed 	      (GtkEditable *, gpointer user_data);
static void	on_remove_button_clicked      (GtkButton *, gpointer user_data);
static void	on_row_move 		      (GtkCList *,
	     				       int old_row,
	     				       int new_row,
	     				       gpointer user_data);
static void	on_select_row 		      (GtkCList	*,
	       				       int row,
	       				       int column,
	       				       GdkEventButton *,
	       				       gpointer user_data);
static gboolean	on_text_field_focus_out_event (GtkWidget *, 
					       GdkEventFocus *, 
					       gpointer user_data);
static void	on_uri_field_changed 	      (GtkEditable *, gpointer user_data);
static gboolean on_window_delete_event 	      (GtkWidget *, 
					       GdkEvent *, 
					       gpointer user_data);
static void     on_window_hide_event 	      (GtkWidget *, 
					       gpointer user_data);
static void     on_window_destroy_event       (GtkWidget *, 
					       gpointer user_data);
static void	repopulate		      (void);
static void	set_up_close_accelerator      (GtkWidget *window);

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

/**
 * create_bookmarks_window:
 * 
 * Create a new bookmark-editing window. 
 * @list: The NautilusBookmarkList that this window will edit.
 *
 * Return value: A pointer to the new window.
 **/
GtkWindow *
create_bookmarks_window (NautilusBookmarkList *list, GtkObject *undo_manager_source)
{
	GtkWidget *window;
	GtkWidget *content_area;
	GtkWidget *list_scroller;
	GtkWidget *right_side;
	GtkWidget *vbox3;
	GtkWidget *name_label;
	GtkWidget *vbox4;
	GtkWidget *url_label;
	GtkWidget *hbox2;

	bookmarks = list;

	window = gnome_dialog_new (_("Bookmarks"), _("Done"), NULL);
	gnome_dialog_close_hides (GNOME_DIALOG (window), TRUE);
	gnome_dialog_set_close (GNOME_DIALOG (window), TRUE);
	
	set_up_close_accelerator (window);
	nautilus_undo_share_undo_manager (GTK_OBJECT (window), undo_manager_source);
	gtk_window_set_wmclass (GTK_WINDOW (window), "bookmarks", "Nautilus");
	gtk_widget_set_usize (window, 
			      BOOKMARKS_WINDOW_MIN_WIDTH, 
			      BOOKMARKS_WINDOW_MIN_HEIGHT);
	nautilus_bookmarks_window_restore_geometry (window);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	
	content_area = gtk_hbox_new (TRUE, GNOME_PAD);
	gtk_widget_show (content_area);
	gtk_box_pack_start (GTK_BOX ((GNOME_DIALOG (window))->vbox), content_area, FALSE, FALSE, 0);

	list_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (list_scroller);
	gtk_box_pack_start (GTK_BOX (content_area), list_scroller, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	bookmark_list_widget = gtk_clist_new (BOOKMARK_LIST_COLUMN_COUNT);
	gtk_widget_show (bookmark_list_widget);
	gtk_container_add (GTK_CONTAINER (list_scroller), bookmark_list_widget);
	gtk_clist_column_titles_hide (GTK_CLIST (bookmark_list_widget));
	gtk_clist_set_column_width (GTK_CLIST (bookmark_list_widget),
				    BOOKMARK_LIST_COLUMN_ICON,
				    NAUTILUS_ICON_SIZE_SMALLER);
	gtk_clist_set_row_height (GTK_CLIST (bookmark_list_widget),
				  NAUTILUS_ICON_SIZE_SMALLER);
	gtk_clist_set_reorderable(GTK_CLIST (bookmark_list_widget), TRUE);
	gtk_clist_set_use_drag_icons(GTK_CLIST (bookmark_list_widget), FALSE);

	right_side = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_widget_show (right_side);
	gtk_box_pack_start (GTK_BOX (content_area), right_side, TRUE, TRUE, 0);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (right_side), vbox3, FALSE, FALSE, 0);

	name_label = gtk_label_new (_("Name"));
	gtk_widget_show (name_label);
	gtk_box_pack_start (GTK_BOX (vbox3), name_label, FALSE, FALSE, 0);

	name_field = nautilus_entry_new ();
	gtk_widget_show (name_field);
	gtk_box_pack_start (GTK_BOX (vbox3), name_field, FALSE, FALSE, 0);
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (name_field), TRUE);
	
	vbox4 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox4);
	gtk_box_pack_start (GTK_BOX (right_side), vbox4, FALSE, FALSE, 0);

	url_label = gtk_label_new (_("Location"));
	gtk_widget_show (url_label);
	gtk_box_pack_start (GTK_BOX (vbox4), url_label, FALSE, FALSE, 0);

	uri_field = nautilus_entry_new ();
	gtk_widget_show (uri_field);
	gtk_box_pack_start (GTK_BOX (vbox4), uri_field, FALSE, FALSE, 0);	
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (uri_field), TRUE);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (right_side), hbox2, FALSE, FALSE, 0);

	remove_button = gtk_button_new_with_label (_("Remove"));
	eel_gtk_button_set_standard_padding (GTK_BUTTON (remove_button));
	gtk_widget_show (remove_button);
	gtk_box_pack_start (GTK_BOX (hbox2), remove_button, TRUE, FALSE, 0);

	bookmark_list_changed_signalID =
		gtk_signal_connect (GTK_OBJECT(bookmarks), "contents_changed",
				    GTK_SIGNAL_FUNC(on_bookmark_list_changed),
				    NULL);
				    
	gtk_signal_connect (GTK_OBJECT(bookmark_list_widget), "row_move",
			    GTK_SIGNAL_FUNC(on_row_move),
			    NULL);
			    
	gtk_signal_connect (GTK_OBJECT(bookmark_list_widget), "select_row",
			    GTK_SIGNAL_FUNC(on_select_row),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                    	    GTK_SIGNAL_FUNC (on_window_delete_event),
                    	    NULL);
                    	    
	gtk_signal_connect (GTK_OBJECT (window), "hide",
                    	    on_window_hide_event,
                    	    NULL);
                    	    
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
                    	    on_window_destroy_event,
                    	    NULL);
                    	    
	name_field_changed_signalID =
		gtk_signal_connect (GTK_OBJECT (name_field), "changed",
                	            GTK_SIGNAL_FUNC (on_name_field_changed),
                      		    NULL);
                      		    
	gtk_signal_connect (GTK_OBJECT (name_field), "focus_out_event",
      	              	    GTK_SIGNAL_FUNC (on_text_field_focus_out_event),
                            NULL);
                            
	gtk_signal_connect (GTK_OBJECT (name_field), "activate",
      	              	    GTK_SIGNAL_FUNC (name_or_uri_field_activate),
                            NULL);

	uri_field_changed_signalID = 
		gtk_signal_connect (GTK_OBJECT (uri_field), "changed",
                	    	    GTK_SIGNAL_FUNC (on_uri_field_changed),
                      		    NULL);
                      		    
	gtk_signal_connect (GTK_OBJECT (uri_field), "focus_out_event",
        	            GTK_SIGNAL_FUNC (on_text_field_focus_out_event),
              	    	    NULL);
              	    	    
	gtk_signal_connect (GTK_OBJECT (uri_field), "activate",
      	              	    GTK_SIGNAL_FUNC (name_or_uri_field_activate),
                            NULL);

	gtk_signal_connect (GTK_OBJECT (remove_button), "clicked",
        	            GTK_SIGNAL_FUNC (on_remove_button_clicked),
                      	    NULL);

	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       repopulate,
					       GTK_OBJECT (window));
                      	    

	/* Set selection mode after connecting signal to notice initial selected row. */
	gtk_clist_set_selection_mode(GTK_CLIST (bookmark_list_widget), 
				     GTK_SELECTION_BROWSE);

	/* Fill in list widget with bookmarks, must be after signals are wired up. */
	repopulate();

	return GTK_WINDOW (window);
}

static NautilusBookmark *
get_selected_bookmark ()
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK_LIST(bookmarks), NULL);

	return nautilus_bookmark_list_item_at(bookmarks, get_selected_row());
}

static guint
get_selected_row ()
{
	g_assert(get_selection_exists());
	return GPOINTER_TO_UINT(g_list_nth_data(
		GTK_CLIST(bookmark_list_widget)->selection, 0));
}

static gboolean
get_selection_exists ()
{
	g_assert (GTK_CLIST(bookmark_list_widget)->selection_mode 
		== GTK_SELECTION_BROWSE);
	return GTK_CLIST(bookmark_list_widget)->rows > 0;
}

static void
install_bookmark_icon (NautilusBookmark *bookmark, int row)
{
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;

	if (!nautilus_bookmark_get_pixmap_and_mask (bookmark,
		  				    NAUTILUS_ICON_SIZE_SMALLER,
						    &pixmap,
						    &bitmap))
	{
		return;
	}

	gtk_clist_set_pixmap (GTK_CLIST (bookmark_list_widget),	
			      row,
			      BOOKMARK_LIST_COLUMN_ICON,
			      pixmap,
			      bitmap);
}

static void
nautilus_bookmarks_window_restore_geometry (GtkWidget *window)
{
	const char *window_geometry;
	
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));

	window_geometry = nautilus_bookmark_list_get_window_geometry (bookmarks);

	if (window_geometry != NULL) 
	{	
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
		
		geometry_string = gnome_geometry_string (GTK_WIDGET (window)->window);	
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
	g_return_if_fail(GTK_IS_CLIST(bookmark_list_widget));
	g_return_if_fail(GTK_IS_ENTRY(name_field));
	g_return_if_fail(get_selection_exists());

	/* Update text displayed in list instantly. Also remember that 
	 * user has changed text so we update real bookmark later. 
	 */
	gtk_clist_set_text(GTK_CLIST(bookmark_list_widget), 
			   get_selected_row(), BOOKMARK_LIST_COLUMN_NAME, 
			   gtk_entry_get_text(GTK_ENTRY(name_field)));
	text_changed = TRUE;
}


static void
on_remove_button_clicked (GtkButton *button,
                          gpointer   user_data)
{
	g_assert(GTK_IS_CLIST(bookmark_list_widget));

	/* Turn off list updating since we're handling the list widget explicitly.
	 * This allows the selection to move to the next row, instead of leaping
	 * back to the top.
	 */
	gtk_signal_handler_block(GTK_OBJECT(bookmarks), 
				 bookmark_list_changed_signalID);
	nautilus_bookmark_list_delete_item_at(bookmarks, get_selected_row());
	gtk_signal_handler_unblock(GTK_OBJECT(bookmarks), 
				   bookmark_list_changed_signalID);

	gtk_clist_remove(GTK_CLIST(bookmark_list_widget), get_selected_row());

	/*
	 * If removing the selected row selected the next row, then we'll
	 * get a callback. But if the list is now empty, we won't get a
	 * callback, which will leave the Remove button and text fields
	 * in the wrong state unless we fix them explicitly here.
	 */
	if (nautilus_bookmark_list_length (bookmarks) == 0) {
		repopulate ();
	}
}


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
	gtk_signal_handler_block(GTK_OBJECT(bookmarks), 
				 bookmark_list_changed_signalID);
	nautilus_bookmark_list_delete_item_at(bookmarks, old_row);
	nautilus_bookmark_list_insert_item(bookmarks, bookmark, new_row);
	gtk_signal_handler_unblock(GTK_OBJECT(bookmarks), 
				   bookmark_list_changed_signalID);

	gtk_object_unref(GTK_OBJECT(bookmark));
}

static void
on_select_row (GtkCList	       *clist,
	       int		row,
	       int	 	column,
	       GdkEventButton  *event,
	       gpointer		user_data)
{
	NautilusBookmark *selected;
	char *name, *uri;

	g_assert (GTK_IS_ENTRY (name_field));
	g_assert (GTK_IS_ENTRY (uri_field));

	/* Workaround for apparent GtkCList bug. See bugzilla.gnome.org 47846. */
	if (clist->rows <= row) {
		return;
	}

	selected = get_selected_bookmark ();
	name = nautilus_bookmark_get_name (selected);
	uri = nautilus_bookmark_get_uri (selected);
	
	nautilus_entry_set_text (NAUTILUS_ENTRY (name_field), name);
	nautilus_entry_set_text (NAUTILUS_ENTRY (uri_field), uri);

	g_free (name);
	g_free (uri);
}


static void
update_bookmark_from_text ()
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
		gtk_signal_handler_block (GTK_OBJECT (bookmarks), 
					  bookmark_list_changed_signalID);
		nautilus_bookmark_list_delete_item_at (bookmarks, selected_row);
		nautilus_bookmark_list_insert_item (bookmarks, bookmark, selected_row);
		gtk_signal_handler_unblock (GTK_OBJECT (bookmarks), 
					    bookmark_list_changed_signalID);

		gtk_object_unref (GTK_OBJECT (bookmark));
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

static void
save_geometry_and_hide (GtkWindow *window)
{
	g_assert (GTK_IS_WINDOW (window));

	nautilus_bookmarks_window_save_geometry (window);
	gtk_widget_hide (GTK_WIDGET (window));
}


static gboolean
on_window_delete_event (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	save_geometry_and_hide (GTK_WINDOW (widget));
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
	nautilus_undo_unregister (GTK_OBJECT (name_field));
	nautilus_undo_unregister (GTK_OBJECT (uri_field));

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
	GtkCList *clist;
	guint index;
	gboolean selection_exists;

	g_assert (GTK_IS_CLIST (bookmark_list_widget));
	g_assert (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	
	clist = GTK_CLIST (bookmark_list_widget);

	/* Freeze while mucking with content so it's not flashy */
	gtk_clist_freeze (clist);
	    
	/* Empty the list. */
	gtk_clist_clear (clist);
	   
	/* Fill the list in with the bookmark names. */
	for (index = 0; index < nautilus_bookmark_list_length(bookmarks); ++index) {
		char *text[BOOKMARK_LIST_COLUMN_COUNT];
		char *bookmark_name;
		NautilusBookmark *bookmark;
		int new_row;

		bookmark = nautilus_bookmark_list_item_at(bookmarks, index);
		bookmark_name = nautilus_bookmark_get_name (bookmark);
		text[BOOKMARK_LIST_COLUMN_ICON] = NULL;
		text[BOOKMARK_LIST_COLUMN_NAME] = bookmark_name;
		new_row = gtk_clist_append (clist, text);
		g_free (bookmark_name);
		
		install_bookmark_icon (bookmark, new_row);
	}
	
	/* Set the sensitivity of widgets that require a selection */
	selection_exists = get_selection_exists();
	gtk_widget_set_sensitive (remove_button, selection_exists);
	gtk_widget_set_sensitive (name_field, selection_exists);
	gtk_widget_set_sensitive (uri_field, selection_exists);
	    
	if (!selection_exists) {
		/* Block signals to avoid modifying non-existent selected item. */
		gtk_signal_handler_block (GTK_OBJECT (name_field), 
					  name_field_changed_signalID);
		nautilus_entry_set_text (NAUTILUS_ENTRY (name_field), "");
		gtk_signal_handler_unblock (GTK_OBJECT (name_field), 
					    name_field_changed_signalID);

		gtk_signal_handler_block (GTK_OBJECT (uri_field), 
					  uri_field_changed_signalID);
		nautilus_entry_set_text (NAUTILUS_ENTRY (uri_field), "");
		gtk_signal_handler_unblock (GTK_OBJECT (uri_field), 
					    uri_field_changed_signalID);
	}
	  
	gtk_clist_thaw (GTK_CLIST (bookmark_list_widget));
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
		save_geometry_and_hide (window);
		gtk_signal_emit_stop_by_name 
			(GTK_OBJECT (window), "key_press_event");
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
	gtk_signal_connect (GTK_OBJECT (window),
			    "key_press_event",
			    GTK_SIGNAL_FUNC (handle_close_accelerator),
			    NULL);
}
