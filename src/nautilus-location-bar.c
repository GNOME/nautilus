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
#include "nautilus-location-bar.h"

#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <eel/eel-accessibility.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-icon-dnd.h>
#include <libnautilus-private/nautilus-multihead-hacks.h>
#include <libnautilus/nautilus-clipboard.h>
#include <stdio.h>
#include <string.h>

#define NAUTILUS_DND_URI_LIST_TYPE 	  "text/uri-list"
#define NAUTILUS_DND_TEXT_PLAIN_TYPE 	  "text/plain"
#define NAUTILUS_DND_URL_TYPE		  "_NETSCAPE_URL"

static const char untranslated_location_label[] = N_("Location:");
static const char untranslated_go_to_label[] = N_("Go To:");
#define LOCATION_LABEL _(untranslated_location_label)
#define GO_TO_LABEL _(untranslated_go_to_label)

struct NautilusLocationBarDetails {
	GtkLabel *label;
	NautilusEntry *entry;
	
	char *last_location;
	
	char *current_directory;
	GList *file_info_list;
	
	guint idle_id;
};

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
static void  nautilus_location_bar_class_init       (NautilusLocationBarClass *class);
static void  nautilus_location_bar_init             (NautilusLocationBar      *bar);
static void  nautilus_location_bar_update_label     (NautilusLocationBar      *bar);

EEL_CLASS_BOILERPLATE (NautilusLocationBar,
		       nautilus_location_bar,
		       NAUTILUS_TYPE_NAVIGATION_BAR)

static NautilusWindow *
nautilus_location_bar_get_window (GtkWidget *bar)
{
	return NAUTILUS_WINDOW (gtk_widget_get_ancestor (bar, NAUTILUS_TYPE_WINDOW));
}

