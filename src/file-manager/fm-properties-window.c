/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-properties-window.c - window that lets user modify file properties

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "fm-properties-window.h"

#include "fm-error-reporting.h"
#include <eel/eel-accessibility.h>
#include <eel/eel-ellipsizing-label.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-mime-application-chooser.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-wrap-table.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-macros.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-help.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>
#include <libnautilus-private/nautilus-customization-data.h>
#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-emblem-utils.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <libnautilus-private/nautilus-undo.h>
#include <string.h>

static GHashTable *windows;
static GHashTable *pending_lists;

struct FMPropertiesWindowDetails {	
	GList *original_files;
	GList *target_files;
	
	GtkNotebook *notebook;
	GtkWidget *remove_image_button;
	GtkWidget *icon_selector_window;
	
	GtkTable *basic_table;
	GtkTable *permissions_table;

	GtkWidget *icon_image;

	NautilusEntry *name_field;
	char *pending_name;

	GtkLabel *directory_contents_title_field;
	GtkLabel *directory_contents_value_field;
	guint update_directory_contents_timeout_id;
	guint update_files_timeout_id;

	GList *directory_contents_widgets;
	int directory_contents_row;

	GList *special_flags_widgets;
	int first_special_flags_row;
	int num_special_flags_rows;

	GList *emblem_buttons;
	GHashTable *initial_emblems;

	GList *permission_buttons;
	GHashTable *initial_permissions;

	GList *value_fields;

	GList *mime_list;

	gboolean deep_count_finished;

	guint total_count;
	GnomeVFSFileSize total_size;

	guint long_operation_underway;

 	GList *changed_files;
};

enum {
	PERMISSIONS_CHECKBOXES_OWNER_ROW,
	PERMISSIONS_CHECKBOXES_GROUP_ROW,
	PERMISSIONS_CHECKBOXES_OTHERS_ROW,
	PERMISSIONS_CHECKBOXES_ROW_COUNT
};

enum {
	PERMISSIONS_CHECKBOXES_READ_COLUMN,
	PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
	PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
	PERMISSIONS_CHECKBOXES_COLUMN_COUNT
};

enum {
	TITLE_COLUMN,
	VALUE_COLUMN,
	COLUMN_COUNT
};

typedef struct {
	GList *original_files;
	GList *target_files;
	GtkWidget *parent_widget;
	char *pending_key;
	GHashTable *pending_files;
} StartupData;

/* drag and drop definitions */

enum {
	TARGET_URI_LIST,
	TARGET_GNOME_URI_LIST,
	TARGET_RESET_BACKGROUND
};

static GtkTargetEntry target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "x-special/gnome-icon-list",  0, TARGET_GNOME_URI_LIST },
	{ "x-special/gnome-reset-background", 0, TARGET_RESET_BACKGROUND }
};

#define DIRECTORY_CONTENTS_UPDATE_INTERVAL	200 /* milliseconds */
#define FILES_UPDATE_INTERVAL			200 /* milliseconds */
#define STANDARD_EMBLEM_HEIGHT			52
#define EMBLEM_LABEL_SPACING			2

static void directory_contents_value_field_update (FMPropertiesWindow *window);
static void file_changed_callback                 (NautilusFile       *file,
						   gpointer            user_data);
static void permission_button_update              (FMPropertiesWindow *window,
						   GtkToggleButton    *button);
static void value_field_update                    (FMPropertiesWindow *window,
						   GtkLabel           *field);
static void properties_window_update              (FMPropertiesWindow *window,
						   GList              *files);
static void is_directory_ready_callback           (NautilusFile       *file,
						   gpointer            data);
static void cancel_group_change_callback          (gpointer            callback_data);
static void cancel_owner_change_callback          (gpointer            callback_data);
static void parent_widget_destroyed_callback      (GtkWidget          *widget,
						   gpointer            callback_data);
static void select_image_button_callback          (GtkWidget          *widget,
						   FMPropertiesWindow *properties_window);
static void set_icon_callback                     (const char         *icon_path,
						   FMPropertiesWindow *properties_window);
static void remove_image_button_callback          (GtkWidget          *widget,
						   FMPropertiesWindow *properties_window);
static void remove_pending                        (StartupData        *data,
						   gboolean            cancel_call_when_ready,
						   gboolean            cancel_timed_wait,
						   gboolean            cancel_destroy_handler);
static void append_extension_pages                (FMPropertiesWindow *window);

GNOME_CLASS_BOILERPLATE (FMPropertiesWindow, fm_properties_window,
			 GtkWindow, GTK_TYPE_WINDOW);

typedef struct {
	NautilusFile *file;
	char *name;
} FileNamePair;

static FileNamePair *
file_name_pair_new (NautilusFile *file, const char *name)
{
	FileNamePair *new_pair;

	new_pair = g_new0 (FileNamePair, 1);
	new_pair->file = file;
	new_pair->name = g_strdup (name);

	nautilus_file_ref (file);

	return new_pair;
}

static void
file_name_pair_free (FileNamePair *pair)
{
	nautilus_file_unref (pair->file);
	g_free (pair->name);
	
	g_free (pair);
}

static gboolean
is_multi_file_window (FMPropertiesWindow *window)
{
	GList *l;
	int count;
	
	count = 0;
	
	for (l = window->details->original_files; l != NULL; l = l->next) {
		if (!nautilus_file_is_gone (NAUTILUS_FILE (l->data))) {
			count++;
			if (count > 1) {
				return TRUE;
			}	
		}
	}

	return FALSE;
}

static NautilusFile *
get_original_file (FMPropertiesWindow *window) 
{
	g_return_val_if_fail (!is_multi_file_window (window), NULL);

	return NAUTILUS_FILE (window->details->original_files->data);
}

static NautilusFile *
get_target_file_for_original_file (NautilusFile *file)
{
	NautilusFile *target_file;
	char *uri_to_display;
	GnomeVFSVolume *volume;
	GnomeVFSDrive *drive;
	NautilusDesktopLink *link;

	target_file = NULL;
	if (nautilus_file_has_volume (file)) {
		volume = nautilus_file_get_volume (file);
		if (volume != NULL) {
			uri_to_display = gnome_vfs_volume_get_activation_uri (volume);
			target_file = nautilus_file_get (uri_to_display);
			g_free (uri_to_display);
		}
	} else if (nautilus_file_has_drive (file)) {
		drive = nautilus_file_get_drive (file);
		if (drive != NULL) {
			uri_to_display = gnome_vfs_drive_get_activation_uri (drive);
			target_file = nautilus_file_get (uri_to_display);
			g_free (uri_to_display);
		}
	} else if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));
		
		/* map to linked URI for these types of links */
		uri_to_display = nautilus_desktop_link_get_activation_uri (link);
		if (uri_to_display) {
                        target_file = nautilus_file_get (uri_to_display);
                        g_free (uri_to_display);
                }
		
		g_object_unref (link);
        }


	if (target_file != NULL) {
		return target_file;
	}

	/* Ref passed-in file here since we've decided to use it. */
	nautilus_file_ref (file);
	return file;
}

static NautilusFile *
get_target_file (FMPropertiesWindow *window)
{
	return NAUTILUS_FILE (window->details->target_files->data);
}

static void
add_prompt (GtkVBox *vbox, const char *prompt_text, gboolean pack_at_start)
{
	GtkWidget *prompt;

	prompt = gtk_label_new (prompt_text);
   	gtk_label_set_justify (GTK_LABEL (prompt), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (prompt), TRUE);
	gtk_widget_show (prompt);
	if (pack_at_start) {
		gtk_box_pack_start (GTK_BOX (vbox), prompt, FALSE, FALSE, 0);
	} else {
		gtk_box_pack_end (GTK_BOX (vbox), prompt, FALSE, FALSE, 0);
	}
}

static void
add_prompt_and_separator (GtkVBox *vbox, const char *prompt_text)
{
	GtkWidget *separator_line;

	add_prompt (vbox, prompt_text, FALSE);

 	separator_line = gtk_hseparator_new ();
  	gtk_widget_show (separator_line);
  	gtk_box_pack_end (GTK_BOX (vbox), separator_line, TRUE, TRUE, GNOME_PAD_BIG);
}

static GdkPixbuf *
get_pixbuf_for_properties_window (FMPropertiesWindow *window)
{
	GdkPixbuf *pixbuf;
	char *icon;
	GList *l;
	
	icon = NULL;
	for (l = window->details->original_files; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		if (!icon) {
			icon = nautilus_icon_factory_get_icon_for_file (file, FALSE);
		} else {
			char *new_icon;
			new_icon = nautilus_icon_factory_get_icon_for_file (file, FALSE);
			if (!new_icon || strcmp (new_icon, icon)) {
				g_free (icon);
				g_free (new_icon);
				icon = NULL;
				break;
			}
			g_free (new_icon);
		}
	}

	if (!icon) {
		icon = g_strdup ("gnome-fs-regular");
	}
	
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon (icon, NULL,
							    NAUTILUS_ICON_SIZE_STANDARD,
							    NULL, NULL,
							    TRUE, NULL);

	g_free (icon);

	return pixbuf;
}


static void
update_properties_window_icon (GtkImage *image)
{
	GdkPixbuf	*pixbuf;
	FMPropertiesWindow *window;

	window = g_object_get_data (G_OBJECT (image), "properties_window");
	
	pixbuf = get_pixbuf_for_properties_window (window);

	gtk_image_set_from_pixbuf (image, pixbuf);

	gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
	
	g_object_unref (pixbuf);
}

/* utility to test if a uri refers to a local image */
static gboolean
uri_is_local_image (const char *uri)
{
	GdkPixbuf *pixbuf;
	char *image_path;
	
	image_path = gnome_vfs_get_local_path_from_uri (uri);
	if (image_path == NULL) {
		return FALSE;
	}

	pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);
	g_free (image_path);
	
	if (pixbuf == NULL) {
		return FALSE;
	}
	g_object_unref (pixbuf);
	return TRUE;
}


static void
reset_icon (FMPropertiesWindow *properties_window)
{
	GList *l;

	for (l = properties_window->details->original_files; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		nautilus_file_set_metadata (file,
					    NAUTILUS_METADATA_KEY_ICON_SCALE,
					    NULL, NULL);
		nautilus_file_set_metadata (file,
					    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
					    NULL, NULL);
	}
	
	gtk_widget_set_sensitive (properties_window->details->remove_image_button, FALSE);
}


static void  
fm_properties_window_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data,
					 guint info, guint time)
{
	char **uris;
	gboolean exactly_one;
	GtkImage *image;
 	GtkWindow *window; 

	image = GTK_IMAGE (widget);
 	window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (image)));

	if (info == TARGET_RESET_BACKGROUND) {
		reset_icon (FM_PROPERTIES_WINDOW (window));
		
		return;
	}
	
	uris = g_strsplit (selection_data->data, "\r\n", 0);
	exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');


	if (!exactly_one) {
		eel_show_error_dialog
			(_("You can't assign more than one custom icon at a time!"),
			 _("Please drag just one image to set a custom icon."), 
			 _("More Than One Image"),
			 window);
	} else {		
		if (uri_is_local_image (uris[0])) {			
			set_icon_callback (gnome_vfs_get_local_path_from_uri (uris[0]), 
					   FM_PROPERTIES_WINDOW (window));
		} else {	
			if (eel_is_remote_uri (uris[0])) {
				eel_show_error_dialog
					(_("The file that you dropped is not local."),
					 _("You can only use local images as custom icons."), 
					 _("Local Images Only"),
					 window);
				
			} else {
				eel_show_error_dialog
					(_("The file that you dropped is not an image."),
					 _("You can only use local images as custom icons."),
					 _("Images Only"),
					 window);
			}
		}		
	}
	g_strfreev (uris);
}

