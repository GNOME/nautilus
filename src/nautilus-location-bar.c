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

#include <string.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkeventbox.h>
#include <libgnomevfs/gnome-vfs.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-mime.h>
#include <libgnome/gnome-i18n.h>

#include <libgnomeui/gnome-uidefs.h>

#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>

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

enum {
	LOCATION_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

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

static void nautilus_location_bar_initialize_class (NautilusLocationBarClass *class);
static void nautilus_location_bar_initialize       (NautilusLocationBar      *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusLocationBar, nautilus_location_bar, GTK_TYPE_HBOX)


static void
drag_data_received_callback (GtkWidget *widget,
		       	     GdkDragContext *context,
		       	     int x,
		       	     int y,
		       	     GtkSelectionData *data,
		             guint info,
		             guint32 time)
{
	GList *names;
	gchar *uri;

	g_assert (NAUTILUS_IS_LOCATION_BAR (widget));
	g_assert (data != NULL);

	names = gnome_uri_list_extract_uris (data->data);

	if (names == NULL) {
		g_warning ("No D&D URI's");
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	/* FIXME bugzilla.eazel.com 670: 
	 * When more than one URI is dragged here, should we make windows? 
	 */
	if (nautilus_g_list_more_than_one_item (names)) {
		g_warning ("Should we make more windows?");
	}

	uri = nautilus_make_uri_from_input (gtk_entry_get_text (GTK_ENTRY (NAUTILUS_LOCATION_BAR (widget)->entry)));

	nautilus_location_bar_set_location (NAUTILUS_LOCATION_BAR (widget),
					    names->data);
	gtk_signal_emit (GTK_OBJECT (widget),
			 signals[LOCATION_CHANGED],
			 uri);

	gnome_uri_list_free_strings (names);

	gtk_drag_finish (context, TRUE, FALSE, time);
	g_free (uri);
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

	entry_text = nautilus_make_uri_from_input (gtk_entry_get_text (NAUTILUS_LOCATION_BAR (widget->parent)->entry));
	
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

static void
editable_activated_callback (GtkEditable *editable,
		       	     NautilusLocationBar *bar)
{
	gchar *uri;

	g_assert (GTK_IS_EDITABLE (editable));
	g_assert (NAUTILUS_IS_LOCATION_BAR (bar));

	uri = nautilus_make_uri_from_input (gtk_entry_get_text (GTK_ENTRY (editable)));
	gtk_signal_emit (GTK_OBJECT (bar),
			 signals[LOCATION_CHANGED],
			 uri);
	g_free (uri);
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
	int base_length, current_path_length, offset;
	const char *base_name;
	char *user_location, *current_path, *dir_name, *expand_text;

	user_location = gtk_editable_get_chars (editable, 0, -1);
 	
	current_path = nautilus_make_uri_from_input (user_location);

	if (!nautilus_str_has_prefix(current_path, "file://")) {
		g_free(current_path);
		return;
	}

	current_path_length = strlen(current_path);	
	offset = current_path_length - strlen(user_location);

	uri = gnome_vfs_uri_new(current_path);
	
	base_name = gnome_vfs_uri_get_basename(uri);
	if (base_name)
		base_length = strlen(base_name);
	else {
		gnome_vfs_uri_unref(uri);
		g_free(current_path);
		return;	
	}
		
	dir_name = gnome_vfs_uri_extract_dirname(uri);

	/* get file info for the directory */

	result = gnome_vfs_directory_list_load (&list, dir_name,
					       GNOME_VFS_FILE_INFO_DEFAULT, NULL, NULL);
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
		if (nautilus_str_has_prefix(current_file_info->name, base_name)) {
			expand_text = accumulate_name(expand_text, current_file_info->name);
		}
		current_file_info = gnome_vfs_directory_list_next(list);
	}
	
	/* if we've got something, add it to the entry */	
	if (expand_text && !nautilus_str_has_suffix(current_path, expand_text)) {
		gtk_entry_append_text (GTK_ENTRY(editable), expand_text + base_length);
 		gtk_entry_select_region(GTK_ENTRY(editable), current_path_length - offset,
					current_path_length + strlen(expand_text) - base_length - offset);
		g_free (expand_text);
	}
	
	g_free(dir_name);
	g_free(current_path);
	g_free(user_location);
	gnome_vfs_directory_list_destroy(list);
	/* FIXME: for purposes of "doing it right" and our own edification we want to
	   be able to remove the following line...but this will work just fine for now.
	   The problem is that *some* mysterious piece of the above code seems to affecting
	   the focus */
	gtk_widget_grab_focus(GTK_WIDGET(editable));
}


/* handle changes in the location entry by checking for tabs */
static void
editable_key_press_callback (GtkEditable *editable,
		       	     	GdkEventKey *event)
{
	g_assert (GTK_IS_EDITABLE (editable));
	if (event->string[0] != 8)  {
		try_to_expand_path(editable);
	}
}	

static void
destroy (GtkObject *object)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (object);

	gtk_widget_destroy (GTK_WIDGET (bar->label));
	gtk_widget_destroy (GTK_WIDGET (bar->entry));

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_location_bar_initialize_class (NautilusLocationBarClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;
	
	signals[LOCATION_CHANGED]
		= gtk_signal_new ("location_changed",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusLocationBarClass,
						     location_changed),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_location_bar_initialize (NautilusLocationBar *bar)
{
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *event_box;
	
	event_box = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (event_box),
					GNOME_PAD_SMALL);
	label = gtk_label_new (_("Location:"));
	gtk_container_add   (GTK_CONTAINER (event_box), label);
	gtk_box_pack_start  (GTK_BOX (bar), event_box, FALSE, TRUE,
			     GNOME_PAD_SMALL);

	entry = nautilus_entry_new ();
	gtk_signal_connect (GTK_OBJECT (entry), "activate",
			    editable_activated_callback, bar);

	gtk_signal_connect_after (GTK_OBJECT (entry), "key_press_event",
			    editable_key_press_callback, NULL);
	
	gtk_box_pack_start (GTK_BOX (bar), entry, TRUE, TRUE, 0);

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

	gtk_widget_show (entry);
	gtk_widget_show_all (event_box);

	bar->label = GTK_LABEL (label);
	bar->entry = GTK_ENTRY (entry);	
}


GtkWidget *
nautilus_location_bar_new (void)
{
	return gtk_widget_new (nautilus_location_bar_get_type (), NULL);
}


/**
 * nautilus_location_bar_set_location
 * 
 * Change the text displayed in the location bar.
 * 
 * @bar: A NautilusLocationBar.
 * @location: The uri that should be displayed.
 */
void
nautilus_location_bar_set_location (NautilusLocationBar *bar,
				    const char *location)
{
	gchar *formatted_location;
	g_assert (location != NULL);
	g_return_if_fail (NAUTILUS_IS_LOCATION_BAR (bar));
	
	/* Note: This is called in reaction to external changes, and 
	 * thus should not emit the LOCATION_CHANGED signal.*/
	
	formatted_location = nautilus_format_uri_for_display (location);
	nautilus_entry_set_text (NAUTILUS_ENTRY (bar->entry),
			    formatted_location);
	g_free (formatted_location);
}

