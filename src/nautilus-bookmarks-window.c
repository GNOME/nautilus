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

#include "nautilus-bookmarks-window.h"


/* Static variables to keep track of window state. If there were
 * more than one bookmark-editing window, these would be struct or
 * class fields. 
 */
static gint		     bookmarklist_changed_signalID;
static NautilusBookmarklist *bookmarks = NULL;
static GtkWidget	    *bookmark_list_widget = NULL; /* awkward name to distinguish from NautilusBookmarklist */
static GtkWidget	    *name_field = NULL;
static gint		     name_field_changed_signalID;
static GtkWidget	    *remove_button = NULL;
static gboolean		     text_changed = FALSE;
static GtkWidget	    *uri_field = NULL;
static gint		     uri_field_changed_signalID;


/* forward declarations */
static const NautilusBookmark *get_selected_bookmark (void);
static guint	get_selected_row	      (void);
static gboolean get_selection_exists 	      (void);
static void	on_bookmarklist_changed       (NautilusBookmarklist *, 
					       gpointer user_data);
static void	on_name_field_changed 	      (GtkEditable *, gpointer user_data);
static void	on_remove_button_clicked      (GtkButton *, gpointer user_data);
static void	on_row_move 		      (GtkCList *,
	     				       gint old_row,
	     				       gint new_row,
	     				       gpointer user_data);
static void	on_select_row 		      (GtkCList	*,
	       				       gint row,
	       				       gint column,
	       				       GdkEventButton *,
	       				       gpointer user_data);
static gboolean	on_text_field_focus_out_event (GtkWidget *, 
					       GdkEventFocus *, 
					       gpointer user_data);
static void	on_uri_field_changed 	      (GtkEditable *, gpointer user_data);
static gboolean on_window_delete_event 	      (GtkWidget *, 
					       GdkEvent *, 
					       gpointer user_data);
static void	repopulate		      (void);



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
 * @list: The NautilusBookmarklist that this window will edit.
 *
 * Return value: A pointer to the new window.
 **/