static GtkWidget *
create_image_widget (FMPropertiesWindow *window)
{
 	GtkWidget *image;
	GdkPixbuf *pixbuf;
	
	pixbuf = get_pixbuf_for_properties_window (window);
	
	image = gtk_image_new ();
	window->details->icon_image = image;

	/* prepare the image to receive dropped objects to assign custom images */
	gtk_drag_dest_set (GTK_WIDGET (image),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   target_table, G_N_ELEMENTS (target_table),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect (image, "drag_data_received",
			  G_CALLBACK (fm_properties_window_drag_data_received), NULL);

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

	g_object_unref (pixbuf);

	g_object_set_data (G_OBJECT (image), "properties_window", window);

	/* React to icon theme changes. */
	g_signal_connect_object (nautilus_icon_factory_get (),
				 "icons_changed",
				 G_CALLBACK (update_properties_window_icon),
				 image, G_CONNECT_SWAPPED);

	return image;
}

static void
update_name_field (FMPropertiesWindow *window)
{
	NautilusFile *file;
	const char *original_name;
	char *current_name, *displayed_name;

	if (is_multi_file_window (window)) {
		/* Multifile property dialog, show all names */
		GString *str;
		char *name;
		gboolean first;
		GList *l;
		
		str = g_string_new ("");

		first = TRUE;

		for (l = window->details->target_files; l != NULL; l = l->next) {
			file = NAUTILUS_FILE (l->data);

			if (!nautilus_file_is_gone (file)) {
				if (!first) {
					g_string_append (str, ", ");
				} 
				first = FALSE;
				
				name = nautilus_file_get_display_name (file);
				g_string_append (str, name);
				g_free (name);
			}
		}
		gtk_entry_set_text (GTK_ENTRY (window->details->name_field), 
				    str->str);
		g_string_free (str, TRUE);

		gtk_editable_set_editable (GTK_EDITABLE (window->details->name_field), 
					   FALSE);
	} else {
		NautilusFile *file;

		file = get_original_file (window);
			
		if (file == NULL || nautilus_file_is_gone (file)) {
			gtk_entry_set_text (GTK_ENTRY (window->details->name_field), "");
			return;
		}

		original_name = (const char *) g_object_get_data (G_OBJECT (window->details->name_field),
								  "original_name");
		
		/* If the file name has changed since the original name was stored,
		 * update the text in the text field, possibly (deliberately) clobbering
		 * an edit in progress. If the name hasn't changed (but some other
		 * aspect of the file might have), then don't clobber changes.
		 */
		current_name = nautilus_file_get_display_name (file);
		if (original_name == NULL || 
		    eel_strcmp (original_name, current_name) != 0) {
			g_object_set_data_full (G_OBJECT (window->details->name_field),
						"original_name",
						current_name,
						g_free);
			
			/* Only reset the text if it's different from what is
			 * currently showing. This causes minimal ripples (e.g.
			 * selection change).
			 */
			displayed_name = gtk_editable_get_chars (GTK_EDITABLE (window->details->name_field), 0, -1);
			if (strcmp (displayed_name, current_name) != 0) {
				gtk_entry_set_text (GTK_ENTRY (window->details->name_field), current_name);
			}
			g_free (displayed_name);
		} else {
			g_free (current_name);
		}
		
		/* 
		 * The UI would look better here if the name were just drawn as
		 * a plain label in the case where it's not editable, with no
		 * border at all. That doesn't seem to be possible with GtkEntry,
		 * so we'd have to swap out the widget to achieve it. I don't
		 * care enough to change this now.
		 */
		gtk_widget_set_sensitive (GTK_WIDGET (window->details->name_field), 
					  nautilus_file_can_rename (file));
	}
}

static void
name_field_restore_original_name (NautilusEntry *name_field)
{
	const char *original_name;
	char *displayed_name;

	original_name = (const char *) g_object_get_data (G_OBJECT (name_field),
							  "original_name");

	if (!original_name) {
		return;
	}

	displayed_name = gtk_editable_get_chars (GTK_EDITABLE (name_field), 0, -1);

	if (strcmp (original_name, displayed_name) != 0) {
		gtk_entry_set_text (GTK_ENTRY (name_field), original_name);
	}
	nautilus_entry_select_all (name_field);

	g_free (displayed_name);
}

static void
rename_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	FMPropertiesWindow *window;
	char *new_name;

	window = FM_PROPERTIES_WINDOW (callback_data);

	/* Complain to user if rename failed. */
	if (result != GNOME_VFS_OK) {
		new_name = window->details->pending_name;
		fm_report_error_renaming_file (file, 
					       window->details->pending_name, 
					       result,
					       GTK_WINDOW (window));
		if (window->details->name_field != NULL) {
			name_field_restore_original_name (window->details->name_field);
		}
	}

	g_object_unref (window);
}

static void
set_pending_name (FMPropertiesWindow *window, const char *name)
{
	g_free (window->details->pending_name);
	window->details->pending_name = g_strdup (name);
}

static void
name_field_done_editing (NautilusEntry *name_field, FMPropertiesWindow *window)
{
	NautilusFile *file;
	char *new_name;
	
	g_return_if_fail (NAUTILUS_IS_ENTRY (name_field));

	/* Don't apply if the dialog has more than one file */
	if (is_multi_file_window (window)) {
		return;
	}	

	file = get_original_file (window);

	/* This gets called when the window is closed, which might be
	 * caused by the file having been deleted.
	 */
	if (nautilus_file_is_gone (file)) {
		return;
	}

	new_name = gtk_editable_get_chars (GTK_EDITABLE (name_field), 0, -1);

	/* Special case: silently revert text if new text is empty. */
	if (strlen (new_name) == 0) {
		name_field_restore_original_name (NAUTILUS_ENTRY (name_field));
	} else {
		set_pending_name (window, new_name);
		g_object_ref (window);
		nautilus_file_rename (file, new_name,
				      rename_callback, window);
	}

	g_free (new_name);
}

static gboolean
name_field_focus_out (NautilusEntry *name_field,
		      GdkEventFocus *event,
		      gpointer callback_data)
{
	g_assert (FM_IS_PROPERTIES_WINDOW (callback_data));

	if (GTK_WIDGET_SENSITIVE (name_field)) {
		name_field_done_editing (name_field, FM_PROPERTIES_WINDOW (callback_data));
	}

	return FALSE;
}

static void
name_field_activate (NautilusEntry *name_field, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_ENTRY (name_field));
	g_assert (FM_IS_PROPERTIES_WINDOW (callback_data));

	/* Accept changes. */
	name_field_done_editing (name_field, FM_PROPERTIES_WINDOW (callback_data));

	nautilus_entry_select_all_at_idle (name_field);
}

static gboolean
file_has_keyword (NautilusFile *file, const char *keyword)
{
	GList *keywords, *word;

	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, keyword, (GCompareFunc) strcmp);
	eel_g_list_free_deep (keywords);
	
	return (word != NULL);
}

static void
get_initial_emblem_state (FMPropertiesWindow *window,
			  const char *name,
			  GList **on,
			  GList **off)
{
	GList *l;
	
	*on = NULL;
	*off = NULL;
	
	for (l = window->details->original_files; l != NULL; l = l->next) {
		GList *initial_emblems;
		
		initial_emblems = g_hash_table_lookup (window->details->initial_emblems,
						       l->data);
		
		if (g_list_find_custom (initial_emblems, name, (GCompareFunc) strcmp)) {
			*on = g_list_prepend (*on, l->data);
		} else {
			*off = g_list_prepend (*off, l->data);
		}
	}
}

static void
emblem_button_toggled (GtkToggleButton *button,
		       FMPropertiesWindow *window)
{
	GList *l;
	GList *keywords;
	GList *word;
	char *name;
	GList *files_on;
	GList *files_off;

	name = g_object_get_data (G_OBJECT (button), "nautilus_emblem_name");

	files_on = NULL;
	files_off = NULL;
	if (gtk_toggle_button_get_active (button)
	    && !gtk_toggle_button_get_inconsistent (button)) {
		/* Go to the initial state unless the initial state was 
		   consistent */
		get_initial_emblem_state (window, name, 
					  &files_on, &files_off);
		
		if (!(files_on && files_off)) {
			g_list_free (files_on);
			g_list_free (files_off);
			files_on = g_list_copy (window->details->original_files);
			files_off = NULL;
		}
	} else if (gtk_toggle_button_get_inconsistent (button)
		   && !gtk_toggle_button_get_active (button)) {
		files_on = g_list_copy (window->details->original_files);
		files_off = NULL;
	} else {
		files_off = g_list_copy (window->details->original_files);
		files_on = NULL;
	}
	
	g_signal_handlers_block_by_func (G_OBJECT (button), 
					 G_CALLBACK (emblem_button_toggled),
					 window);
	
	gtk_toggle_button_set_active (button, files_on != NULL);
	gtk_toggle_button_set_inconsistent (button, files_on && files_off);

	g_signal_handlers_unblock_by_func (G_OBJECT (button), 
					   G_CALLBACK (emblem_button_toggled),
					   window);

	for (l = files_on; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		keywords = nautilus_file_get_keywords (file);
		
		word = g_list_find_custom (keywords, name,  (GCompareFunc)strcmp);
		if (!word) {
			keywords = g_list_prepend (keywords, g_strdup (name));
		}
		nautilus_file_set_keywords (file, keywords);
		eel_g_list_free_deep (keywords);
	}
	
	for (l = files_off; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);

		keywords = nautilus_file_get_keywords (file);
		
		word = g_list_find_custom (keywords, name,  (GCompareFunc)strcmp);
		if (word) {
			keywords = g_list_remove_link (keywords, word);
			eel_g_list_free_deep (word);
		}
		nautilus_file_set_keywords (file, keywords);
		eel_g_list_free_deep (keywords);
	}	

	g_list_free (files_on);
	g_list_free (files_off);	
}

static void
emblem_button_update (FMPropertiesWindow *window,
			GtkToggleButton *button)
{
	GList *l;
	char *name;
	gboolean all_set;
	gboolean all_unset;

	name = g_object_get_data (G_OBJECT (button), "nautilus_emblem_name");

	all_set = TRUE;
	all_unset = TRUE;
	for (l = window->details->original_files; l != NULL; l = l->next) {
		gboolean has_keyword;
		NautilusFile *file;

		file = NAUTILUS_FILE (l->data);
		
		has_keyword = file_has_keyword (file, name);

		if (has_keyword) {
			all_unset = FALSE;
		} else {
			all_set = FALSE;
		}
	}
	
	g_signal_handlers_block_by_func (G_OBJECT (button), 
					 G_CALLBACK (emblem_button_toggled),
					 window);

	gtk_toggle_button_set_active (button, !all_unset);
	gtk_toggle_button_set_inconsistent (button, !all_unset && !all_set);

	g_signal_handlers_unblock_by_func (G_OBJECT (button), 
					   G_CALLBACK (emblem_button_toggled),
					   window);

}

static void
update_properties_window_title (FMPropertiesWindow *window)
{
	char *name, *title;

	g_return_if_fail (GTK_IS_WINDOW (window));

	if (is_multi_file_window (window)) {
		title = g_strdup_printf (_("Properties"));
	} else {
		name = nautilus_file_get_display_name (get_original_file (window));
		title = g_strdup_printf (_("%s Properties"), name);
		g_free (name);
	}
	
  	gtk_window_set_title (GTK_WINDOW (window), title);

	g_free (title);
}

static void
clear_extension_pages (FMPropertiesWindow *window)
{
	int i;
	int num_pages;
	GtkWidget *page;

	num_pages = gtk_notebook_get_n_pages
				(GTK_NOTEBOOK (window->details->notebook));

	for (i = 0; i < num_pages; i++) {
		page = gtk_notebook_get_nth_page
				(GTK_NOTEBOOK (window->details->notebook), i);

		if (g_object_get_data (G_OBJECT (page), "is-extension-page")) {
			gtk_notebook_remove_page
				(GTK_NOTEBOOK (window->details->notebook), i);
			num_pages--;
			i--;
		}
	}
}

static void
refresh_extension_pages (FMPropertiesWindow *window)
{
	clear_extension_pages (window);
	append_extension_pages (window);	
}

static void
remove_from_dialog (FMPropertiesWindow *window,
		    NautilusFile *file)
{
	int index;
	GList *original_link;
	GList *target_link;
	NautilusFile *original_file;
	NautilusFile *target_file;

	index = g_list_index (window->details->target_files, file);
	if (index == -1) {
		index = g_list_index (window->details->original_files, file);
		g_return_if_fail (index != -1);
	}	

	original_link = g_list_nth (window->details->original_files, index);
	target_link = g_list_nth (window->details->target_files, index);

	g_return_if_fail (original_link && target_link);

	original_file = NAUTILUS_FILE (original_link->data);
	target_file = NAUTILUS_FILE (target_link->data);
	
	window->details->original_files = g_list_remove_link (window->details->original_files, original_link);
	g_list_free (original_link);

	window->details->target_files = g_list_remove_link (window->details->target_files, target_link);
	g_list_free (target_link);

	g_hash_table_remove (window->details->initial_emblems, original_file);
	g_hash_table_remove (window->details->initial_permissions, target_file);

	g_signal_handlers_disconnect_by_func (original_file,
					      G_CALLBACK (file_changed_callback),
					      window);
	g_signal_handlers_disconnect_by_func (target_file,
					      G_CALLBACK (file_changed_callback),
					      window);

	nautilus_file_monitor_remove (original_file, &window->details->original_files);
	nautilus_file_monitor_remove (target_file, &window->details->target_files);

	nautilus_file_unref (original_file);
	nautilus_file_unref (target_file);
	
}

static gboolean
mime_list_equal (GList *a, GList *b)
{
	while (a && b) {
		if (strcmp (a->data, b->data)) {
			return FALSE;
		}	
		a = a->next;
		b = b->next;
	}

	return (a == b);
}

