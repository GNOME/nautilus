/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmarks-window.c - implementation of bookmark-editing window.

   Copyright (C) 1999, 2000 Eazel, Inc.

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
*/

#include <config.h>
#include "nautilus-bookmarks-window.h"
#include <libnautilus/nautilus-undo.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-undo-signal-handlers.h>
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
static GtkWidget	    *show_static_checkbox = NULL;
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
static gboolean	on_text_field_focus_in_event  (GtkWidget *, 
					       GdkEventFocus *, 
					       gpointer user_data);
static gboolean	on_text_field_focus_out_event (GtkWidget *, 
					       GdkEventFocus *, 
					       gpointer user_data);
static void	on_uri_field_changed 	      (GtkEditable *, gpointer user_data);
static gboolean on_window_delete_event 	      (GtkWidget *, 
					       GdkEvent *, 
					       gpointer user_data);
static void	repopulate		      (void);

static void	update_built_in_bookmarks_checkbox_to_match_preference (gpointer user_data);
static void	update_built_in_bookmarks_preference_to_match_checkbox (gpointer user_data);




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

/* Used for window position & size sanity-checking. The sizes are big enough to prevent
 * at least normal-sized gnome panels from obscuring the window at the screen edges. 
 */
#define MINIMUM_ON_SCREEN_WIDTH		100
#define MINIMUM_ON_SCREEN_HEIGHT	100


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

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	nautilus_undo_share_undo_manager (GTK_OBJECT (window), undo_manager_source);
	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
	gtk_window_set_title (GTK_WINDOW (window), _("Nautilus: Bookmarks"));
	gtk_widget_set_usize (window, 
			      BOOKMARKS_WINDOW_MIN_WIDTH, 
			      BOOKMARKS_WINDOW_MIN_HEIGHT);
	nautilus_bookmarks_window_restore_geometry (window);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	
	content_area = gtk_hbox_new (TRUE, GNOME_PAD);
	gtk_widget_show (content_area);
	gtk_container_add (GTK_CONTAINER (window), content_area);

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
	nautilus_gtk_button_set_padding (GTK_BUTTON (remove_button), GNOME_PAD_SMALL);
	gtk_widget_show (remove_button);
	gtk_box_pack_start (GTK_BOX (hbox2), remove_button, TRUE, FALSE, 0);

	show_static_checkbox = gtk_check_button_new_with_label (_("Include built-in bookmarks in menu"));
	gtk_widget_show (show_static_checkbox);
	gtk_box_pack_end (GTK_BOX (right_side), show_static_checkbox, FALSE, FALSE, 0);

	update_built_in_bookmarks_checkbox_to_match_preference (GTK_CHECK_BUTTON (show_static_checkbox));
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BUILT_IN_BOOKMARKS,
					   update_built_in_bookmarks_checkbox_to_match_preference,
					   show_static_checkbox);

	gtk_signal_connect (GTK_OBJECT (show_static_checkbox), "toggled",
			    update_built_in_bookmarks_preference_to_match_checkbox,
			    show_static_checkbox);

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
                    	    
	name_field_changed_signalID =
		gtk_signal_connect (GTK_OBJECT (name_field), "changed",
                	            GTK_SIGNAL_FUNC (on_name_field_changed),
                      		    NULL);
                      		    
	gtk_signal_connect (GTK_OBJECT (name_field), "focus_in_event",
      	              	    GTK_SIGNAL_FUNC (on_text_field_focus_in_event),
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
                      		    
	gtk_signal_connect (GTK_OBJECT (uri_field), "focus_in_event",
      	              	    GTK_SIGNAL_FUNC (on_text_field_focus_in_event),
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

	window_geometry = nautilus_bookmark_list_get_window_geometry(bookmarks);

	if (window_geometry != NULL) 
	{	
		int left, top, width, height;

		if (gnome_parse_geometry (window_geometry, &left, &top, &width, &height))
		{
			/* Adjust for sanity, in case screen size has changed or
			 * stored numbers are bogus. Make the entire window fit
			 * on the screen, so all controls can be reached. Also
			 * make sure the window isn't ridiculously small. Also
			 * make sure the top of the window is on screen, for
			 * draggability (perhaps not absolutely required, depending
			 * on window manager, but seems like a sensible rule anyway).
			 */
			width = CLAMP (width, BOOKMARKS_WINDOW_MIN_WIDTH, gdk_screen_width());
			height = CLAMP (height, BOOKMARKS_WINDOW_MIN_HEIGHT, gdk_screen_height());

			top = CLAMP (top, 0, gdk_screen_height() - MINIMUM_ON_SCREEN_HEIGHT);
			/* FIXME bugzilla.eazel.com 669: 
			 * If window has negative left coordinate, set_uposition sends it
			 * somewhere else entirely. Not sure what level contains this bug (XWindows?).
			 * Hacked around by pinning the left edge to zero.
			 */
			left = CLAMP (left, 0, gdk_screen_width() - MINIMUM_ON_SCREEN_WIDTH);
					    		
			gtk_widget_set_uposition (window, left, top);
			gtk_window_set_default_size (GTK_WINDOW (window), width, height);

			return;
		}
	}

	/* fall through to default if necessary */
	gtk_window_set_default_size (GTK_WINDOW (window), 
				     BOOKMARKS_WINDOW_INITIAL_WIDTH, 
				     BOOKMARKS_WINDOW_INITIAL_HEIGHT);

	/* Let window manager handle default position if no position stored */
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
on_text_field_focus_in_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
	g_assert (NAUTILUS_IS_ENTRY (widget));

	nautilus_entry_select_all (NAUTILUS_ENTRY (widget));
	return FALSE;
}

static gboolean
on_text_field_focus_out_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
	g_assert (NAUTILUS_IS_ENTRY (widget));

	update_bookmark_from_text ();
	gtk_editable_select_region (GTK_EDITABLE (widget), -1, -1);
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
	nautilus_bookmarks_window_save_geometry (GTK_WINDOW (widget));
	
	/* Hide but don't destroy */
	gtk_widget_hide (widget);

	/* Disable undo for entry widgets */
	nautilus_undo_unregister (GTK_OBJECT (name_field));
	nautilus_undo_unregister (GTK_OBJECT (uri_field));

	/* Seems odd to restore the geometry just after saving it,
	 * and when the window is hidden, but this insures that
	 * the next time the window is shown it will have the
	 * right hints in it to appear in the correct place.
	 */
	nautilus_bookmarks_window_restore_geometry (widget);

	return TRUE;
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

static void
synch_built_in_bookmarks_preference_and_checkbox (GtkCheckButton *checkbox, gboolean trust_checkbox)
{
	gboolean preference_setting, checkbox_setting;

	g_assert (GTK_IS_CHECK_BUTTON (checkbox));

	preference_setting = nautilus_preferences_get_boolean 
		(NAUTILUS_PREFERENCES_SHOW_BUILT_IN_BOOKMARKS, TRUE);

	checkbox_setting = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));

	if (preference_setting == checkbox_setting) {
		return;
	}

	if (trust_checkbox) {
		nautilus_preferences_set_boolean
			(NAUTILUS_PREFERENCES_SHOW_BUILT_IN_BOOKMARKS, checkbox_setting);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), preference_setting);
	}
}

static void
update_built_in_bookmarks_preference_to_match_checkbox (gpointer user_data)
{
	synch_built_in_bookmarks_preference_and_checkbox (user_data, TRUE);
}

static void
update_built_in_bookmarks_checkbox_to_match_preference (gpointer user_data)
{
	synch_built_in_bookmarks_preference_and_checkbox (user_data, FALSE);
}