GtkWidget *
create_bookmarks_window(NautilusBookmarklist *list)
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
	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
	gtk_window_set_title (GTK_WINDOW (window), _("nautilus: Bookmarks"));
	gtk_window_set_default_size (GTK_WINDOW (window), 
				     BOOKMARKS_WINDOW_INITIAL_WIDTH, 
				     BOOKMARKS_WINDOW_INITIAL_HEIGHT);
	gtk_widget_set_usize (window, 
			      BOOKMARKS_WINDOW_MIN_WIDTH, 
			      BOOKMARKS_WINDOW_MIN_HEIGHT);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	content_area = gtk_hbox_new (TRUE, GNOME_PAD);
	gtk_widget_ref (content_area);
	gtk_widget_show (content_area);
	gtk_container_add (GTK_CONTAINER (window), content_area);

	list_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_ref (list_scroller);
	gtk_widget_show (list_scroller);
	gtk_box_pack_start (GTK_BOX (content_area), list_scroller, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	bookmark_list_widget = gtk_clist_new (1);
	gtk_widget_ref (bookmark_list_widget);
	gtk_widget_show (bookmark_list_widget);
	gtk_container_add (GTK_CONTAINER (list_scroller), bookmark_list_widget);
	gtk_clist_column_titles_hide (GTK_CLIST (bookmark_list_widget));
	gtk_clist_set_reorderable(GTK_CLIST (bookmark_list_widget), TRUE);
	gtk_clist_set_use_drag_icons(GTK_CLIST (bookmark_list_widget), FALSE);

	right_side = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_widget_ref (right_side);
	gtk_widget_show (right_side);
	gtk_box_pack_start (GTK_BOX (content_area), right_side, TRUE, TRUE, 0);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_ref (vbox3);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (right_side), vbox3, FALSE, FALSE, 0);

	name_label = gtk_label_new (_("Name"));
	gtk_widget_ref (name_label);
	gtk_widget_show (name_label);
	gtk_box_pack_start (GTK_BOX (vbox3), name_label, FALSE, FALSE, 0);

	name_field = gtk_entry_new ();
	gtk_widget_ref (name_field);
	gtk_widget_show (name_field);
	gtk_box_pack_start (GTK_BOX (vbox3), name_field, FALSE, FALSE, 0);

	vbox4 = gtk_vbox_new (FALSE, 0);
	gtk_widget_ref (vbox4);
	gtk_widget_show (vbox4);
	gtk_box_pack_start (GTK_BOX (right_side), vbox4, FALSE, FALSE, 0);

	url_label = gtk_label_new (_("Location"));
	gtk_widget_ref (url_label);
	gtk_widget_show (url_label);
	gtk_box_pack_start (GTK_BOX (vbox4), url_label, FALSE, FALSE, 0);

	uri_field = gtk_entry_new ();
	gtk_widget_ref (uri_field);
	gtk_widget_show (uri_field);
	gtk_box_pack_start (GTK_BOX (vbox4), uri_field, FALSE, FALSE, 0);

	hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_ref (hbox2);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (right_side), hbox2, FALSE, FALSE, 0);

	remove_button = gtk_button_new_with_label (_("Remove"));
	gtk_misc_set_padding (GTK_MISC (GTK_BIN(remove_button)->child), 
			      GNOME_PAD_SMALL, 
			      GNOME_PAD_SMALL);
	gtk_widget_ref (remove_button);
	gtk_widget_show (remove_button);
	gtk_box_pack_start (GTK_BOX (hbox2), remove_button, TRUE, FALSE, 0);

	/* Wire up all the signals. */

	bookmarklist_changed_signalID =
		gtk_signal_connect (GTK_OBJECT(bookmarks), "contents_changed",
				    GTK_SIGNAL_FUNC(on_bookmarklist_changed),
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
                      		    
	gtk_signal_connect (GTK_OBJECT (name_field), "focus_out_event",
      	              	    GTK_SIGNAL_FUNC (on_text_field_focus_out_event),
                            NULL);
                            
	uri_field_changed_signalID = 
		gtk_signal_connect (GTK_OBJECT (uri_field), "changed",
                	    	    GTK_SIGNAL_FUNC (on_uri_field_changed),
                      		    NULL);
                      		    
	gtk_signal_connect (GTK_OBJECT (uri_field), "focus_out_event",
        	            GTK_SIGNAL_FUNC (on_text_field_focus_out_event),
              	    	    NULL);
              	    	    
	gtk_signal_connect (GTK_OBJECT (remove_button), "clicked",
        	            GTK_SIGNAL_FUNC (on_remove_button_clicked),
                      	    NULL);

	/* Set selection mode after connecting signal to notice initial selected row. */
	gtk_clist_set_selection_mode(GTK_CLIST (bookmark_list_widget), 
				     GTK_SELECTION_BROWSE);

	/* Fill in list widget with bookmarks, must be after signals are wired up. */
	repopulate();

	return window;
}

static const NautilusBookmark *
get_selected_bookmark ()
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARKLIST(bookmarks), NULL);

	return nautilus_bookmarklist_item_at(bookmarks, get_selected_row());
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
on_bookmarklist_changed(NautilusBookmarklist *bookmarks, gpointer data)
{
	g_return_if_fail(NAUTILUS_IS_BOOKMARKLIST(bookmarks));

	/* maybe add logic here or in repopulate to save/restore selection */
	repopulate();
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
			   get_selected_row(), 0, 
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
				 bookmarklist_changed_signalID);
	nautilus_bookmarklist_delete_item_at(bookmarks, get_selected_row());
	gtk_signal_handler_unblock(GTK_OBJECT(bookmarks), 
				   bookmarklist_changed_signalID);

	gtk_clist_remove(GTK_CLIST(bookmark_list_widget), get_selected_row());
}


static void
on_row_move (GtkCList *clist,
	     gint      old_row,
	     gint      new_row,
	     gpointer  user_data)
{
	NautilusBookmark *bookmark;

	bookmark = nautilus_bookmark_copy(
		nautilus_bookmarklist_item_at(bookmarks, old_row));

	/* turn off list updating 'cuz otherwise the list-reordering code runs
	 * after repopulate(), thus reordering the correctly-ordered list.
	 */
	gtk_signal_handler_block(GTK_OBJECT(bookmarks), 
				 bookmarklist_changed_signalID);
	nautilus_bookmarklist_delete_item_at(bookmarks, old_row);
	nautilus_bookmarklist_insert_item(bookmarks, bookmark, new_row);
	gtk_signal_handler_unblock(GTK_OBJECT(bookmarks), 
				   bookmarklist_changed_signalID);

	gtk_object_destroy(GTK_OBJECT(bookmark));
}