static GList *
get_mime_list (FMPropertiesWindow *window)
{
	GList *ret;
	GList *l;
	
	ret = NULL;
	for (l = window->details->target_files; l != NULL; l = l->next) {
		ret = g_list_append (ret, nautilus_file_get_mime_type (NAUTILUS_FILE (l->data)));
	}
	ret = g_list_reverse (ret);
	return ret;
}

static void
properties_window_update (FMPropertiesWindow *window, 
			  GList *files)
{
	GList *l;
	GList *mime_list;
	GList *tmp;
	NautilusFile *changed_file;
	gboolean dirty_original = FALSE;
	gboolean dirty_target = FALSE;

	if (files == NULL) {
		dirty_original = TRUE;
		dirty_target = TRUE;
	}

	for (tmp = files; tmp != NULL; tmp = tmp->next) {
		changed_file = NAUTILUS_FILE (tmp->data);

		if (changed_file && nautilus_file_is_gone (changed_file)) {
			/* Remove the file from the property dialog */
			remove_from_dialog (window, changed_file);
			changed_file = NULL;
			
			if (window->details->original_files == NULL) {
				return;
			}
		}		
		if (changed_file == NULL ||
		    g_list_find (window->details->original_files, changed_file)) {
			dirty_original = TRUE;
		}
		if (changed_file == NULL ||
		    g_list_find (window->details->target_files, changed_file)) {
			dirty_target = TRUE;
		}

	}

	if (dirty_original) {
		update_properties_window_title (window);
		update_properties_window_icon (GTK_IMAGE (window->details->icon_image));

		update_name_field (window);

		for (l = window->details->emblem_buttons; l != NULL; l = l->next) {
			emblem_button_update (window, GTK_TOGGLE_BUTTON (l->data));
		}
		
		/* If any of the value fields start to depend on the original
		 * value, value_field_updates should be added here */
	}

	if (dirty_target) {
		for (l = window->details->permission_buttons; l != NULL; l = l->next) {
			permission_button_update (window, GTK_TOGGLE_BUTTON (l->data));
		}
		
		for (l = window->details->value_fields; l != NULL; l = l->next) {
			value_field_update (window, GTK_LABEL (l->data));
		}
	}

	mime_list = get_mime_list (window);

	if (!window->details->mime_list) {
		window->details->mime_list = mime_list;
	} else {
		if (!mime_list_equal (window->details->mime_list, mime_list)) {
			refresh_extension_pages (window);			
		}
		
		eel_g_list_free_deep (window->details->mime_list);
		window->details->mime_list = mime_list;
	}
}

static gboolean
update_files_callback (gpointer data)
{
 	FMPropertiesWindow *window;
 
 	window = FM_PROPERTIES_WINDOW (data);
 
	window->details->update_files_timeout_id = 0;

	properties_window_update (window, window->details->changed_files);
	
	if (window->details->original_files == NULL) {
		/* Close the window if no files are left */
		gtk_widget_destroy (GTK_WIDGET (window));
	} else {
		nautilus_file_list_free (window->details->changed_files);
		window->details->changed_files = NULL;
	}
	
 	return FALSE;
 }

static void
schedule_files_update (FMPropertiesWindow *window)
 {
 	g_assert (FM_IS_PROPERTIES_WINDOW (window));
 
	if (window->details->update_files_timeout_id == 0) {
		window->details->update_files_timeout_id
			= g_timeout_add (FILES_UPDATE_INTERVAL,
					 update_files_callback,
 					 window);
 	}
 }

static gboolean
file_list_attributes_identical (GList *file_list, const char *attribute_name)
{
	gboolean identical;
	char *first_attr;
	GList *l;
	
	first_attr = NULL;
	identical = TRUE;
	
	for (l = file_list; l != NULL; l = l->next) {
		NautilusFile *file;

		file = NAUTILUS_FILE (l->data);
	
		if (nautilus_file_is_gone (file)) {
			continue;
		}

		if (first_attr == NULL) {
			first_attr = nautilus_file_get_string_attribute_with_default (file, attribute_name);
		} else {
			char *attr;
			attr = nautilus_file_get_string_attribute_with_default (file, attribute_name);
			if (strcmp (attr, first_attr)) {
				identical = FALSE;
				g_free (attr);
				break;
			}
			g_free (attr);
		}
	}

	g_free (first_attr);
	return identical;
}

static char *
file_list_get_string_attribute (GList *file_list, 
				const char *attribute_name,
				const char *inconsistent_value)
{
	if (file_list_attributes_identical (file_list, attribute_name)) {
		GList *l;
		
		for (l = file_list; l != NULL; l = l->next) {
			NautilusFile *file;
			
			file = NAUTILUS_FILE (l->data);
			if (!nautilus_file_is_gone (file)) {
				return nautilus_file_get_string_attribute_with_default
					(file, 
					 attribute_name);
			}
		}
		return g_strdup (_("unknown"));
	} else {
		return g_strdup (inconsistent_value);
	}
}


static gboolean 
file_list_all_local (GList *file_list)
{
	GList *l;
	for (l = file_list; l != NULL; l = l->next) {
		if (!nautilus_file_is_local (NAUTILUS_FILE (l->data))) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
file_list_all_directories (GList *file_list)
{
	GList *l;
	for (l = file_list; l != NULL; l = l->next) {
		if (!nautilus_file_is_directory (NAUTILUS_FILE (l->data))) {
			return FALSE;
		}
	}
	return TRUE;
}

static void
value_field_update_internal (GtkLabel *label, 
			     GList *file_list, 
			     gboolean ellipsize_text)
{
	const char *attribute_name;
	char *attribute_value;
	char *inconsistent_string;

	g_assert (GTK_IS_LABEL (label));
	g_assert (!ellipsize_text || EEL_IS_ELLIPSIZING_LABEL (label));

	attribute_name = g_object_get_data (G_OBJECT (label), "file_attribute");
	inconsistent_string = g_object_get_data (G_OBJECT (label), "inconsistent_string");
	attribute_value = file_list_get_string_attribute (file_list, 
							  attribute_name,
							  inconsistent_string);

	if (ellipsize_text) {
		eel_ellipsizing_label_set_text (EEL_ELLIPSIZING_LABEL (label), 
						attribute_value);
	} else {
		gtk_label_set_text (label, attribute_value);
	}
	g_free (attribute_value);	
}

static void
value_field_update (FMPropertiesWindow *window, GtkLabel *label)
{
	gboolean ellipsize_text;
	gboolean use_original;	

	ellipsize_text = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label), "ellipsize_text"));
	use_original = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label), "use_original"));

	value_field_update_internal (label, 
				     (use_original ?
				      window->details->original_files : 
				      window->details->target_files),
				     ellipsize_text);
}

static GtkLabel *
attach_label (GtkTable *table,
	      int row,
	      int column,
	      const char *initial_text,
	      gboolean right_aligned,
	      gboolean bold,
	      gboolean ellipsize_text,
	      gboolean selectable,
	      gboolean mnemonic)
{
	GtkWidget *label_field;

	if (ellipsize_text) {
		label_field = eel_ellipsizing_label_new (initial_text);
	} else if (mnemonic) {
		label_field = gtk_label_new_with_mnemonic (initial_text);
	} else {
		label_field = gtk_label_new (initial_text);
	}

	if (selectable) {
		gtk_label_set_selectable (GTK_LABEL (label_field), TRUE);
	}
	
	if (bold) {
		eel_gtk_label_make_bold (GTK_LABEL (label_field));
	}
	gtk_misc_set_alignment (GTK_MISC (label_field), right_aligned ? 1 : 0, 0.5);
	gtk_widget_show (label_field);
	gtk_table_attach (table, label_field,
			  column, column + 1,
			  row, row + 1,
			  ellipsize_text
			    ? GTK_FILL | GTK_EXPAND
			    : GTK_FILL,
			  0,
			  0, 0);

	return GTK_LABEL (label_field);
}	      

static GtkLabel *
attach_value_label (GtkTable *table,
	      		  int row,
	      		  int column,
	      		  const char *initial_text)
{
	return attach_label (table, row, column, initial_text, FALSE, FALSE, FALSE, TRUE, FALSE);
}

static GtkLabel *
attach_ellipsizing_value_label (GtkTable *table,
				int row,
				int column,
				const char *initial_text)
{
	return attach_label (table, row, column, initial_text, FALSE, FALSE, TRUE, TRUE, FALSE);
}

static void
attach_value_field_internal (FMPropertiesWindow *window,
			     GtkTable *table,
			     int row,
			     int column,
			     const char *file_attribute_name,
			     const char *inconsistent_string,
			     gboolean show_original,
			     gboolean ellipsize_text)
{
	GtkLabel *value_field;

	if (ellipsize_text) {
		value_field = attach_ellipsizing_value_label (table, row, column, "");
	} else {
		value_field = attach_value_label (table, row, column, "");
	}

  	/* Stash a copy of the file attribute name in this field for the callback's sake. */
	g_object_set_data_full (G_OBJECT (value_field), "file_attribute",
				g_strdup (file_attribute_name), g_free);

	g_object_set_data_full (G_OBJECT (value_field), "inconsistent_string",
				g_strdup (inconsistent_string), g_free);

	g_object_set_data (G_OBJECT (value_field), "show_original", GINT_TO_POINTER (show_original));
	g_object_set_data (G_OBJECT (value_field), "ellipsize_text", GINT_TO_POINTER (ellipsize_text));

	window->details->value_fields = g_list_prepend (window->details->value_fields,
							value_field);
}			     

static void
attach_value_field (FMPropertiesWindow *window,
		    GtkTable *table,
		    int row,
		    int column,
		    const char *file_attribute_name,
		    const char *inconsistent_string,
		    gboolean show_original)
{
	attach_value_field_internal (window, 
				     table, row, column, 
				     file_attribute_name, 
				     inconsistent_string,
				     show_original,
				     FALSE);
}

static void
attach_ellipsizing_value_field (FMPropertiesWindow *window,
				GtkTable *table,
		    	  	int row,
		    		int column,
		    		const char *file_attribute_name,
				const char *inconsistent_string,
				gboolean show_original)
{
	attach_value_field_internal (window,
				     table, row, column, 
				     file_attribute_name, 
				     inconsistent_string, 
				     show_original,
				     TRUE);
}

static void
group_change_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	g_assert (callback_data == NULL);
	
	/* Report the error if it's an error. */
	eel_timed_wait_stop (cancel_group_change_callback, file);
	fm_report_error_setting_group (file, result, NULL);
	nautilus_file_unref (file);
}

static void
cancel_group_change_callback (gpointer callback_data)
{
	NautilusFile *file;

	file = NAUTILUS_FILE (callback_data);
	nautilus_file_cancel (file, group_change_callback, NULL);
	nautilus_file_unref (file);
}

static void
activate_group_callback (GtkMenuItem *menu_item, FileNamePair *pair)
{
	g_assert (pair != NULL);

	/* Try to change file group. If this fails, complain to user. */
	nautilus_file_ref (pair->file);
	eel_timed_wait_start
		(cancel_group_change_callback,
		 pair->file,
		 _("Cancel Group Change?"),
		 _("Changing group."),
		 NULL); /* FIXME bugzilla.gnome.org 42397: Parent this? */
	nautilus_file_set_group
		(pair->file, pair->name,
		 group_change_callback, NULL);
}

static GtkWidget *
create_group_menu_item (NautilusFile *file, const char *group_name)
{
	GtkWidget *menu_item;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (group_name != NULL);

	menu_item = gtk_menu_item_new_with_label (group_name);
	gtk_widget_show (menu_item);

	eel_gtk_signal_connect_free_data_custom (GTK_OBJECT (menu_item),
						 "activate",
						 G_CALLBACK (activate_group_callback),
						 file_name_pair_new (file, group_name),
						 (GDestroyNotify)file_name_pair_free);

	return menu_item;
}

