/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-location-bar.c - Location bar for Nautilus

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ettore Perazzoli <ettore@gnu.org>
   DnD code added by Michael Meeks <michael@nuclecu.unam.mx>
*/

#include <config.h>
#include "nautilus-location-bar.h"

#include "nautilus-window.h"

#include <string.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkeventbox.h>
#include <libgnomevfs/gnome-vfs.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-mime.h>
#include <libgnome/gnome-i18n.h>

#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-uidefs.h>

#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>

#include <stdio.h>

#define NAUTILUS_DND_URI_LIST_TYPE 	  "text/uri-list"
#define NAUTILUS_DND_TEXT_PLAIN_TYPE 	  "text/plain"
#define NAUTILUS_DND_URL_TYPE		  "_NETSCAPE_URL"

enum {
	NAUTILUS_DND_MC_DESKTOP_ICON,
	NAUTILUS_DND_URI_LIST,
	NAUTILUS_DND_TEXT_PLAIN,
	NAUTILUS_DND_URL,
	NAUTILUS_DND_NTARGETS
};

static GtkTargetEntry drag_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
	{ NAUTILUS_DND_URL_TYPE,        0, NAUTILUS_DND_URL }
};

static GtkTargetEntry drop_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
	{ NAUTILUS_DND_URL_TYPE,        0, NAUTILUS_DND_URL }
};

static char *nautilus_location_bar_get_location     (NautilusNavigationBar    *navigation_bar); 
static void  nautilus_location_bar_set_location     (NautilusNavigationBar    *navigation_bar,
						     const char               *location);
static void  nautilus_location_bar_initialize_class (NautilusLocationBarClass *class);
static void  nautilus_location_bar_initialize       (NautilusLocationBar      *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusLocationBar, nautilus_location_bar, NAUTILUS_TYPE_NAVIGATION_BAR)

static NautilusWindow *
nautilus_location_bar_get_window (GtkWidget *widget)
{
	return NAUTILUS_WINDOW (gtk_widget_get_toplevel (widget));
}

