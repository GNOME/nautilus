/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 *         Michael Meeks <michael@nuclecu.unam.mx>
 *	   Andy Hertzfeld <andy@eazel.com>
 *
 */

/* nautilus-location-bar.c - Location bar for Nautilus
 */

#include <config.h>
#include "nautilus-location-entry.h"

#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-icon-dnd.h>
#include <libnautilus/nautilus-clipboard.h>
#include <stdio.h>
#include <string.h>

struct NautilusLocationEntryDetails {
	GtkLabel *label;
	
	char *current_directory;
	GList *file_info_list;
	
	guint idle_id;
};

static void  nautilus_location_entry_class_init       (NautilusLocationEntryClass *class);
static void  nautilus_location_entry_init             (NautilusLocationEntry      *entry);

EEL_CLASS_BOILERPLATE (NautilusLocationEntry,
		       nautilus_location_entry,
		       NAUTILUS_TYPE_ENTRY)

/* utility routine to determine the string to expand to.  If we don't have anything yet, accept
   the whole string, otherwise accept the largest part common to both */

static char *
accumulate_name_utf8 (char *full_name, char *candidate_name)
{
	char *result_name, *str1, *str2;

	if (!g_utf8_validate (candidate_name, -1, NULL)) {
		return full_name;
	}
	
	if (full_name == NULL) {
		result_name = g_strdup (candidate_name);
	} else {
		result_name = full_name;
		if (!eel_str_has_prefix (full_name, candidate_name)) {
			str1 = full_name;
			str2 = candidate_name;

			while ((g_utf8_get_char (str1) == g_utf8_get_char (str2))) {
				str1 = g_utf8_next_char (str1);
				str2 = g_utf8_next_char (str2);
			}
			*str1 = '\0';
		}
	}

	return result_name;
}

static char *
accumulate_name_locale (char *full_name, char *candidate_name)
{
	char *result_name, *str1, *str2;

	if (full_name == NULL)
		result_name = g_strdup (candidate_name);
	else {
		result_name = full_name;
		if (!eel_str_has_prefix (full_name, candidate_name)) {
			str1 = full_name;
			str2 = candidate_name;

			while (*str1 == *str2) {
				str1++;
				str2++;
			}
			*str1 = '\0';
		}
	}

	return result_name;
}

/* utility routine to load the file info list for the current directory, if necessary */
static void
get_file_info_list (NautilusLocationEntry *entry, const char* dir_name)
{
	GnomeVFSResult result;

	if (eel_strcmp (entry->details->current_directory, dir_name) != 0) {
		g_free (entry->details->current_directory);
		if (entry->details->file_info_list) {
			gnome_vfs_file_info_list_free (entry->details->file_info_list);	
			entry->details->file_info_list = NULL;		
		}

		entry->details->current_directory = g_strdup (dir_name);
		result = gnome_vfs_directory_list_load (&entry->details->file_info_list, dir_name,
							GNOME_VFS_FILE_INFO_DEFAULT);
		if (result != GNOME_VFS_OK) {
			if (entry->details->file_info_list) {
				gnome_vfs_file_info_list_free (entry->details->file_info_list);	
				entry->details->file_info_list = NULL;			
			}
		}
	}
}

/* routine that performs the tab expansion using gnome-vfs.  Extract the directory name and
   incomplete basename, then iterate through the directory trying to complete it.  If we
   find something, add it to the entry */
  