static void
synch_groups_menu (GtkOptionMenu *option_menu, NautilusFile *file)
{
	GList *groups;
	GList *node;
	GtkWidget *new_menu;
	GtkWidget *menu_item;
	const char *group_name;
	char *current_group_name;
	int group_index;
	int current_group_index;

	g_assert (GTK_IS_OPTION_MENU (option_menu));
	g_assert (NAUTILUS_IS_FILE (file));

	current_group_name = nautilus_file_get_string_attribute (file, "group");
	current_group_index = -1;

	groups = nautilus_file_get_settable_group_names (file);
	new_menu = gtk_menu_new ();

	for (node = groups, group_index = 0; node != NULL; node = node->next, ++group_index) {
		group_name = (const char *)node->data;
		if (strcmp (group_name, current_group_name) == 0) {
			current_group_index = group_index;
		}
		menu_item = create_group_menu_item (file, group_name);
		gtk_menu_shell_append (GTK_MENU_SHELL (new_menu), menu_item);
	}

	/* If current group wasn't in list, we prepend it (with a separator). 
	 * This can happen if the current group is an id with no matching
	 * group in the groups file.
	 */
	if (current_group_index < 0 && current_group_name != NULL) {
		if (groups != NULL) {
			menu_item = gtk_menu_item_new ();
			gtk_widget_show (menu_item);
			gtk_menu_shell_prepend (GTK_MENU_SHELL (new_menu), menu_item);
		}
		menu_item = create_group_menu_item (file, current_group_name);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (new_menu), menu_item);
		current_group_index = 0;
	}

        /* We create and attach a new menu here because adding/removing
         * items from existing menu screws up the size of the option menu.
         */
        gtk_option_menu_set_menu (option_menu, new_menu);

	gtk_option_menu_set_history (option_menu, current_group_index);

	g_free (current_group_name);
	eel_g_list_free_deep (groups);
}

static GtkOptionMenu *
attach_option_menu (GtkTable *table,
		    int row)
{
	GtkWidget *option_menu;
	GtkWidget *aligner;

	option_menu = gtk_option_menu_new ();
	gtk_widget_show (option_menu);

	/* Put option menu in alignment to make it left-justified
	 * but minimally sized.
	 */	
	aligner = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_widget_show (aligner);

	gtk_container_add (GTK_CONTAINER (aligner), option_menu);
	gtk_table_attach (table, aligner,
			  VALUE_COLUMN, VALUE_COLUMN + 1,
			  row, row + 1,
			  GTK_FILL, 0,
			  0, 0);

	return GTK_OPTION_MENU (option_menu);
}		    	

static GtkOptionMenu*
attach_group_menu (GtkTable *table,
		   int row,
		   NautilusFile *file)
{
	GtkOptionMenu *option_menu;

	option_menu = attach_option_menu (table, row);

	synch_groups_menu (option_menu, file);

	/* Connect to signal to update menu when file changes. */
	g_signal_connect_object (file, "changed",
				 G_CALLBACK (synch_groups_menu),
				 option_menu, G_CONNECT_SWAPPED);	
	return option_menu;
}	

static void
owner_change_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	g_assert (callback_data == NULL);
	
	/* Report the error if it's an error. */
	eel_timed_wait_stop (cancel_owner_change_callback, file);
	fm_report_error_setting_owner (file, result, NULL);
	nautilus_file_unref (file);
}

static void
cancel_owner_change_callback (gpointer callback_data)
{
	NautilusFile *file;

	file = NAUTILUS_FILE (callback_data);
	nautilus_file_cancel (file, owner_change_callback, NULL);
	nautilus_file_unref (file);
}

static void
activate_owner_callback (GtkMenuItem *menu_item, FileNamePair *pair)
{
	g_assert (GTK_IS_MENU_ITEM (menu_item));
	g_assert (pair != NULL);
	g_assert (NAUTILUS_IS_FILE (pair->file));
	g_assert (pair->name != NULL);

	/* Try to change file owner. If this fails, complain to user. */
	nautilus_file_ref (pair->file);
	eel_timed_wait_start
		(cancel_owner_change_callback,
		 pair->file,
		 _("Cancel Owner Change?"),
		 _("Changing owner."),
		 NULL); /* FIXME bugzilla.gnome.org 42397: Parent this? */
	nautilus_file_set_owner
		(pair->file, pair->name,
		 owner_change_callback, NULL);
}

static GtkWidget *
create_owner_menu_item (NautilusFile *file, const char *user_name)
{
	GtkWidget *menu_item;
	char **name_array;
	char *label_text;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (user_name != NULL);

	name_array = g_strsplit (user_name, "\n", 2);
	if (name_array[1] != NULL) {
		label_text = g_strdup_printf ("%s - %s", name_array[0], name_array[1]);
	} else {
		label_text = g_strdup (name_array[0]);
	}

	menu_item = gtk_menu_item_new_with_label (label_text);
	g_free (label_text);

	gtk_widget_show (menu_item);

	eel_gtk_signal_connect_free_data_custom (GTK_OBJECT (menu_item),
						 "activate",
						 G_CALLBACK (activate_owner_callback),
						 file_name_pair_new (file, name_array[0]),
						 (GDestroyNotify)file_name_pair_free);
	g_strfreev (name_array);
	return menu_item;
}

static void
synch_user_menu (GtkOptionMenu *option_menu, NautilusFile *file)
{
	GList *users;
	GList *node;
	GtkWidget *new_menu;
	GtkWidget *menu_item;
	const char *user_name;
	char *owner_name;
	int user_index;
	int owner_index;

	g_assert (GTK_IS_OPTION_MENU (option_menu));
	g_assert (NAUTILUS_IS_FILE (file));

	owner_name = nautilus_file_get_string_attribute (file, "owner");
	owner_index = -1;

	users = nautilus_get_user_names ();
	new_menu = gtk_menu_new ();

	for (node = users, user_index = 0; node != NULL; node = node->next, ++user_index) {
		user_name = (const char *)node->data;
		if (strcmp (user_name, owner_name) == 0) {
			owner_index = user_index;
		}
		menu_item = create_owner_menu_item (file, user_name);
		gtk_menu_shell_append (GTK_MENU_SHELL (new_menu), menu_item);
	}

	/* If owner wasn't in list, we prepend it (with a separator). 
	 * This can happen if the owner is an id with no matching
	 * identifier in the passwords file.
	 */
	if (owner_index < 0 && owner_name != NULL) {
		if (users != NULL) {
			menu_item = gtk_menu_item_new ();
			gtk_widget_show (menu_item);
			gtk_menu_shell_prepend (GTK_MENU_SHELL (new_menu), menu_item);
		}
		menu_item = create_owner_menu_item (file, owner_name);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (new_menu), menu_item);
		owner_index = 0;
	}

        /* We create and attach a new menu here because adding/removing
         * items from existing menu screws up the size of the option menu.
         */
        gtk_option_menu_set_menu (option_menu, new_menu);

	gtk_option_menu_set_history (option_menu, owner_index);

	g_free (owner_name);
	eel_g_list_free_deep (users);
}	

static GtkOptionMenu*
attach_owner_menu (GtkTable *table,
		   int row,
		   NautilusFile *file)
{
	GtkOptionMenu *option_menu;

	option_menu = attach_option_menu (table, row);

	synch_user_menu (option_menu, file);

	/* Connect to signal to update menu when file changes. */
	g_signal_connect_object (file, "changed",
				 G_CALLBACK (synch_user_menu),
				 option_menu, G_CONNECT_SWAPPED);	
	return option_menu;
}

static guint
append_row (GtkTable *table)
{
	guint new_row_count;

	new_row_count = table->nrows + 1;

	gtk_table_resize (table, new_row_count, table->ncols);
	gtk_table_set_row_spacing (table, new_row_count - 1, GNOME_PAD);

	return new_row_count - 1;
}

static GtkWidget *
append_separator (GtkTable *table)
{
	GtkWidget *separator;
	guint last_row;

	last_row = append_row (table);
	separator = gtk_hseparator_new ();
	gtk_widget_show (separator);
	gtk_table_attach (table, separator,
			  TITLE_COLUMN, COLUMN_COUNT,
			  last_row, last_row+1,
			  GTK_FILL, 0,
			  0, 0);
	return separator;				   
}

static void
directory_contents_value_field_update (FMPropertiesWindow *window)
{
	NautilusRequestStatus file_status, status;
	char *text, *temp;
	guint directory_count;
	guint file_count;
	guint total_count;
	guint unreadable_directory_count;
	GnomeVFSFileSize total_size;
	gboolean used_two_lines;
	NautilusFile *file;
	GList *l;
	guint file_unreadable;
	GnomeVFSFileSize file_size;

	g_assert (FM_IS_PROPERTIES_WINDOW (window));

	status = NAUTILUS_REQUEST_DONE;
	file_status = NAUTILUS_REQUEST_NOT_STARTED;
	total_count = window->details->total_count;
	total_size = window->details->total_size;
	unreadable_directory_count = FALSE;

	for (l = window->details->target_files; l; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		if (nautilus_file_is_directory (file)) {
			file_status = nautilus_file_get_deep_counts (file, 
					 &directory_count,
					 &file_count, 
					 &file_unreadable,
					 &file_size);
			total_count += (file_count + directory_count);
			total_size += file_size;
			
			if (file_unreadable) {
				unreadable_directory_count = TRUE;
			}
			
			if (file_status != NAUTILUS_REQUEST_DONE) {
				status = file_status;
			}
		} else {
			++total_count;
			total_size += nautilus_file_get_size (file);
		}
	}
	
	/* If we've already displayed the total once, don't do another visible
	 * count-up if the deep_count happens to get invalidated.
	 * But still display the new total, since it might have changed.
	 */
	if (window->details->deep_count_finished &&
	    status != NAUTILUS_REQUEST_DONE) {
		return;
	}

	text = NULL;
	used_two_lines = FALSE;
	
	if (total_count == 0) {
		switch (status) {
		case NAUTILUS_REQUEST_DONE:
			if (unreadable_directory_count == 0) {
				text = g_strdup (_("nothing"));
			} else {
				text = g_strdup (_("unreadable"));
			}
			
			break;
		default:
			text = g_strdup ("...");
		}
	} else {
		char *size_str;
		size_str = gnome_vfs_format_file_size_for_display (total_size);
		text = g_strdup_printf (ngettext("%d item, with size %s",
						 "%d items, totalling %s",
						 total_count),
					total_count, size_str);
		g_free (size_str);

		if (unreadable_directory_count != 0) {
			temp = text;
			text = g_strconcat (temp, "\n",
					    _("(some contents unreadable)"),
					    NULL);
			g_free (temp);
			used_two_lines = TRUE;
		}
	}

	gtk_label_set_text (window->details->directory_contents_value_field,
			    text);
	g_free (text);

	/* Also set the title field here, with a trailing carriage return &
	 * space if the value field has two lines. This is a hack to get the
	 * "Contents:" title to line up with the first line of the
	 * 2-line value. Maybe there's a better way to do this, but I
	 * couldn't think of one.
	 */
	text = g_strdup (_("Contents:"));
	if (used_two_lines) {
		temp = text;
		text = g_strconcat (temp, "\n ", NULL);
		g_free (temp);
	}
	gtk_label_set_text (window->details->directory_contents_title_field,
			    text);
	g_free (text);

	if (status == NAUTILUS_REQUEST_DONE) {
		window->details->deep_count_finished = TRUE;
	}
}

static gboolean
update_directory_contents_callback (gpointer data)
{
	FMPropertiesWindow *window;

	window = FM_PROPERTIES_WINDOW (data);

	window->details->update_directory_contents_timeout_id = 0;
	directory_contents_value_field_update (window);

	return FALSE;
}

static void
schedule_directory_contents_update (FMPropertiesWindow *window)
{
	g_assert (FM_IS_PROPERTIES_WINDOW (window));

	if (window->details->update_directory_contents_timeout_id == 0) {
		window->details->update_directory_contents_timeout_id
			= g_timeout_add (DIRECTORY_CONTENTS_UPDATE_INTERVAL,
					 update_directory_contents_callback,
					 window);
	}
}

static GtkLabel *
attach_directory_contents_value_field (FMPropertiesWindow *window,
				       GtkTable *table,
				       int row)
{
	GtkLabel *value_field;
	GList *l;
	NautilusFile *file;

	value_field = attach_value_label (table, row, VALUE_COLUMN, "");

	g_assert (window->details->directory_contents_value_field == NULL);
	window->details->directory_contents_value_field = value_field;

	gtk_label_set_line_wrap (value_field, TRUE);
	
	/* Fill in the initial value. */
	directory_contents_value_field_update (window);
 
	       
	for (l = window->details->original_files; l; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		nautilus_file_recompute_deep_counts (file);
		
		g_signal_connect_object (file,
					 "updated_deep_count_in_progress",
					 G_CALLBACK (schedule_directory_contents_update),
					 window, G_CONNECT_SWAPPED);
	}
	
	return value_field;	
}

static GtkLabel *
attach_title_field (GtkTable *table,
		     int row,
		     const char *title)
{
	return attach_label (table, row, TITLE_COLUMN, title, TRUE, TRUE, FALSE, FALSE, TRUE);
}		      

static guint
append_title_field (GtkTable *table, const char *title, GtkLabel **label)
{
	guint last_row;
	GtkLabel *title_label;

	last_row = append_row (table);
	title_label = attach_title_field (table, last_row, title);

	if (label) {
		*label = title_label;
	}

	return last_row;
}