static void
drag_data_received_callback (GtkWidget *widget,
		       	     GdkDragContext *context,
		       	     int x,
		       	     int y,
		       	     GtkSelectionData *data,
		             guint info,
		             guint32 time,
			     gpointer callback_data)
{
	GList *names, *node;
	NautilusApplication *application;
	int name_count;
	NautilusWindow *new_window;
	NautilusWindow *window;
	GdkScreen      *screen;
	gboolean new_windows_for_extras;
	char *prompt;

	g_assert (NAUTILUS_IS_LOCATION_BAR (widget));
	g_assert (data != NULL);
	g_assert (callback_data == NULL);

	names = nautilus_icon_dnd_uri_list_extract_uris (data->data);

	if (names == NULL) {
		g_warning ("No D&D URI's");
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	window = nautilus_location_bar_get_window (widget);
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
		/* eel_run_simple_dialog should really take in pairs
		 * like gtk_dialog_new_with_buttons() does. */
		new_windows_for_extras = eel_run_simple_dialog 
			(GTK_WIDGET (window),
			 TRUE,
			 prompt,
			 _("View in Multiple Windows?"),
			 GTK_STOCK_OK, GTK_STOCK_CANCEL,
			 NULL) == 0 /* GNOME_OK */;

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
		application = window->application;
		screen = gtk_window_get_screen (GTK_WINDOW (window));

		for (node = names->next; node != NULL; node = node->next) {
			new_window = nautilus_application_create_window (application, screen);
			nautilus_window_go_to (new_window, node->data);
		}
	}

	nautilus_icon_dnd_uri_list_free_strings (names);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
drag_data_get_callback (GtkWidget *widget,
		  	GdkDragContext *context,
		  	GtkSelectionData *selection_data,
		  	guint info,
		 	guint32 time,
			gpointer callback_data)
{
	NautilusNavigationBar *bar;
	char *entry_text;

	g_assert (selection_data != NULL);
	bar = NAUTILUS_NAVIGATION_BAR (callback_data);

	entry_text = nautilus_navigation_bar_get_location (bar);
	
	switch (info) {
	case NAUTILUS_DND_URI_LIST:
	case NAUTILUS_DND_TEXT_PLAIN:
	case NAUTILUS_DND_URL:
		gtk_selection_data_set (selection_data,
					selection_data->target,
					8, (guchar *) entry_text,
					eel_strlen (entry_text));
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (entry_text);
}

/* routine that determines the usize for the label widget as larger
   then the size of the largest string and then sets it to that so
   that we don't have localization problems. see
   gtk_label_finalize_lines in gtklabel.c (line 618) for the code that
   we are imitating here. */

static void
style_set_handler (GtkWidget *widget, GtkStyle *previous_style)
{
	PangoLayout *layout;
	int width, width2;

	layout = gtk_label_get_layout (GTK_LABEL(widget));

	layout = pango_layout_copy (layout);

	pango_layout_set_text (layout, LOCATION_LABEL, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	
	pango_layout_set_text (layout, GO_TO_LABEL, -1);
	pango_layout_get_pixel_size (layout, &width2, NULL);
	width = MAX (width, width2);

	width += 2 * GTK_MISC (widget)->xpad;

	gtk_widget_set_size_request (widget, width, -1);
}

static gboolean
have_broken_filenames (void)
{
	static gboolean initialized = FALSE;
	static gboolean broken;
  
	if (initialized) {
		return broken;
	}

	broken = g_getenv ("G_BROKEN_FILENAMES") != NULL;
  
	initialized = TRUE;
  
	return broken;
}


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
get_file_info_list (NautilusLocationBar *bar, const char* dir_name)
{
	GnomeVFSResult result;

	if (eel_strcmp (bar->details->current_directory, dir_name) != 0) {
		g_free (bar->details->current_directory);
		if (bar->details->file_info_list) {
			gnome_vfs_file_info_list_free (bar->details->file_info_list);	
			bar->details->file_info_list = NULL;		
		}

		bar->details->current_directory = g_strdup (dir_name);
		result = gnome_vfs_directory_list_load (&bar->details->file_info_list, dir_name,
							GNOME_VFS_FILE_INFO_DEFAULT);
		if (result != GNOME_VFS_OK) {
			if (bar->details->file_info_list) {
				gnome_vfs_file_info_list_free (bar->details->file_info_list);	
				bar->details->file_info_list = NULL;			
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
	NautilusLocationBar *bar;

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

	bar = NAUTILUS_LOCATION_BAR (callback_data);
	editable = GTK_EDITABLE (bar->details->entry);
	user_location = gtk_editable_get_chars (editable, 0, -1);
	bar->details->idle_id = 0;

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
	get_file_info_list (bar, dir_name);
	if (bar->details->file_info_list == NULL) {
		g_free (dir_name);
		g_free (base_name);
		g_free (current_path);
		return FALSE;
	}

	/* iterate through the directory, keeping the intersection of all the names that
	   have the current basename as a prefix. */
	expand_text = NULL;
	for (element = bar->details->file_info_list; element != NULL; element = element->next) {
		current_file_info = element->data;
		if (eel_str_has_prefix (current_file_info->name, base_name)) {
			if (current_file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
				expand_name = g_strconcat (current_file_info->name, "/", NULL);
			} else {
				expand_name = g_strdup (current_file_info->name);
			}
			if (have_broken_filenames()) {
				expand_text = accumulate_name_locale (expand_text, expand_name);
			} else {
				expand_text = accumulate_name_utf8 (expand_text, expand_name);
			}
			g_free (expand_name);
		}
	}

	if (have_broken_filenames ()) {
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
	NautilusLocationBar *bar;

	if (event->type != GDK_KEY_PRESS) {
		return;
	}

	editable = GTK_EDITABLE (entry);
	keyevent = (GdkEventKey *)event;
	bar = NAUTILUS_LOCATION_BAR (user_data);

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
			if (bar->details->idle_id == 0) {
				bar->details->idle_id = gtk_idle_add (try_to_expand_path, bar);
			}
		}
	} else {
		/* FIXME: Also might be good to do this when you click
		 * to change the position or selection.
		 */
		if (bar->details->idle_id != 0) {
			gtk_idle_remove (bar->details->idle_id);
			bar->details->idle_id = 0;
		}
	}

	nautilus_location_bar_update_label (bar);
}

static void
real_activate (NautilusNavigationBar *navigation_bar)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (navigation_bar);

	/* Put the keyboard focus in the text field when switching to this mode,
	 * and select all text for easy overtyping 
	 */
	gtk_widget_grab_focus (GTK_WIDGET (bar->details->entry));
	nautilus_entry_select_all (bar->details->entry);
}

static void
finalize (GObject *object)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (object);

	g_free (bar->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
destroy (GtkObject *object)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (object);
	
	/* cancel the pending idle call, if any */
	if (bar->details->idle_id != 0) {
		gtk_idle_remove (bar->details->idle_id);
		bar->details->idle_id = 0;
	}
	
	if (bar->details->file_info_list) {
		gnome_vfs_file_info_list_free (bar->details->file_info_list);	
		bar->details->file_info_list = NULL;
	}
	
	g_free (bar->details->current_directory);
	bar->details->current_directory = NULL;
	
	g_free (bar->details->last_location);
	bar->details->last_location = NULL;
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_location_bar_class_init (NautilusLocationBarClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	NautilusNavigationBarClass *navigation_bar_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;
	
	navigation_bar_class = NAUTILUS_NAVIGATION_BAR_CLASS (class);

	navigation_bar_class->activate = real_activate;
	navigation_bar_class->get_location = nautilus_location_bar_get_location;
	navigation_bar_class->set_location = nautilus_location_bar_set_location;
}

static void
nautilus_location_bar_init (NautilusLocationBar *bar)
{
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *event_box;
	GtkWidget *hbox;

	bar->details = g_new0 (NautilusLocationBarDetails, 1);

	hbox = gtk_hbox_new (0, FALSE);

	event_box = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (event_box),
					GNOME_PAD_SMALL);
	label = gtk_label_new (LOCATION_LABEL);
	gtk_container_add   (GTK_CONTAINER (event_box), label);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	g_signal_connect (label, "style_set", 
			  G_CALLBACK (style_set_handler), NULL);

	gtk_box_pack_start (GTK_BOX (hbox), event_box, FALSE, TRUE,
			    GNOME_PAD_SMALL);

	entry = nautilus_entry_new ();

	nautilus_entry_set_special_tab_handling (NAUTILUS_ENTRY (entry), TRUE);
	
	g_signal_connect_object (entry, "activate",
				 G_CALLBACK (nautilus_navigation_bar_location_changed),
				 bar, G_CONNECT_SWAPPED);
	g_signal_connect_object (entry, "event_after",
				 G_CALLBACK (editable_event_after_callback), bar, 0);

	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	eel_accessibility_set_up_label_widget_relation (label, entry);

	gtk_container_add (GTK_CONTAINER (bar), hbox);

	/* Drag source */
	gtk_drag_source_set (GTK_WIDGET (event_box), 
			     GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			     drag_types, G_N_ELEMENTS (drag_types),
			     GDK_ACTION_LINK);
	g_signal_connect_object (event_box, "drag_data_get",
				 G_CALLBACK (drag_data_get_callback), bar, 0);

	/* Drag dest. */
	gtk_drag_dest_set (GTK_WIDGET (bar),
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect (bar, "drag_data_received",
			  G_CALLBACK (drag_data_received_callback), NULL);

	gtk_widget_show_all (hbox);

	bar->details->label = GTK_LABEL (label);
	bar->details->entry = NAUTILUS_ENTRY (entry);	
}

GtkWidget *
nautilus_location_bar_new (NautilusWindow *window)
{
	GtkWidget *bar;
	NautilusLocationBar *location_bar;

	bar = gtk_widget_new (NAUTILUS_TYPE_LOCATION_BAR, NULL);
	location_bar = NAUTILUS_LOCATION_BAR (bar);

	/* Clipboard */
	nautilus_clipboard_set_up_editable
		(GTK_EDITABLE (location_bar->details->entry),
		 nautilus_window_get_ui_container (window),
		 TRUE);

	return bar;
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
	 * thus should not emit the LOCATION_CHANGED signal. */
	
	formatted_location = eel_format_uri_for_display (location);
	nautilus_entry_set_text (NAUTILUS_ENTRY (bar->details->entry),
				 formatted_location);
	set_position_and_selection_to_end (GTK_EDITABLE (bar->details->entry));
	g_free (formatted_location);

	/* free up the cached file info from the previous location */
	g_free (bar->details->current_directory);
	bar->details->current_directory = NULL;
	
	gnome_vfs_file_info_list_free (bar->details->file_info_list);	
	bar->details->file_info_list = NULL;			
	
	/* remember the original location for later comparison */
	
	g_free (bar->details->last_location);
	bar->details->last_location = g_strdup (location);
	nautilus_location_bar_update_label (bar);
}

/**
 * nautilus_location_bar_get_location
 *
 * Get the "URI" represented by the text in the location bar.
 *
 * @bar: A NautilusLocationBar.
 *
 * returns a newly allocated "string" containing the mangled
 * (by eel_make_uri_from_input) text that the user typed in...maybe a URI 
 * but not guaranteed.
 *
 **/
static char *
nautilus_location_bar_get_location (NautilusNavigationBar *navigation_bar) 
{
	NautilusLocationBar *bar;
	char *user_location, *best_uri;

	bar = NAUTILUS_LOCATION_BAR (navigation_bar);
	
	user_location = gtk_editable_get_chars (GTK_EDITABLE (bar->details->entry), 0, -1);
	best_uri = eel_make_uri_from_input (user_location);
	g_free (user_location);
	return best_uri;
}
	       
/**
 * nautilus_location_bar_update_label
 *
 * if the text in the entry matches the uri, set the label to "location", otherwise use "goto"
 *
 **/
static void
nautilus_location_bar_update_label (NautilusLocationBar *bar)
{
	const char *current_text;
	char *current_location;
	
	current_text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));
	current_location = eel_make_uri_from_input (current_text);
	
	if (eel_uris_match (bar->details->last_location, current_location)) {
		gtk_label_set_text (GTK_LABEL (bar->details->label), LOCATION_LABEL);
	} else {		 
		gtk_label_set_text (GTK_LABEL (bar->details->label), GO_TO_LABEL);
	}

	g_free (current_location);
}