static gboolean
try_to_expand_path (gpointer callback_data)
{
	NautilusLocationEntry *entry;

	GnomeVFSFileInfo *current_file_info;
	GList *element;
	GnomeVFSURI *uri;
	GtkEditable *editable;

	char *base_name_uri_escaped;
	char *base_name;
	char *base_name_utf8;
	char *user_location;
	char *current_path;
	char *dir_name;
	char *expand_text;
	char *expand_text_utf8;
	char *expand_name;
	char *insert_text;

	int base_name_length;
	int user_location_length;
	int expand_text_length;
	int pos;

	entry = NAUTILUS_LOCATION_ENTRY (callback_data);
	editable = GTK_EDITABLE (entry);
	user_location = gtk_editable_get_chars (editable, 0, -1);
	entry->details->idle_id = 0;

	/* if it's just '~' don't expand because slash shouldn't be appended */
	if (eel_strcmp (user_location, "~") == 0) {
		g_free (user_location);
		return FALSE;
	}

	/* Trailing whitespace is OK here since the cursor is known to
	   be at the end of the text and therefor after the whitespace. */
	current_path = eel_make_uri_from_input_with_trailing_ws (user_location);
	if (!eel_istr_has_prefix (current_path, "file://")) {
		g_free (user_location);
		g_free (current_path);
		return FALSE;
	}

	/* We already completed if we have a trailing '/' */
	if (current_path[strlen (current_path) - 1] == GNOME_VFS_URI_PATH_CHR) {
		g_free (user_location);
		g_free (current_path);
		return FALSE;
	}

	user_location_length = g_utf8_strlen (user_location, -1);

	g_free (user_location);

	uri = gnome_vfs_uri_new (current_path);

	base_name_uri_escaped = gnome_vfs_uri_extract_short_name (uri);
	if (base_name_uri_escaped == NULL) {
		base_name = NULL;
	} else {
		base_name = gnome_vfs_unescape_string (base_name_uri_escaped, NULL);
	}
	g_free (base_name_uri_escaped);

	if (base_name == NULL) {
		gnome_vfs_uri_unref (uri);
		g_free (current_path);
		return FALSE;
	}

	dir_name = gnome_vfs_uri_extract_dirname (uri);

	gnome_vfs_uri_unref (uri);
	uri = NULL;

	/* get file info for the directory, if it hasn't changed since last time */
	get_file_info_list (entry, dir_name);
	if (entry->details->file_info_list == NULL) {
		g_free (dir_name);
		g_free (base_name);
		g_free (current_path);
		return FALSE;
	}

	/* iterate through the directory, keeping the intersection of all the names that
	   have the current basename as a prefix. */
	expand_text = NULL;
	for (element = entry->details->file_info_list; element != NULL; element = element->next) {
		current_file_info = element->data;
		if (eel_str_has_prefix (current_file_info->name, base_name)) {
			if (current_file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				expand_name = g_strconcat (current_file_info->name, "/", NULL);
			} else {
				expand_name = g_strdup (current_file_info->name);
			}
			if (nautilus_have_broken_filenames()) {
				expand_text = accumulate_name_locale (expand_text, expand_name);
			} else {
				expand_text = accumulate_name_utf8 (expand_text, expand_name);
			}
			g_free (expand_name);
		}
	}

	if (nautilus_have_broken_filenames ()) {
		if (expand_text) {
			expand_text_utf8 = g_locale_to_utf8 (expand_text, -1, NULL, NULL, NULL);
			g_free (expand_text);
			expand_text = expand_text_utf8;
		}
		
		base_name_utf8 = g_locale_to_utf8 (base_name, -1, NULL, NULL, NULL);
		g_free (base_name);
		base_name = base_name_utf8;
	} 

	/* if we've got something, add it to the entry */
	if (expand_text != NULL && base_name != NULL) {
		expand_text_length = g_utf8_strlen (expand_text, -1);
		base_name_length = g_utf8_strlen (base_name, -1);
		
		if (!eel_str_has_suffix (base_name, expand_text)
		    && base_name_length < expand_text_length) {
			insert_text = g_utf8_offset_to_pointer (expand_text, base_name_length);
			pos = user_location_length;
			gtk_editable_insert_text (editable,
						  insert_text,
						  g_utf8_strlen (insert_text, -1),
						  &pos);

			pos = user_location_length;
			gtk_editable_select_region (editable, pos, -1);
		}
	}
	g_free (expand_text);

	g_free (dir_name);
	g_free (base_name);
	g_free (current_path);

	return FALSE;
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
	case GDK_KP_Home:
	case GDK_KP_End:
	case GDK_Left:
	case GDK_Right:
	case GDK_KP_Left:
	case GDK_KP_Right:
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

static int
get_editable_number_of_chars (GtkEditable *editable)
{
	char *text;
	int length;

	text = gtk_editable_get_chars (editable, 0, -1);
	length = g_utf8_strlen (text, -1);
	g_free (text);
	return length;
}

static void
set_position_and_selection_to_end (GtkEditable *editable)
{
	int end;

	end = get_editable_number_of_chars (editable);
	gtk_editable_select_region (editable, end, end);
	gtk_editable_set_position (editable, end);
}

static gboolean
position_and_selection_are_at_end (GtkEditable *editable)
{
	int end;
	int start_sel, end_sel;
	
	end = get_editable_number_of_chars (editable);
	if (gtk_editable_get_selection_bounds (editable, &start_sel, &end_sel)) {
		if (start_sel != end || end_sel != end) {
			return FALSE;
		}
	}
	return gtk_editable_get_position (editable) == end;
}

static void
editable_event_after_callback (GtkEntry *entry,
			       GdkEvent *event,
			       gpointer user_data)
{
	GtkEditable *editable;
	GdkEventKey *keyevent;
	NautilusLocationEntry *location_entry;

	if (event->type != GDK_KEY_PRESS) {
		return;
	}

	editable = GTK_EDITABLE (entry);
	keyevent = (GdkEventKey *)event;
	location_entry = NAUTILUS_LOCATION_ENTRY (user_data);

	/* After typing the right arrow key we move the selection to
	 * the end, if we have a valid selection - since this is most
	 * likely an auto-completion. We ignore shift / control since
	 * they can validly be used to extend the selection.
	 */
	if ((keyevent->keyval == GDK_Right || keyevent->keyval == GDK_End) &&
	    !(keyevent->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) && 
	    gtk_editable_get_selection_bounds (editable, NULL, NULL)) {
		set_position_and_selection_to_end (editable);
	}

	/* Only do expanding when we are typing at the end of the
	 * text. Do the expand at idle time to avoid slowing down
	 * typing when the directory is large. Only trigger the expand
	 * when we type a key that would have inserted characters.
	 */
	if (position_and_selection_are_at_end (editable)) {
		if (entry_would_have_inserted_characters (keyevent)) {
			if (location_entry->details->idle_id == 0) {
				location_entry->details->idle_id = g_idle_add (try_to_expand_path, location_entry);
			}
		}
	} else {
		/* FIXME: Also might be good to do this when you click
		 * to change the position or selection.
		 */
		if (location_entry->details->idle_id != 0) {
			g_source_remove (location_entry->details->idle_id);
			location_entry->details->idle_id = 0;
		}
	}
}

static void
finalize (GObject *object)
{
	NautilusLocationEntry *entry;

	entry = NAUTILUS_LOCATION_ENTRY (object);

	g_free (entry->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
destroy (GtkObject *object)
{
	NautilusLocationEntry *entry;

	entry = NAUTILUS_LOCATION_ENTRY (object);
	
	/* cancel the pending idle call, if any */
	if (entry->details->idle_id != 0) {
		g_source_remove (entry->details->idle_id);
		entry->details->idle_id = 0;
	}
	
	if (entry->details->file_info_list) {
		gnome_vfs_file_info_list_free (entry->details->file_info_list);	
		entry->details->file_info_list = NULL;
	}
	
	g_free (entry->details->current_directory);
	entry->details->current_directory = NULL;
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_location_entry_class_init (NautilusLocationEntryClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;
}

static void
nautilus_location_entry_init (NautilusLocationEntry *entry)
{
	entry->details = g_new0 (NautilusLocationEntryDetails, 1);

	nautilus_entry_set_special_tab_handling (NAUTILUS_ENTRY (entry), TRUE);

	g_signal_connect_object (entry, "event_after",
				 G_CALLBACK (editable_event_after_callback), entry, 0);

}

GtkWidget *
nautilus_location_entry_new (void)
{
	GtkWidget *entry;

	entry = gtk_widget_new (NAUTILUS_TYPE_LOCATION_ENTRY, NULL);

	return entry;
}