static void
drag_data_received_callback (GtkWidget *widget,
		       	     GdkDragContext *context,
		       	     int x,
		       	     int y,
		       	     GtkSelectionData *data,
		             guint info,
		             guint32 time)
{
	GList *names, *node;
	NautilusApplication *application;
	int name_count;
	NautilusWindow *new_window;
	gboolean new_windows_for_extras;
	char *prompt;

	g_assert (NAUTILUS_IS_LOCATION_BAR (widget));
	g_assert (data != NULL);

	names = gnome_uri_list_extract_uris (data->data);

	if (names == NULL) {
		g_warning ("No D&D URI's");
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	new_windows_for_extras = FALSE;
	/* Ask user if they really want to open multiple windows
	 * for multiple dropped URIs. This is likely to have been
	 * a mistake.
	 */
	name_count = g_list_length (names);
	if (name_count > 1) {
		prompt = g_strdup_printf (_("Do you want to view these %d locations "
					  "in separate windows?"), 
					  name_count);
		new_windows_for_extras = nautilus_simple_dialog 
			(GTK_WIDGET (nautilus_location_bar_get_window (widget)),
			 prompt,
			 _("View in Multiple Windows?"),
			 GNOME_STOCK_BUTTON_OK,
			 GNOME_STOCK_BUTTON_CANCEL,
			 NULL) == GNOME_OK;

		g_free (prompt);
		
		if (!new_windows_for_extras) {
			gtk_drag_finish (context, FALSE, FALSE, time);
			return;
		}
	}

	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (widget),
					      names->data);	
	nautilus_navigation_bar_location_changed (NAUTILUS_NAVIGATION_BAR (widget));

	if (new_windows_for_extras) {
		application = nautilus_location_bar_get_window (widget)->application;
		for (node = names->next; node != NULL; node = node->next) {
			new_window = nautilus_application_create_window (application);
			nautilus_window_goto_uri (new_window, node->data);
		}
	}
						  
	gnome_uri_list_free_strings (names);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
drag_data_get_callback (GtkWidget *widget,
		  	GdkDragContext *context,
		  	GtkSelectionData *selection_data,
		  	guint info,
		 	guint32 time)
{
	char *entry_text;

	g_assert (selection_data != NULL);

	entry_text = nautilus_navigation_bar_get_location (NAUTILUS_NAVIGATION_BAR (widget->parent));
	
	switch (info) {
	case NAUTILUS_DND_URI_LIST:
	case NAUTILUS_DND_TEXT_PLAIN:
	case NAUTILUS_DND_URL:
		gtk_selection_data_set (selection_data,
					selection_data->target,
					8, (guchar *) entry_text,
					nautilus_strlen (entry_text));
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (entry_text);
}

/* utility routine to determine the string to expand to.  If we don't have anything yet, accept
   the whole string, otherwise accept the largest part common to both */
   
static char *
accumulate_name(char *full_name, char *candidate_name)
{
	char *result_name, *str1, *str2;
	if (full_name == NULL)
		result_name = g_strdup(candidate_name);
	else {
		result_name = full_name;
		if (!nautilus_str_has_prefix(full_name, candidate_name)) {
			str1 = full_name;
			str2 = candidate_name;
			while ((*str1++ == *str2++)) {};	
			*--str1 = '\0';
		}
	}
	return result_name;
}

/* routine that performs the tab expansion using gnome-vfs.  Extract the directory name and
  incomplete basename, then iterate through the directory trying to complete it.  If we
  find something, add it to the entry */
  
static void
try_to_expand_path(GtkEditable *editable)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;
	GnomeVFSURI *uri;
	int base_length;
	int current_path_length;
	int offset;
	const char *base_name;
	char *user_location;
	char *current_path;
	char *dir_name;
	char *expand_text;

	user_location = gtk_editable_get_chars (editable, 0, -1);
 	
	current_path = nautilus_make_uri_from_input (user_location);

	if (!nautilus_istr_has_prefix (current_path, "file://")) {
		g_free (current_path);
		return;
	}

	current_path_length = strlen(current_path);	
	offset = current_path_length - strlen(user_location);

	uri = gnome_vfs_uri_new (current_path);
	
	base_name = gnome_vfs_uri_get_basename (uri);
	if (base_name)
		base_length = strlen (base_name);
	else {
		gnome_vfs_uri_unref (uri);
		g_free (current_path);
		return;	
	}
		
	dir_name = gnome_vfs_uri_extract_dirname(uri);

	/* get file info for the directory */

	result = gnome_vfs_directory_list_load (&list, dir_name,
					       GNOME_VFS_FILE_INFO_DEFAULT, NULL);
	if (result != GNOME_VFS_OK) {
		g_free(dir_name);
		gnome_vfs_uri_unref(uri);
		g_free(current_path);
		return;
	}

	/* iterate through the directory, keeping the intersection of all the names that
	   have the current basename as a prefix. */

	current_file_info = gnome_vfs_directory_list_first(list);
	expand_text = NULL;
	while (current_file_info != NULL) {
		if (nautilus_str_has_prefix (current_file_info->name, base_name)) {
			expand_text = accumulate_name (expand_text, current_file_info->name);
		}
		current_file_info = gnome_vfs_directory_list_next (list);
	}
	
	/* if we've got something, add it to the entry */	
	if (expand_text && !nautilus_str_has_suffix (current_path, expand_text)) {
		gtk_entry_append_text (GTK_ENTRY (editable), expand_text + base_length);
 		gtk_entry_select_region (GTK_ENTRY (editable), current_path_length - offset,
					 current_path_length + strlen (expand_text) - base_length - offset);
		g_free (expand_text);
	}
	
	g_free(dir_name);
	g_free(current_path);
	g_free(user_location);
	gnome_vfs_directory_list_destroy(list);
}

/* Until we have a more elegant solution, this is how we figure out if
 * the GtkEntry inserted characters, assuming that the return value is
 * TRUE indicating that the GtkEntry consumed the key event for some
 * reason. This is a clone of code from GtkEntry.
 */
static gboolean
entry_would_have_inserted_characters (const GdkEventKey *event)
{
	switch (event->keyval) {
	case GDK_BackSpace:
	case GDK_Clear:
	case GDK_Insert:
	case GDK_Delete:
	case GDK_Home:
	case GDK_End:
	case GDK_Left:
	case GDK_Right:
	case GDK_Return:
		return FALSE;
	default:
		if (event->keyval >= 0x20 && event->keyval <= 0xFF) {
			if ((event->state & GDK_CONTROL_MASK) != 0) {
				return FALSE;
			}
			if ((event->state & GDK_MOD1_MASK) != 0) {
				return FALSE;
			}
		}
		return event->length > 0;
	}
}

/* Handle changes in the location entry. This is a marshal-style
 * callback so that we don't mess up the return value.
 */
static void
editable_key_press_callback (GtkObject *object,
			     gpointer data,
			     guint n_args,
			     GtkArg *args)
{
	GtkEditable *editable;
	GdkEventKey *event;
	int position;
	gboolean *return_value_location;

	g_assert (data == NULL);
	g_assert (n_args == 1);
	g_assert (args != NULL);

	editable = GTK_EDITABLE (object);
	event = GTK_VALUE_POINTER (args[0]);
	return_value_location = GTK_RETLOC_BOOL (args[1]);

	if (event->keyval == GDK_Right && editable->has_selection) {
		position = strlen (gtk_entry_get_text (GTK_ENTRY (editable)));
		gtk_entry_select_region (GTK_ENTRY (editable), position, position);
		return;
	}
	
	/* Only do an expand if we just handled a key that inserted
	 * characters.
	 */
	if (*return_value_location
	    && entry_would_have_inserted_characters (event))  {
		try_to_expand_path (editable);
	}
}

static void
destroy (GtkObject *object)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (object);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_location_bar_initialize_class (NautilusLocationBarClass *class)
{
	GtkObjectClass *object_class;
	NautilusNavigationBarClass *navigation_bar_class;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;
	
	navigation_bar_class = NAUTILUS_NAVIGATION_BAR_CLASS (class);

	navigation_bar_class->get_location = nautilus_location_bar_get_location;
	navigation_bar_class->set_location = nautilus_location_bar_set_location;
}

static void
nautilus_location_bar_initialize (NautilusLocationBar *bar)
{
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *event_box;
	GtkWidget *hbox;

	hbox = gtk_hbox_new (0, FALSE);

	event_box = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (event_box),
					GNOME_PAD_SMALL);
	label = gtk_label_new (_("Location:"));
	gtk_container_add   (GTK_CONTAINER (event_box), label);


	gtk_box_pack_start  (GTK_BOX (hbox), event_box, FALSE, TRUE,
			     GNOME_PAD_SMALL);

	entry = nautilus_entry_new ();
	gtk_signal_connect_object (GTK_OBJECT (entry), "activate",
				   nautilus_navigation_bar_location_changed, GTK_OBJECT (bar));

	/* The callback uses the marshal interface directly
	 * so it can both read and write the return value.
	 */
	gtk_signal_connect_full (GTK_OBJECT (entry), "key_press_event",
				 NULL, editable_key_press_callback,
				 NULL, NULL,
				 FALSE, TRUE);
	
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (bar), hbox);

	/* Drag source */
	gtk_drag_source_set (GTK_WIDGET (event_box), 
			     GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			     drag_types, NAUTILUS_N_ELEMENTS (drag_types),
			     GDK_ACTION_LINK);
	gtk_signal_connect  (GTK_OBJECT (event_box), "drag_data_get",
			     GTK_SIGNAL_FUNC (drag_data_get_callback),
			     bar);

	/* Drag dest. */
	gtk_drag_dest_set  (GTK_WIDGET (bar),
			    GTK_DEST_DEFAULT_ALL,
			    drop_types, NAUTILUS_N_ELEMENTS (drop_types),
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	gtk_signal_connect (GTK_OBJECT (bar), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received_callback),
			    NULL);

	gtk_widget_show_all (hbox);

	bar->label = GTK_LABEL (label);
	bar->entry = GTK_ENTRY (entry);	
}