static guint
append_title_value_pair (FMPropertiesWindow *window,
			 GtkTable *table,
			 const char *title, 
 			 const char *file_attribute_name,
			 const char *inconsistent_state,
			 gboolean show_original)
{
	guint last_row;

	last_row = append_title_field (table, title, NULL);
	attach_value_field (window, table, last_row, VALUE_COLUMN, 
			    file_attribute_name,
			    inconsistent_state,
			    show_original); 

	return last_row;
}

static guint
append_title_and_ellipsizing_value (FMPropertiesWindow *window,
				    GtkTable *table,
				    const char *title,
				    const char *file_attribute_name,
				    const char *inconsistent_state,
				    gboolean show_original)
{
	guint last_row;

	last_row = append_title_field (table, title, NULL);
	attach_ellipsizing_value_field (window, table, last_row, VALUE_COLUMN, 
					file_attribute_name,
					inconsistent_state,
					show_original);

	return last_row;
}

static void
update_visibility_of_table_rows (GtkTable *table,
		   	 	 gboolean should_show,
		   		 int first_row, 
		   		 int row_count,
		   		 GList *widgets)
{
	GList *node;
	int i;

	for (node = widgets; node != NULL; node = node->next) {
		if (should_show) {
			gtk_widget_show (GTK_WIDGET (node->data));
		} else {
			gtk_widget_hide (GTK_WIDGET (node->data));
		}
	}

	for (i= 0; i < row_count; ++i) {
		gtk_table_set_row_spacing (table, first_row + i, should_show ? GNOME_PAD : 0);
	}
}				   

static void
update_visibility_of_item_count_fields (FMPropertiesWindow *window)
{
	gboolean should_show_count;
	GList *l;
	guint count = 0;
	NautilusFile *file;
               
	for (l = window->details->target_files; l; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		count += nautilus_file_should_show_directory_item_count (file);
	}
	should_show_count = count;

	update_visibility_of_table_rows
		(window->details->basic_table,
		 should_show_count,
		 window->details->directory_contents_row,
		 1,
		 window->details->directory_contents_widgets);
}

static void
update_visibility_of_item_count_fields_wrapper (gpointer callback_data)
{
	update_visibility_of_item_count_fields (FM_PROPERTIES_WINDOW (callback_data));
}  

static void
remember_directory_contents_widget (FMPropertiesWindow *window, GtkWidget *widget)
{
	g_assert (FM_IS_PROPERTIES_WINDOW (window));
	g_assert (GTK_IS_WIDGET (widget));
	
	window->details->directory_contents_widgets = 
		g_list_prepend (window->details->directory_contents_widgets, widget);
}

static guint
append_directory_contents_fields (FMPropertiesWindow *window,
				  GtkTable *table)
{
	GtkLabel *title_field, *value_field;
	guint last_row;

	last_row = append_row (table);

	title_field = attach_title_field (table, last_row, "");
	window->details->directory_contents_title_field = title_field;
	gtk_label_set_line_wrap (title_field, TRUE);

	value_field = attach_directory_contents_value_field 
		(window, table, last_row);

	remember_directory_contents_widget (window, GTK_WIDGET (title_field));
	remember_directory_contents_widget (window, GTK_WIDGET (value_field));
	window->details->directory_contents_row = last_row;

	update_visibility_of_item_count_fields (window);
	eel_preferences_add_callback_while_alive 
		(NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
		 update_visibility_of_item_count_fields_wrapper,
		 window,
		 G_OBJECT (window));
	
	return last_row;
}

static GtkWidget *
create_page_with_vbox (GtkNotebook *notebook,
		       const char *title)
{
	GtkWidget *vbox;

	g_assert (GTK_IS_NOTEBOOK (notebook));
	g_assert (title != NULL);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD);
	gtk_notebook_append_page (notebook, vbox, gtk_label_new (title));

	return vbox;
}		       

static void
apply_standard_table_padding (GtkTable *table)
{
	gtk_table_set_row_spacings (table, GNOME_PAD);
	gtk_table_set_col_spacings (table, GNOME_PAD);	
}

static GtkWidget *
create_attribute_value_table (GtkVBox *vbox, int row_count)
{
	GtkWidget *table;

	table = gtk_table_new (row_count, COLUMN_COUNT, FALSE);
	apply_standard_table_padding (GTK_TABLE (table));
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);	

	return table;
}

static void
create_page_with_table_in_vbox (GtkNotebook *notebook, 
				const char *title, 
				int row_count, 
				GtkTable **return_table, 
				GtkWidget **return_vbox)
{
	GtkWidget *table;
	GtkWidget *vbox;

	vbox = create_page_with_vbox (notebook, title);
	table = create_attribute_value_table (GTK_VBOX (vbox), row_count);

	if (return_table != NULL) {
		*return_table = GTK_TABLE (table);
	}

	if (return_vbox != NULL) {
		*return_vbox = vbox;
	}
}		

static gboolean
is_merged_trash_directory (NautilusFile *file) 
{
	char *file_uri;
	gboolean result;

	file_uri = nautilus_file_get_uri (file);
	result = eel_uris_match (file_uri, EEL_TRASH_URI);
	g_free (file_uri);

	return result;
}

static gboolean
should_show_custom_icon_buttons (FMPropertiesWindow *window) 
{
	/* FIXME bugzilla.gnome.org 45642:
	 * Custom icons aren't displayed on the the desktop Trash icon, so
	 * we shouldn't pretend that they work by showing them here.
	 * When bug 5642 is fixed we can remove this case.
	 */
	if (!is_multi_file_window (window) 
	    && is_merged_trash_directory (get_target_file (window))) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_file_type (FMPropertiesWindow *window) 
{
	/* The trash on the desktop is one-of-a-kind */
	if (!is_multi_file_window (window) 
	    && is_merged_trash_directory (get_target_file (window))) {
		return FALSE;
	}


	return TRUE;
}

static gboolean
should_show_accessed_date (FMPropertiesWindow *window) 
{
	/* Accessed date for directory seems useless. If we some
	 * day decide that it is useful, we should separately
	 * consider whether it's useful for "trash:".
	 */
	if (file_list_all_directories (window->details->target_files)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_mime_type (FMPropertiesWindow *window) 
{
	/* FIXME bugzilla.gnome.org 45652:
	 * nautilus_file_is_directory should return TRUE for special
	 * trash directory, but doesn't. I could trivially fix this
	 * with a check for is_merged_trash_directory here instead.
	 */
	if (file_list_all_directories (window->details->target_files)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_link_target (FMPropertiesWindow *window)
{
	if (!is_multi_file_window (window)
	    && nautilus_file_is_symbolic_link (get_target_file (window))) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
should_show_free_space (FMPropertiesWindow *window)
{
	if (file_list_all_local (window->details->target_files)
	    && file_list_all_directories (window->details->target_files)) {
		return TRUE;
	}

	return FALSE;
}

static void
create_basic_page (FMPropertiesWindow *window)
{
	GtkTable *table;
	GtkWidget *container;
	GtkWidget *name_field;
	GtkWidget *icon_aligner;
	GtkWidget *icon_pixmap_widget;

	GtkWidget *hbox, *name_label;

	create_page_with_table_in_vbox (window->details->notebook, 
					_("Basic"), 
					1,
					&table, 
					&container);
	window->details->basic_table = table;
	
	/* Icon pixmap */
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);
	gtk_table_attach (table,
			  hbox,
			  TITLE_COLUMN, 
			  TITLE_COLUMN + 1,
			  0, 1,
			  0, 0,
			  0, 0);

	icon_pixmap_widget = create_image_widget (window);
	gtk_widget_show (icon_pixmap_widget);
	
	icon_aligner = gtk_alignment_new (1, 0.5, 0, 0);
	gtk_widget_show (icon_aligner);
	
	gtk_container_add (GTK_CONTAINER (icon_aligner), icon_pixmap_widget);
	gtk_box_pack_start (GTK_BOX (hbox), icon_aligner, TRUE, TRUE, 0);

	/* Name label */
	if (is_multi_file_window (window)) {
		name_label = gtk_label_new_with_mnemonic (_("_Names:"));
	} else {
		name_label = gtk_label_new_with_mnemonic (_("_Name:"));
	}
	eel_gtk_label_make_bold (GTK_LABEL (name_label));
	gtk_widget_show (name_label);
	gtk_box_pack_end (GTK_BOX (hbox), name_label, FALSE, FALSE, 0);

	/* Name field */
	name_field = nautilus_entry_new ();
	window->details->name_field = NAUTILUS_ENTRY (name_field);
	gtk_widget_show (name_field);
	gtk_table_attach (table,
			  name_field,
			  VALUE_COLUMN, 
			  VALUE_COLUMN + 1,
			  0, 1,
			  GTK_FILL, 0,
			  0, 0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (name_label), name_field);

	/* Update name field initially before hooking up changed signal. */
	update_name_field (window);

/* FIXME bugzilla.gnome.org 42151:
 * With this (and one place elsewhere in this file, not sure which is the
 * trouble-causer) code in place, bug 2151 happens (crash on quit). Since
 * we've removed Undo from Nautilus for now, I'm just ifdeffing out this
 * code rather than trying to fix 2151 now. Note that it might be possible
 * to fix 2151 without making Undo actually work, it's just not worth the
 * trouble.
 */
#ifdef UNDO_ENABLED
	/* Set up name field for undo */
	nautilus_undo_set_up_nautilus_entry_for_undo ( NAUTILUS_ENTRY (name_field));
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (name_field), TRUE);
#endif

	g_signal_connect_object (name_field, "focus_out_event",
				 G_CALLBACK (name_field_focus_out), window, 0);                      			    
	g_signal_connect_object (name_field, "activate",
				 G_CALLBACK (name_field_activate), window, 0);

        /* Start with name field selected, if it's sensitive. */
        if (GTK_WIDGET_SENSITIVE (name_field)) {
		nautilus_entry_select_all (NAUTILUS_ENTRY (name_field));
	        gtk_widget_grab_focus (GTK_WIDGET (name_field));
        }
        
	if (should_show_file_type (window)) {
		append_title_value_pair (window,
					 table, _("Type:"), 
					 "type",
					 _("--"),
					 FALSE);
	}

	if (is_multi_file_window (window) ||
	    nautilus_file_is_directory (get_target_file (window))) {
		append_directory_contents_fields (window, table);
	} else {
		append_title_value_pair (window, table, _("Size:"), 
					 "size",
					 _("--"),
					 FALSE);
	}

	append_title_and_ellipsizing_value (window, table, _("Location:"), 
					    "where",
					    _("--"),
					    FALSE);
	
	if (should_show_free_space (window)) {
		append_title_and_ellipsizing_value (window, table, 
						    _("Volume:"), 
						    "volume",
						    _("--"),
						    FALSE);
		append_title_value_pair (window, table, _("Free space:"), 
					 "free_space",
					 _("--"),
					 FALSE);
	}

	if (should_show_link_target (window)) {
		append_title_and_ellipsizing_value (window, table, 
						    _("Link target:"), 
						    "link_target",
						    _("--"),
						    FALSE);
	}
	if (should_show_mime_type (window)) {
		append_title_value_pair (window, table, _("MIME type:"), 
					 "mime_type",
					 _("--"),
					 FALSE);
	}				  
	
	/* Blank title ensures standard row height */
	append_title_field (table, "", NULL);
	
	append_title_value_pair (window, table, _("Modified:"), 
				 "date_modified",
				 _("--"),
				 FALSE);
	
	if (should_show_accessed_date (window)) {
		append_title_value_pair (window, table, _("Accessed:"), 
					 "date_accessed",
					 _("--"),
					 FALSE);
	}

	if (should_show_custom_icon_buttons (window)) {
		GtkWidget *button_box;
		GtkWidget *temp_button;
		GList *l;
		
		/* add command buttons for setting and clearing custom icons */
		button_box = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (button_box);
		gtk_box_pack_end (GTK_BOX(container), button_box, FALSE, FALSE, 4);  
		
	 	temp_button = gtk_button_new_with_mnemonic (_("_Select Custom Icon..."));
		gtk_widget_show (temp_button);
		gtk_box_pack_start (GTK_BOX (button_box), temp_button, FALSE, FALSE, 4);  

		g_signal_connect_object (temp_button, "clicked", G_CALLBACK (select_image_button_callback), window, 0);
	 	
	 	temp_button = gtk_button_new_with_mnemonic (_("_Remove Custom Icon"));
		gtk_widget_show (temp_button);
		gtk_box_pack_start (GTK_BOX(button_box), temp_button, FALSE, FALSE, 4);  

	 	g_signal_connect_object (temp_button, "clicked", G_CALLBACK (remove_image_button_callback), window, 0);

		window->details->remove_image_button = temp_button;
		
		/* de-sensitize the remove button if there isn't a custom image */
		
		gtk_widget_set_sensitive (temp_button, FALSE);
		for (l = window->details->original_files; l != NULL; l = l->next) {
			char *image_uri = nautilus_file_get_metadata 
				(NAUTILUS_FILE (l->data), 
				 NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);
			if (image_uri) {
				gtk_widget_set_sensitive (temp_button, TRUE);
			}
			
			g_free (image_uri);
		}

		window->details->icon_selector_window = NULL;
	}
}

static GHashTable *
get_initial_emblems (GList *files)
{
	GHashTable *ret;
	GList *l;
	
	ret = g_hash_table_new_full (g_direct_hash, 
				     g_direct_equal,
				     NULL,
				     (GDestroyNotify)eel_g_list_free_deep);

	for (l = files; l != NULL; l = l->next) {
		NautilusFile *file;
		GList *keywords;
		
		file = NAUTILUS_FILE (l->data);

		keywords = nautilus_file_get_keywords (file);
		g_hash_table_insert (ret, file, keywords);
	}

	return ret;
}

static void
create_emblems_page (FMPropertiesWindow *window)
{
	GtkWidget *emblems_table, *button, *scroller;
	char *emblem_name;
	GdkPixbuf *pixbuf;
	char *label;
	GList *icons, *l;

	/* The emblems wrapped table */
	scroller = eel_scrolled_wrap_table_new (TRUE, &emblems_table);

	gtk_container_set_border_width (GTK_CONTAINER (emblems_table), GNOME_PAD);
	
	gtk_widget_show (scroller);

	gtk_notebook_append_page (window->details->notebook, 
				  scroller, gtk_label_new (_("Emblems")));

	icons = nautilus_emblem_list_availible ();

	window->details->initial_emblems = get_initial_emblems (window->details->original_files);

	l = icons;
	while (l != NULL) {
		emblem_name = l->data;
		l = l->next;
		
		if (!nautilus_emblem_should_show_in_list (emblem_name)) {
			continue;
		}
		
		pixbuf = nautilus_icon_factory_get_pixbuf_from_name (emblem_name, NULL,
								     NAUTILUS_ICON_SIZE_SMALL,
								     &label);

		if (pixbuf == NULL) {
			continue;
		}

		if (label == NULL) {
			label = nautilus_emblem_get_keyword_from_icon_name (emblem_name);
		}
		
		button = eel_labeled_image_check_button_new (label, pixbuf);
		eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (GTK_BIN (button)->child), STANDARD_EMBLEM_HEIGHT);
		eel_labeled_image_set_spacing (EEL_LABELED_IMAGE (GTK_BIN (button)->child), EMBLEM_LABEL_SPACING);
		
		g_free (label);
		g_object_unref (pixbuf);

		/* Attach parameters and signal handler. */
		g_object_set_data_full (G_OBJECT (button), "nautilus_emblem_name",
					nautilus_emblem_get_keyword_from_icon_name (emblem_name), g_free);
				     
		window->details->emblem_buttons = 
			g_list_append (window->details->emblem_buttons,
				       button);

		g_signal_connect_object (button, "toggled",
					 G_CALLBACK (emblem_button_toggled), 
					 G_OBJECT (window),
					 0);

		gtk_container_add (GTK_CONTAINER (emblems_table), button);
	}
	eel_g_list_free_deep (icons);
	gtk_widget_show_all (emblems_table);
}

static void
permission_change_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	FMPropertiesWindow *window;
	g_assert (callback_data != NULL);

	window = FM_PROPERTIES_WINDOW (callback_data);
	if (GTK_WIDGET (window)->window != NULL &&
	    window->details->long_operation_underway == 1) {
		/* finished !! */
		gdk_window_set_cursor (GTK_WIDGET (window)->window, NULL);
	}
	window->details->long_operation_underway--;
	
	/* Report the error if it's an error. */
	fm_report_error_setting_permissions (file, result, NULL);

	g_object_unref (window);
}

static void
get_initial_permission_state (FMPropertiesWindow *window,
			      GnomeVFSFilePermissions mask,
			      GList **on,
			      GList **off)
{
	GList *l;
	
	*on = NULL;
	*off = NULL;
	
	for (l = window->details->target_files; l != NULL; l = l->next) {
		GnomeVFSFilePermissions permissions;
		
		permissions = GPOINTER_TO_INT (g_hash_table_lookup (window->details->initial_permissions,
								    l->data));
					       
		if ((permissions & mask) != 0) {
			*on = g_list_prepend (*on, l->data);
		} else {
			*off = g_list_prepend (*off, l->data);
		}
	}
}

static void
permission_button_toggled (GtkToggleButton *button, 
			   FMPropertiesWindow *window)
{
	GList *l;
	GnomeVFSFilePermissions permission_mask;
	GList *files_on;
	GList *files_off;
	
	permission_mask = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
							      "permission"));

	files_on = NULL;
	files_off = NULL;
	if (gtk_toggle_button_get_active (button)
	    && !gtk_toggle_button_get_inconsistent (button)) {
		/* Go to the initial state unless the initial state was 
		   consistent */
		get_initial_permission_state (window, permission_mask,
					      &files_on, &files_off);
		
		if (!(files_on && files_off)) {
			g_list_free (files_on);
			g_list_free (files_off);
			files_on = g_list_copy (window->details->target_files);
			files_off = NULL;
		}
	} else if (gtk_toggle_button_get_inconsistent (button)
		   && !gtk_toggle_button_get_active (button)) {
		files_on = g_list_copy (window->details->target_files);
		files_off = NULL;
	} else {
		files_off = g_list_copy (window->details->target_files);
		files_on = NULL;
	}
	
	g_signal_handlers_block_by_func (G_OBJECT (button), 
					 G_CALLBACK (permission_button_toggled),
					 window);
	
	gtk_toggle_button_set_active (button, files_on != NULL);
	gtk_toggle_button_set_inconsistent (button, files_on && files_off);

	g_signal_handlers_unblock_by_func (G_OBJECT (button), 
					   G_CALLBACK (permission_button_toggled),
					   window);

	if (window->details->long_operation_underway == 0) {
		/* start long operation */
		GdkCursor * cursor;
	       
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (window)->window, cursor);
		gdk_cursor_unref (cursor);
	}
	window->details->long_operation_underway += g_list_length (files_on);
	window->details->long_operation_underway += g_list_length (files_off);

	for (l = files_on; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);

		if (nautilus_file_can_set_permissions (file)) {
			GnomeVFSFilePermissions permissions;

			permissions = nautilus_file_get_permissions (file);
			permissions |= permission_mask;
			
			g_object_ref (window);
			nautilus_file_set_permissions
				(file, permissions,
				 permission_change_callback,
				 window);
		}
		
	}
	
	for (l = files_off; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);

		if (nautilus_file_can_set_permissions (file)) {
			GnomeVFSFilePermissions permissions;

			permissions = nautilus_file_get_permissions (file);
			permissions &= ~permission_mask;

			g_object_ref (window);
			nautilus_file_set_permissions
				(file, permissions,
				 permission_change_callback,
				 window);
		}
	}	

	g_list_free (files_on);
	g_list_free (files_off);
}