static void
on_select_row (GtkCList	       *clist,
	       gint		row,
	       gint	 	column,
	       GdkEventButton  *event,
	       gpointer		user_data)
{
	const NautilusBookmark *selected;

	g_assert(GTK_IS_ENTRY(name_field));
	g_assert(GTK_IS_ENTRY(uri_field));

	selected = get_selected_bookmark();
	gtk_entry_set_text(GTK_ENTRY(name_field), 
			   nautilus_bookmark_get_name(selected));
	gtk_entry_set_text(GTK_ENTRY(uri_field), 
			   nautilus_bookmark_get_uri(selected));
}


static gboolean
on_text_field_focus_out_event (GtkWidget     *widget,
			      GdkEventFocus *event,
			      gpointer       user_data)
{
	if (text_changed)
	{
		NautilusBookmark *bookmark;
		guint		  selected_row;

		g_assert(GTK_IS_ENTRY(name_field));
		g_assert(GTK_IS_ENTRY(uri_field));

		bookmark = nautilus_bookmark_new(
			gtk_entry_get_text(GTK_ENTRY(name_field)),
			gtk_entry_get_text(GTK_ENTRY(uri_field)));
		selected_row = get_selected_row();

		/* turn off list updating 'cuz otherwise the list-reordering code runs
		 * after repopulate(), thus reordering the correctly-ordered list.
		 */
		gtk_signal_handler_block(GTK_OBJECT(bookmarks), 
					 bookmarklist_changed_signalID);
		nautilus_bookmarklist_delete_item_at(bookmarks, selected_row);
		nautilus_bookmarklist_insert_item(bookmarks, bookmark, selected_row);
		gtk_signal_handler_unblock(GTK_OBJECT(bookmarks), 
					   bookmarklist_changed_signalID);

		gtk_object_destroy(GTK_OBJECT(bookmark));
	}
	
	return FALSE;
}


static void
on_uri_field_changed (GtkEditable *editable,
		      gpointer     user_data)
{
	/* Remember that user has changed text so we 
	 * update real bookmark later. 
	 */
	text_changed = TRUE;
}


static gboolean
on_window_delete_event (GtkWidget *widget,
			GdkEvent  *event,
			gpointer   user_data)
{
	g_return_val_if_fail(GTK_IS_WINDOW(widget), FALSE);
	gtk_widget_hide(widget);

	return TRUE;
}

static void
repopulate ()
{
	GtkCList *clist;
	guint	  index;
	gboolean  selection_exists;

	g_assert(GTK_IS_CLIST(bookmark_list_widget));
	g_assert(NAUTILUS_IS_BOOKMARKLIST(bookmarks));
	
	clist = GTK_CLIST(bookmark_list_widget);

	/* Freeze while mucking with content so it's not flashy */
	gtk_clist_freeze(clist);
	    
	/* Empty the list. */
	gtk_clist_clear(clist);
	   
	/* Fill the list in with the bookmark names. */
	for (index = 0; index < nautilus_bookmarklist_length(bookmarks); ++index)
	{
		gchar *text[1];

		text[0] = (gchar *)nautilus_bookmark_get_name(
			nautilus_bookmarklist_item_at(bookmarks, index));
		gtk_clist_append(clist, text);
	}
	    
	/* Set the sensitivity of widgets that require a selection */
	selection_exists = get_selection_exists();
	gtk_widget_set_sensitive(remove_button, selection_exists);
	gtk_widget_set_sensitive(name_field, selection_exists);
	gtk_widget_set_sensitive(uri_field, selection_exists);
	    
	if (!selection_exists)
	{
		/* Block signals to avoid modifying non-existent selected item. */
		gtk_signal_handler_block(GTK_OBJECT(name_field), 
					 name_field_changed_signalID);
		gtk_entry_set_text(GTK_ENTRY(name_field), "");
		gtk_signal_handler_unblock(GTK_OBJECT(name_field), 
					   name_field_changed_signalID);

		gtk_signal_handler_block(GTK_OBJECT(uri_field), 
					 uri_field_changed_signalID);
		gtk_entry_set_text(GTK_ENTRY(uri_field), "");
		gtk_signal_handler_unblock(GTK_OBJECT(uri_field), 
					   uri_field_changed_signalID);
	}
	  
	gtk_clist_thaw(GTK_CLIST(bookmark_list_widget));
}