GtkWidget *
nautilus_location_bar_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_LOCATION_BAR, NULL);
}

static void
nautilus_location_bar_set_location (NautilusNavigationBar *navigation_bar,
				    const char *location)
{
	NautilusLocationBar *bar;
	char *formatted_location;

	g_assert (location != NULL);
	
	bar = NAUTILUS_LOCATION_BAR (navigation_bar);

	/* Note: This is called in reaction to external changes, and 
	 * thus should not emit the LOCATION_CHANGED signal.*/
	
	formatted_location = nautilus_format_uri_for_display (location);
	nautilus_entry_set_text (NAUTILUS_ENTRY (bar->entry),
				 formatted_location);
	g_free (formatted_location);
}

/**
 * nautilus_location_bar_get_location
 *
 * Get the "URI" represented by the text in the location bar.
 *
 * @bar: A NautilusLocationBar.
 *
 * returns a newly allocated "string" containing the mangled
 * (by nautilus_make_uri_from_input) text that the user typed in...maybe a URI 
 * but not garunteed.
 *
 **/

static char *
nautilus_location_bar_get_location (NautilusNavigationBar *navigation_bar) 
{
	NautilusLocationBar *bar;
	char *user_location, *best_uri;

	bar = NAUTILUS_LOCATION_BAR (navigation_bar);
	
	user_location = gtk_editable_get_chars (GTK_EDITABLE (bar->entry), 0, -1);
	best_uri = nautilus_make_uri_from_input (user_location);
	g_free (user_location);
	return best_uri;
}
	       