static void
permission_button_update (FMPropertiesWindow *window,
			  GtkToggleButton *button)
{
	GList *l;
	gboolean all_set;
	gboolean all_unset;
	gboolean all_cannot_set;
	GnomeVFSFilePermissions button_permission;
	
	button_permission = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
								"permission"));
	
	all_set = TRUE;
	all_unset = TRUE;
	all_cannot_set = TRUE;
	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		GnomeVFSFilePermissions file_permissions;

		file = NAUTILUS_FILE (l->data);

		if (!nautilus_file_can_get_permissions (file)) {
			continue;
		}

		file_permissions = nautilus_file_get_permissions (file);

		if ((file_permissions & button_permission) != 0) {
			all_unset = FALSE;
		} else {
			all_set = FALSE;
		}

		if (nautilus_file_can_set_permissions (file)) {
			all_cannot_set = FALSE;
		}
	}
	
	g_signal_handlers_block_by_func (G_OBJECT (button), 
					 G_CALLBACK (permission_button_toggled),
					 window);

	gtk_toggle_button_set_active (button, !all_unset);
	gtk_toggle_button_set_inconsistent (button, !all_unset && !all_set);
	gtk_widget_set_sensitive (GTK_WIDGET (button), !all_cannot_set);

	g_signal_handlers_unblock_by_func (G_OBJECT (button), 
					   G_CALLBACK (permission_button_toggled),
					   window);
}


static void
set_up_permissions_checkbox (FMPropertiesWindow *window,
			     GtkWidget *check_button, 
			     GnomeVFSFilePermissions permission)
{
	/* Load up the check_button with data we'll need when updating its state. */
        g_object_set_data (G_OBJECT (check_button), "permission", 
			   GINT_TO_POINTER (permission));
        g_object_set_data (G_OBJECT (check_button), "properties_window", 
			   window);
	
	window->details->permission_buttons = 
		g_list_prepend (window->details->permission_buttons,
				check_button);

	g_signal_connect_object (check_button, "toggled",
				 G_CALLBACK (permission_button_toggled),
				 window,
				 0);
}

static void
add_permissions_checkbox (FMPropertiesWindow *window,
			  GtkTable *table, 
			  int row, int column, 
			  GnomeVFSFilePermissions permission_to_check,
			  GtkLabel *label_for)
{
	GtkWidget *check_button;
	gchar *label;
	gboolean a11y_enabled;

	if (column == PERMISSIONS_CHECKBOXES_READ_COLUMN) {
		label = _("_Read");
	} else if (column == PERMISSIONS_CHECKBOXES_WRITE_COLUMN) {
		label = _("_Write");
	} else {
		label = _("E_xecute");
	}

	check_button = gtk_check_button_new_with_mnemonic (label);
	gtk_widget_show (check_button);
	gtk_table_attach (table, check_button,
			  column, column + 1,
			  row, row + 1,
			  GTK_FILL, 0,
			  0, 0);

	set_up_permissions_checkbox (window, 
				     check_button, 
				     permission_to_check);

	a11y_enabled = GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (check_button));
	if (a11y_enabled) {
		eel_accessibility_set_up_label_widget_relation (GTK_WIDGET (label_for),
								check_button);
	}
}

static GtkWidget *
append_special_execution_checkbox (FMPropertiesWindow *window,
				   GtkTable *table,
				   const char *label_text,
				   GnomeVFSFilePermissions permission_to_check)
{
	GtkWidget *check_button;
	guint last_row;

	last_row = append_row (table);

	check_button = gtk_check_button_new_with_mnemonic (label_text);
	gtk_widget_show (check_button);

	gtk_table_attach (table, check_button,
			  VALUE_COLUMN, VALUE_COLUMN + 1,
			  last_row, last_row + 1,
			  GTK_FILL, 0,
			  0, 0);

	set_up_permissions_checkbox (window, 
				     check_button, 
				     permission_to_check);
	++window->details->num_special_flags_rows;

	return check_button;
}

static void
remember_special_flags_widget (FMPropertiesWindow *window, GtkWidget *widget)
{
	g_assert (FM_IS_PROPERTIES_WINDOW (window));
	g_assert (GTK_IS_WIDGET (widget));
	
	window->details->special_flags_widgets = 
		g_list_prepend (window->details->special_flags_widgets, widget);
}

static void
update_visibility_of_special_flags_widgets (FMPropertiesWindow *window)
{
	update_visibility_of_table_rows 
		(window->details->permissions_table,
		 eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS),
		 window->details->first_special_flags_row,
		 window->details->num_special_flags_rows,
		 window->details->special_flags_widgets);
}

static void
update_visibility_of_special_flags_widgets_wrapper (gpointer callback_data)
{
	update_visibility_of_special_flags_widgets (FM_PROPERTIES_WINDOW (callback_data));
}

static void
append_special_execution_flags (FMPropertiesWindow *window, GtkTable *table)
{
	

	remember_special_flags_widget (window, append_special_execution_checkbox 
		(window, table, _("Set _user ID"), GNOME_VFS_PERM_SUID));

	window->details->first_special_flags_row = table->nrows - 1;

	remember_special_flags_widget (window, GTK_WIDGET (attach_title_field 
		(table, table->nrows - 1, _("Special flags:"))));

	remember_special_flags_widget (window, append_special_execution_checkbox 
		(window, table, _("Set gro_up ID"), GNOME_VFS_PERM_SGID));
	remember_special_flags_widget (window, append_special_execution_checkbox 
		(window, table, _("_Sticky"), GNOME_VFS_PERM_STICKY));

	remember_special_flags_widget (window, append_separator (table));
	++window->details->num_special_flags_rows;

	update_visibility_of_special_flags_widgets (window);
	eel_preferences_add_callback_while_alive 
		(NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
		 update_visibility_of_special_flags_widgets_wrapper,
		 window,
		 G_OBJECT (window));
	
}

static gboolean
all_can_get_permissions (GList *file_list)
{
	GList *l;
	for (l = file_list; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		if (!nautilus_file_can_get_permissions (file)) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
all_can_set_permissions (GList *file_list)
{
	GList *l;
	for (l = file_list; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);

		if (!nautilus_file_can_set_permissions (file)) {
			return FALSE;
		}
	}

	return TRUE;
}

static GHashTable *
get_initial_permissions (GList *file_list)
{
	GHashTable *ret;
	GList *l;

	ret = g_hash_table_new (g_direct_hash,
				g_direct_equal);
	
	for (l = file_list; l != NULL; l = l->next) {
		GnomeVFSFilePermissions permissions;
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		permissions = nautilus_file_get_permissions (file);
		g_hash_table_insert (ret, file,
				     GINT_TO_POINTER (permissions));
	}

	return ret;
}

static void
create_permissions_page (FMPropertiesWindow *window)
{
	GtkWidget *vbox;
	GtkTable *page_table, *check_button_table;
	char *file_name, *prompt_text;
	guint last_row;
	guint checkbox_titles_row;
	GtkLabel *group_label;
	GtkLabel *owner_label;
	GtkLabel *owner_perm_label;
	GtkLabel *group_perm_label;
	GtkLabel *other_perm_label;
	GtkOptionMenu *group_menu;
	GtkOptionMenu *owner_menu;
	GList *file_list;

	vbox = create_page_with_vbox (window->details->notebook,
				      _("Permissions"));

	file_list = window->details->original_files;

	window->details->initial_permissions = get_initial_permissions (window->details->target_files);
	
	if (all_can_get_permissions (file_list)) {
		if (!all_can_set_permissions (file_list)) {
			add_prompt_and_separator (
				GTK_VBOX (vbox), 
				_("You are not the owner, so you can't change these permissions."));
		}

		page_table = GTK_TABLE (gtk_table_new (1, COLUMN_COUNT, FALSE));
		window->details->permissions_table = page_table;

		apply_standard_table_padding (page_table);
		last_row = 0;
		gtk_widget_show (GTK_WIDGET (page_table));
		gtk_box_pack_start (GTK_BOX (vbox), 
				    GTK_WIDGET (page_table), 
				    TRUE, TRUE, 0);

		if (!is_multi_file_window (window) && nautilus_file_can_set_owner (get_target_file (window))) {
			owner_label = attach_title_field (page_table, last_row, _("File _owner:"));
			/* Option menu in this case. */
			owner_menu = attach_owner_menu (page_table, last_row, get_target_file (window));
			gtk_label_set_mnemonic_widget (owner_label,
						       GTK_WIDGET (owner_menu));
		} else {
			attach_title_field (page_table, last_row, _("File owner:"));
			/* Static text in this case. */
			attach_value_field (window, 
					    page_table, last_row, VALUE_COLUMN,
					    "owner",
					    _("--"),
					    FALSE); 
		}

		if (!is_multi_file_window (window) && nautilus_file_can_set_group (get_target_file (window))) {
			last_row = append_title_field (page_table,
						       _("_File group:"),
						       &group_label);
			/* Option menu in this case. */
			group_menu = attach_group_menu (page_table, last_row,
							get_target_file (window));
			gtk_label_set_mnemonic_widget (group_label,
						       GTK_WIDGET (group_menu));
		} else {
			last_row = append_title_field (page_table,
						       _("File group:"),
						       NULL);
			/* Static text in this case. */
			attach_value_field (window, page_table, last_row, 
					    VALUE_COLUMN, 
					    "group",
					    _("--"),
					    FALSE); 
		}

		append_separator (page_table);
		
		checkbox_titles_row = append_title_field (page_table, _("Owner:"), &owner_perm_label);
		append_title_field (page_table, _("Group:"), &group_perm_label);
		append_title_field (page_table, _("Others:"), &other_perm_label);
		
		check_button_table = GTK_TABLE (gtk_table_new 
						   (PERMISSIONS_CHECKBOXES_ROW_COUNT, 
						    PERMISSIONS_CHECKBOXES_COLUMN_COUNT, 
						    FALSE));
		apply_standard_table_padding (check_button_table);
		gtk_widget_show (GTK_WIDGET (check_button_table));
		gtk_table_attach (page_table, GTK_WIDGET (check_button_table),
				  VALUE_COLUMN, VALUE_COLUMN + 1, 
				  checkbox_titles_row, checkbox_titles_row + PERMISSIONS_CHECKBOXES_ROW_COUNT,
				  0, 0,
				  0, 0);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_USER_READ,
					  owner_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_USER_WRITE,
					  owner_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_USER_EXEC,
					  owner_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_GROUP_READ,
					  group_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_GROUP_WRITE,
					  group_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_GROUP_EXEC,
					  group_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_OTHER_READ,
					  other_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_OTHER_WRITE,
					  other_perm_label);

		add_permissions_checkbox (window,
					  check_button_table, 
					  PERMISSIONS_CHECKBOXES_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_OTHER_EXEC,
					  other_perm_label);

		append_separator (page_table);

		append_special_execution_flags (window, page_table);
		
		append_title_value_pair
			(window, page_table, _("Text view:"), 
			 "permissions", _("--"),
			 FALSE);
		append_title_value_pair 
			(window, page_table, _("Number view:"), 
			 "octal_permissions", _("--"),
			 FALSE);
		append_title_value_pair
			(window, page_table, _("Last changed:"), 
			 "date_permissions", _("--"),
			 FALSE);
	} else {
		if (!is_multi_file_window (window)) {
			file_name = nautilus_file_get_display_name (get_target_file (window));
			prompt_text = g_strdup_printf (_("The permissions of \"%s\" could not be determined."), file_name);
			g_free (file_name);
		} else {
			prompt_text = g_strdup (_("The permissions of the selected file could not be determined."));
		}
		
		add_prompt (GTK_VBOX (vbox), prompt_text, TRUE);
		g_free (prompt_text);
	}
}

static void
append_extension_pages (FMPropertiesWindow *window)
{
	GList *providers;
	GList *p;
	
 	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER);
	
	for (p = providers; p != NULL; p = p->next) {
		NautilusPropertyPageProvider *provider;
		GList *pages;
		GList *l;

		provider = NAUTILUS_PROPERTY_PAGE_PROVIDER (p->data);
		
		pages = nautilus_property_page_provider_get_pages 
			(provider, window->details->target_files);
		
		for (l = pages; l != NULL; l = l->next) {
			NautilusPropertyPage *page;
			GtkWidget *page_widget;
			GtkWidget *label;
			
			page = NAUTILUS_PROPERTY_PAGE (l->data);

			g_object_get (G_OBJECT (page), 
				      "page", &page_widget, "label", &label, 
				      NULL);
			
			gtk_notebook_append_page (window->details->notebook, 
						  page_widget, label);

			g_object_set_data (G_OBJECT (page_widget), 
					   "is-extension-page",
					   page);

			gtk_widget_unref (page_widget);
			gtk_widget_unref (label);

			g_object_unref (page);
		}

		g_list_free (pages);
	}

	nautilus_module_extension_list_free (providers);
}

static gboolean
should_show_emblems (FMPropertiesWindow *window) 
{
	/* FIXME bugzilla.gnome.org 45643:
	 * Emblems aren't displayed on the the desktop Trash icon, so
	 * we shouldn't pretend that they work by showing them here.
	 * When bug 5643 is fixed we can remove this case.
	 */
	if (!is_multi_file_window (window) 
	    && is_merged_trash_directory (get_target_file (window))) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_permissions (FMPropertiesWindow *window) 
{
	/* Don't show permissions for the Trash since it's not
	 * really a file system object.
	 */
	if (!is_multi_file_window (window)
	    && is_merged_trash_directory (get_target_file (window))) {
		return FALSE;
	}

	return TRUE;
}

static char *
get_pending_key (GList *file_list)
{
	GList *l;
	GList *uris;
	GString *key;
	char *ret;
	
	uris = NULL;
	for (l = file_list; l != NULL; l = l->next) {
		uris = g_list_prepend (uris, nautilus_file_get_uri (NAUTILUS_FILE (l->data)));
	}
	uris = g_list_sort (uris, (GCompareFunc)strcmp);

	key = g_string_new ("");
	for (l = uris; l != NULL; l = l->next) {
		g_string_append (key, l->data);
		g_string_append (key, ";");
	}

	eel_g_list_free_deep (uris);

	ret = key->str;
	g_string_free (key, FALSE);

	return ret;
}

static StartupData *
startup_data_new (GList *original_files, 
		  GList *target_files,
		  const char *pending_key,
		  GtkWidget *parent_widget)
{
	StartupData *data;
	GList *l;

	data = g_new0 (StartupData, 1);
	data->original_files = nautilus_file_list_copy (original_files);
	data->target_files = nautilus_file_list_copy (target_files);
	data->parent_widget = parent_widget;
	data->pending_key = g_strdup (pending_key);
	data->pending_files = g_hash_table_new (g_direct_hash,
						g_direct_equal);

	for (l = data->target_files; l != NULL; l = l->next) {
		g_hash_table_insert (data->pending_files, l->data, l->data);
	}

	return data;
}

static void
startup_data_free (StartupData *data)
{
	nautilus_file_list_free (data->original_files);
	nautilus_file_list_free (data->target_files);
	g_hash_table_destroy (data->pending_files);
	g_free (data->pending_key);
	g_free (data);
}

static void
help_button_callback (GtkWidget *widget, GtkWidget *property_window)
{
	GError *error = NULL;

	gnome_help_display_desktop_on_screen (NULL, "user-guide", "user-guide.xml", "gosnautilus-51",
					      gtk_window_get_screen (GTK_WINDOW (property_window)),
&error);

	if (error) {
		eel_show_error_dialog (_("There was an error displaying help."), error->message, _("Couldn't Show Help"),
				       GTK_WINDOW (property_window));
		g_error_free (error);
	}
}

static void
file_changed_callback (NautilusFile *file, gpointer user_data)
{
	FMPropertiesWindow *window = FM_PROPERTIES_WINDOW (user_data);

	if (!g_list_find (window->details->changed_files, file)) {
		nautilus_file_ref (file);
		window->details->changed_files = g_list_prepend (window->details->changed_files, file);
		
		schedule_files_update (window);
	}
}

static gboolean
should_show_open_with (FMPropertiesWindow *window)
{
	return !is_multi_file_window (window);
}

static void
create_open_with_page (FMPropertiesWindow *window)
{
	GtkWidget *vbox;
	char *uri;
	char *mime_type;
	
	uri = nautilus_file_get_uri (get_target_file (window));
	mime_type = nautilus_file_get_mime_type (get_target_file (window));
	
	vbox = eel_mime_application_chooser_new (uri, mime_type);
	gtk_widget_show (vbox);
	
	g_free (uri);
	g_free (mime_type);

	gtk_notebook_append_page (window->details->notebook, 
				  vbox, gtk_label_new (_("Open With")));
}


static FMPropertiesWindow *
create_properties_window (StartupData *startup_data)
{
	FMPropertiesWindow *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GList *l;

	window = FM_PROPERTIES_WINDOW (gtk_widget_new (fm_properties_window_get_type (), NULL));

	window->details->original_files = nautilus_file_list_copy (startup_data->original_files);
	
	window->details->target_files = nautilus_file_list_copy (startup_data->target_files);

	gtk_window_set_wmclass (GTK_WINDOW (window), "file_properties", "Nautilus");
	gtk_window_set_screen (GTK_WINDOW (window),
			       gtk_widget_get_screen (startup_data->parent_widget));

	/* Set initial window title */
	update_properties_window_title (window);

	/* Start monitoring the file attributes we display. Note that some
	 * of the attributes are for the original file, and some for the
	 * target files.
	 */

	for (l = window->details->original_files; l != NULL; l = l->next) {
		NautilusFile *file;
		NautilusFileAttributes attributes;

		file = NAUTILUS_FILE (l->data);

		attributes = nautilus_icon_factory_get_required_file_attributes ();
		attributes |= NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME
			| NAUTILUS_FILE_ATTRIBUTE_SLOW_MIME_TYPE;

		nautilus_file_monitor_add (NAUTILUS_FILE (l->data),
					   &window->details->original_files, 
					   attributes);	
	}
	
	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		NautilusFileAttributes attributes;

		file = NAUTILUS_FILE (l->data);
		
		attributes = 0;
		if (nautilus_file_is_directory (file)) {
			attributes |= NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS;
		}
		
		attributes |= NAUTILUS_FILE_ATTRIBUTE_METADATA;
		nautilus_file_monitor_add (file, &window->details->target_files, attributes);
	}	
		
	for (l = window->details->target_files; l != NULL; l = l->next) {
		g_signal_connect_object (NAUTILUS_FILE (l->data),
					 "changed",
					 G_CALLBACK (file_changed_callback),
					 G_OBJECT (window),
					 0);
	}

	for (l = window->details->original_files; l != NULL; l = l->next) {
		g_signal_connect_object (NAUTILUS_FILE (l->data),
					 "changed",
					 G_CALLBACK (file_changed_callback),
					 G_OBJECT (window),
					 0);
	}

	/* Create box for notebook and button box. */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window),
			   GTK_WIDGET (vbox));

	/* Create the notebook tabs. */
	window->details->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_widget_show (GTK_WIDGET (window->details->notebook));
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (window->details->notebook),
			    TRUE, TRUE, 0);

	/* Create the pages. */
	create_basic_page (window);

	if (should_show_emblems (window)) {
		create_emblems_page (window);
	}

	if (should_show_permissions (window)) {
		create_permissions_page (window);
	}

	if (should_show_open_with (window)) {
		create_open_with_page (window);
	}

	/* append pages from available views */
	append_extension_pages (window);

	/* Create box for help and close buttons. */
	hbox = gtk_hbutton_box_new ();
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, TRUE, 5);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_EDGE);

	button = gtk_button_new_from_stock (GTK_STOCK_HELP);
 	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (button),
			    FALSE, TRUE, 0);
	g_signal_connect_object (button, "clicked",
				 G_CALLBACK (help_button_callback),
				 window, 0);
	
	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (hbox), GTK_WIDGET (button),
			    FALSE, TRUE, 0);
	g_signal_connect_swapped (button, "clicked",
				  G_CALLBACK (gtk_widget_destroy),
				  window);

	/* Update from initial state */
	properties_window_update (window, NULL);

	return window;
}

static GList *
get_target_file_list (GList *original_files)
{
	GList *ret;
	GList *l;
	
	ret = NULL;
	
	for (l = original_files; l != NULL; l = l->next) {
		NautilusFile *target;
		
		target = get_target_file_for_original_file (NAUTILUS_FILE (l->data));
		
		ret = g_list_prepend (ret, target);
	}

	ret = g_list_reverse (ret);

	return ret;
}

static void
add_window (FMPropertiesWindow *window)
{
	if (!is_multi_file_window (window)) {
		g_hash_table_insert (windows,
				     get_original_file (window), 
				     window);
		g_object_set_data (G_OBJECT (window), "window_key", 
				   get_original_file (window));
	}
}

static void
remove_window (FMPropertiesWindow *window)
{
	gpointer key;

	key = g_object_get_data (G_OBJECT (window), "window_key");
	if (key) {
		g_hash_table_remove (windows, key);
	}
}

static GtkWindow *
get_existing_window (GList *file_list)
{
	if (!file_list->next) {
		return g_hash_table_lookup (windows, file_list->data);
	}	

	return NULL;
}

static void
cancel_create_properties_window_callback (gpointer callback_data)
{
	remove_pending ((StartupData *)callback_data, TRUE, FALSE, TRUE);
}

static void
parent_widget_destroyed_callback (GtkWidget *widget, gpointer callback_data)
{
	g_assert (widget == ((StartupData *)callback_data)->parent_widget);
	
	remove_pending ((StartupData *)callback_data, TRUE, TRUE, FALSE);
}

static void
cancel_call_when_ready_callback (gpointer key,
				 gpointer value,
				 gpointer user_data)
{
	nautilus_file_cancel_call_when_ready 
		(NAUTILUS_FILE (key), 
		 is_directory_ready_callback, 
		 user_data);
}

static void
remove_pending (StartupData *startup_data,
		gboolean cancel_call_when_ready,
		gboolean cancel_timed_wait,
		gboolean cancel_destroy_handler)
{
	if (cancel_call_when_ready) {
		g_hash_table_foreach (startup_data->pending_files,
				      cancel_call_when_ready_callback,
				      startup_data);
				      
	}
	if (cancel_timed_wait) {
		eel_timed_wait_stop 
			(cancel_create_properties_window_callback, startup_data);
	}
	if (cancel_destroy_handler) {
		g_signal_handlers_disconnect_by_func (startup_data->parent_widget,
						      G_CALLBACK (parent_widget_destroyed_callback),
						      startup_data);
	}

	g_hash_table_remove (pending_lists, startup_data->pending_key);

	startup_data_free (startup_data);
}

static void
is_directory_ready_callback (NautilusFile *file,
			     gpointer data)
{
	StartupData *startup_data;
	
	startup_data = data;
	
	g_hash_table_remove (startup_data->pending_files, file);

	if (g_hash_table_size (startup_data->pending_files) == 0) {
		FMPropertiesWindow *new_window;
		
		new_window = create_properties_window (startup_data);
		
		add_window (new_window);
		
		remove_pending (startup_data, FALSE, TRUE, TRUE);
		
/* FIXME bugzilla.gnome.org 42151:
 * See comment elsewhere in this file about bug 2151.
 */
#ifdef UNDO_ENABLED
		nautilus_undo_share_undo_manager (GTK_OBJECT (new_window),
						  GTK_OBJECT (callback_data));
#endif	
		gtk_window_present (GTK_WINDOW (new_window));
	}
}


void
fm_properties_window_present (GList *original_files,
			      GtkWidget *parent_widget) 
{
	GList *l, *next;
	GtkWidget *parent_window;
	StartupData *startup_data;
	GList *target_files;
	GtkWindow *existing_window;
	char *pending_key;

	g_return_if_fail (original_files != NULL);
	g_return_if_fail (GTK_IS_WIDGET (parent_widget));

	/* Create the hash tables first time through. */
	if (windows == NULL) {
		windows = eel_g_hash_table_new_free_at_exit
			(NULL, NULL, "property windows");
	}
	
	if (pending_lists == NULL) {
		pending_lists = eel_g_hash_table_new_free_at_exit
			(g_str_hash, g_str_equal, "pending property window files");
	}
	
	/* Look to see if there's already a window for this file. */
	existing_window = get_existing_window (original_files);
	if (existing_window != NULL) {
		gtk_window_set_screen (existing_window,
				       gtk_widget_get_screen (parent_widget));
		gtk_window_present (existing_window);
		return;
	}


	pending_key = get_pending_key (original_files);
	
	/* Look to see if we're already waiting for a window for this file. */
	if (g_hash_table_lookup (pending_lists, pending_key) != NULL) {
		return;
	}

	target_files = get_target_file_list (original_files);

	startup_data = startup_data_new (original_files, 
					 target_files,
					 pending_key,
					 parent_widget);

	nautilus_file_list_free (target_files);
	g_free(pending_key);

	/* Wait until we can tell whether it's a directory before showing, since
	 * some one-time layout decisions depend on that info. 
	 */
	
	g_hash_table_insert (pending_lists, startup_data->pending_key, startup_data->pending_key);
	g_signal_connect (parent_widget, "destroy",
			  G_CALLBACK (parent_widget_destroyed_callback), startup_data);

	parent_window = gtk_widget_get_ancestor (parent_widget, GTK_TYPE_WINDOW);

	eel_timed_wait_start
		(cancel_create_properties_window_callback,
		 startup_data,
		 _("Cancel Showing Properties Window?"),
		 _("Creating Properties window."),
		 parent_window == NULL ? NULL : GTK_WINDOW (parent_window));


	for (l = startup_data->target_files; l != NULL; l = next) {
		next = l->next;
		nautilus_file_call_when_ready
			(NAUTILUS_FILE (l->data),
			 NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY,
			 is_directory_ready_callback,
			 startup_data);
	}
}

static void
real_destroy (GtkObject *object)
{
	FMPropertiesWindow *window;
	GList *l;

	window = FM_PROPERTIES_WINDOW (object);

	remove_window (window);

	for (l = window->details->original_files; l != NULL; l = l->next) {
		nautilus_file_monitor_remove (NAUTILUS_FILE (l->data), &window->details->original_files);
	}
	nautilus_file_list_free (window->details->original_files);
	window->details->original_files = NULL;
	
	for (l = window->details->target_files; l != NULL; l = l->next) {
		nautilus_file_monitor_remove (NAUTILUS_FILE (l->data), &window->details->target_files);
	}
	nautilus_file_list_free (window->details->target_files);
	window->details->target_files = NULL;

	nautilus_file_list_free (window->details->changed_files);
	window->details->changed_files = NULL;
 
	window->details->name_field = NULL;
	
	g_list_free (window->details->directory_contents_widgets);
	window->details->directory_contents_widgets = NULL;

	g_list_free (window->details->special_flags_widgets);
	window->details->special_flags_widgets = NULL;

	g_list_free (window->details->emblem_buttons);
	window->details->emblem_buttons = NULL;

	if (window->details->initial_emblems) {
		g_hash_table_destroy (window->details->initial_emblems);
		window->details->initial_emblems = NULL;
	}

	g_list_free (window->details->permission_buttons);
	window->details->permission_buttons = NULL;

	if (window->details->initial_permissions) {
		g_hash_table_destroy (window->details->initial_permissions);
		window->details->initial_permissions = NULL;
	}

	g_list_free (window->details->value_fields);
	window->details->value_fields = NULL;

	if (window->details->update_directory_contents_timeout_id != 0) {
		g_source_remove (window->details->update_directory_contents_timeout_id);
		window->details->update_directory_contents_timeout_id = 0;
	}

	if (window->details->update_files_timeout_id != 0) {
		g_source_remove (window->details->update_files_timeout_id);
		window->details->update_files_timeout_id = 0;
	}

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
real_finalize (GObject *object)
{
	FMPropertiesWindow *window;

	window = FM_PROPERTIES_WINDOW (object);

	eel_g_list_free_deep (window->details->mime_list);

	g_free (window->details->pending_name);
	g_free (window->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* icon selection callback to set the image of the file object to the selected file */
static void
set_icon_callback (const char* icon_path, FMPropertiesWindow *properties_window)
{
	NautilusFile *file;
	char *icon_uri;
	GnomeDesktopItem *ditem;
	char *file_uri;
	
	g_return_if_fail (properties_window != NULL);
	g_return_if_fail (FM_IS_PROPERTIES_WINDOW (properties_window));

	if (icon_path != NULL) {
		GList *l;
		
		icon_uri = gnome_vfs_get_uri_from_local_path (icon_path);
		for (l = properties_window->details->original_files; l != NULL; l = l->next) {
			file = NAUTILUS_FILE (l->data);
			file_uri = nautilus_file_get_uri (file);
			
			if (nautilus_file_is_mime_type (file, "application/x-desktop")) {
				ditem = gnome_desktop_item_new_from_uri (file_uri,
								 GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
								 NULL);

				if (ditem != NULL) {
					gnome_desktop_item_set_string (ditem,
								       GNOME_DESKTOP_ITEM_ICON,
								       icon_path);
					gnome_desktop_item_save (ditem, NULL, TRUE, NULL);
					gnome_desktop_item_unref (ditem);
					nautilus_file_invalidate_attributes (file,
									     NAUTILUS_FILE_ATTRIBUTE_CUSTOM_ICON);
				}
			} else {
			
				nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL, icon_uri);
				nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_ICON_SCALE, NULL, NULL);
			}

			g_free (file_uri);

		}
		g_free (icon_uri);	
		
		/* re-enable the property window's clear image button */ 
		gtk_widget_set_sensitive (properties_window->details->remove_image_button, TRUE);
	}
}

/* handle the "select icon" button */
static void
select_image_button_callback (GtkWidget *widget, FMPropertiesWindow *properties_window)
{
	GtkWidget *dialog;

	g_assert (FM_IS_PROPERTIES_WINDOW (properties_window));

	dialog = properties_window->details->icon_selector_window;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog));
	} else {
		dialog = eel_gnome_icon_selector_new (_("Select an icon"),
						      NULL,
					 	      GTK_WINDOW (properties_window),
						      (EelIconSelectionFunction) set_icon_callback,
						      properties_window);
		
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

		properties_window->details->icon_selector_window = dialog;

		eel_add_weak_pointer (&properties_window->details->icon_selector_window);
	}
}

static void
remove_image_button_callback (GtkWidget *widget, FMPropertiesWindow *properties_window)
{
	g_return_if_fail (FM_IS_PROPERTIES_WINDOW (properties_window));

	reset_icon (properties_window);
}

static void
fm_properties_window_class_init (FMPropertiesWindowClass *class)
{
	G_OBJECT_CLASS (class)->finalize = real_finalize;
	GTK_OBJECT_CLASS (class)->destroy = real_destroy;
}

static void
fm_properties_window_instance_init (FMPropertiesWindow *window)
{
	window->details = g_new0 (FMPropertiesWindowDetails, 1);

	eel_gtk_window_set_up_close_accelerator (GTK_WINDOW (window));
}
