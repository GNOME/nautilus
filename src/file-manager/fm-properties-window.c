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
#include <eel/eel-ellipsizing-label.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-image.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-viewport.h>
#include <eel/eel-wrap-table.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktable.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-private/nautilus-customization-data.h>
#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#include <libnautilus/nautilus-undo.h>
#include <string.h>

static GHashTable *windows;
static GHashTable *pending_files;

struct FMPropertiesWindowDetails {
	NautilusFile *original_file;
	NautilusFile *target_file;
	GtkNotebook *notebook;
	GtkWidget *remove_image_button;
	
	guint file_changed_handler_id;

	GtkTable *basic_table;
	GtkTable *permissions_table;

	NautilusEntry *name_field;
	char *pending_name;

	GtkLabel *directory_contents_title_field;
	GtkLabel *directory_contents_value_field;
	guint update_directory_contents_timeout_id;

	GList *directory_contents_widgets;
	int directory_contents_row;

	GList *special_flags_widgets;
	int first_special_flags_row;
	int num_special_flags_rows;

	gboolean deep_count_finished;
};

enum {
	PERMISSIONS_CHECKBOXES_TITLE_ROW,
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
	NautilusFile *original_file;
	NautilusFile *target_file;
	FMDirectoryView *directory_view;
} StartupData;


/* drag and drop definitions */

enum {
	TARGET_URI_LIST,
	TARGET_GNOME_URI_LIST
};

static GtkTargetEntry target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "x-special/gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

#define ERASE_EMBLEM_FILENAME	"erase.png"

#define DIRECTORY_CONTENTS_UPDATE_INTERVAL	200 /* milliseconds */
#define STANDARD_EMBLEM_HEIGHT			52
#define EMBLEM_LABEL_SPACING			2

static void real_destroy                          (GtkObject               *object);
static void real_finalize                         (GtkObject               *object);
static void real_shutdown                         (GtkObject               *object);
static void fm_properties_window_initialize_class (FMPropertiesWindowClass *class);
static void fm_properties_window_initialize       (FMPropertiesWindow      *window);
static void create_properties_window_callback     (NautilusFile		   *file,
						   gpointer                 data);
static void cancel_group_change_callback          (gpointer                 callback_data);
static void cancel_owner_change_callback          (gpointer                 callback_data);
static void directory_view_destroyed_callback     (FMDirectoryView         *view,
						   gpointer                 callback_data);
static void select_image_button_callback          (GtkWidget               *widget,
						   FMPropertiesWindow      *properties_window);
static void set_icon_callback                     (const char* icon_path, 
						   FMPropertiesWindow *properties_window);
static void remove_image_button_callback          (GtkWidget               *widget,
						   FMPropertiesWindow      *properties_window);
static void remove_pending_file                   (StartupData             *data,
						   gboolean                 cancel_call_when_ready,
						   gboolean                 cancel_timed_wait,
						   gboolean                 cancel_destroy_handler);

EEL_DEFINE_CLASS_BOILERPLATE (FMPropertiesWindow, fm_properties_window, GTK_TYPE_WINDOW)

static void
fm_properties_window_initialize_class (FMPropertiesWindowClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	
	object_class->destroy = real_destroy;
	object_class->shutdown = real_shutdown;
	object_class->finalize = real_finalize;
}

static void
fm_properties_window_initialize (FMPropertiesWindow *window)
{
	window->details = g_new0 (FMPropertiesWindowDetails, 1);

	eel_gtk_window_set_up_close_accelerator (GTK_WINDOW (window));
}

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
get_pixbuf_for_properties_window (NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));
	
	return nautilus_icon_factory_get_pixbuf_for_file (file, NULL, NAUTILUS_ICON_SIZE_STANDARD, TRUE);
}

static void
update_properties_window_icon (EelImage *image)
{
	GdkPixbuf	*pixbuf;
	NautilusFile	*file;

	g_assert (EEL_IS_IMAGE (image));

	file = gtk_object_get_data (GTK_OBJECT (image), "nautilus_file");

	g_assert (NAUTILUS_IS_FILE (file));
	
	pixbuf = get_pixbuf_for_properties_window (file);

	eel_image_set_pixbuf (image, pixbuf);
	
	gdk_pixbuf_unref (pixbuf);
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

	pixbuf = gdk_pixbuf_new_from_file (image_path);
	g_free (image_path);
	
	if (pixbuf == NULL) {
		return FALSE;
	}
	gdk_pixbuf_unref (pixbuf);
	return TRUE;
}

static void  
fm_properties_window_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data,
					 guint info, guint time)
{
	char **uris;
	gboolean exactly_one;
	EelImage *image;
 	GtkWindow *window; 

	uris = g_strsplit (selection_data->data, "\r\n", 0);
	exactly_one = uris[0] != NULL && uris[1] == NULL;

	image = EEL_IMAGE (widget);
 	window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (image)));


	if (!exactly_one) {
		eel_show_error_dialog (
				       _("You can't assign more than one custom icon at a time! "
					 "Please drag just one image to set a custom icon."), 
				       _("More Than One Image"),
				       window);
	} else {		
		if (uri_is_local_image (uris[0])) {			
			set_icon_callback (gnome_vfs_get_local_path_from_uri (uris[0]), 
					   FM_PROPERTIES_WINDOW (window));
		} else {	
			if (eel_is_remote_uri (uris[0])) {
				eel_show_error_dialog (
						       _("The file that you dropped is not local.  "
							 "You can only use local images as custom icons."), 
						       _("Local Images Only"),
						       window);
				
			} else {
				eel_show_error_dialog (
						       _("The file that you dropped is not an image.  "
							 "You can only use local images as custom icons."),
						       _("Images Only"),
						       window);
			}
		}		
	}
	g_strfreev (uris);
}

static GtkWidget *
create_image_widget_for_file (NautilusFile *file)
{
 	GtkWidget *image;
	GdkPixbuf *pixbuf;
	
	pixbuf = get_pixbuf_for_properties_window (file);
	
	image = eel_image_new (NULL);

	/* prepare the image to receive dropped objects to assign custom images */
	gtk_drag_dest_set (GTK_WIDGET (image),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   target_table, EEL_N_ELEMENTS (target_table),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);

	gtk_signal_connect( GTK_OBJECT (image), "drag_data_received",
			    GTK_SIGNAL_FUNC (fm_properties_window_drag_data_received), NULL);


	eel_image_set_pixbuf (EEL_IMAGE (image), pixbuf);

	gdk_pixbuf_unref (pixbuf);

	nautilus_file_ref (file);
	gtk_object_set_data_full (GTK_OBJECT (image),
				  "nautilus_file",
				  file,
				  (GtkDestroyNotify) nautilus_file_unref);

	/* React to icon theme changes. */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_properties_window_icon,
					       GTK_OBJECT (image));

	/* Name changes can also change icon (since name is determined by MIME type) */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       update_properties_window_icon,
					       GTK_OBJECT (image));
	return image;
}

static void
name_field_update_to_match_file (NautilusEntry *name_field)
{
	NautilusFile *file;
	const char *original_name;
	char *current_name, *displayed_name;

	file = gtk_object_get_data (GTK_OBJECT (name_field), "nautilus_file");

	if (file == NULL || nautilus_file_is_gone (file)) {
		gtk_widget_set_sensitive (GTK_WIDGET (name_field), FALSE);
		gtk_entry_set_text (GTK_ENTRY (name_field), "");
		return;
	}

	original_name = (const char *) gtk_object_get_data (GTK_OBJECT (name_field),
						      	    "original_name");

	/* If the file name has changed since the original name was stored,
	 * update the text in the text field, possibly (deliberately) clobbering
	 * an edit in progress. If the name hasn't changed (but some other
	 * aspect of the file might have), then don't clobber changes.
	 */
	current_name = nautilus_file_get_display_name (file);
	if (eel_strcmp (original_name, current_name) != 0) {
		gtk_object_set_data_full (GTK_OBJECT (name_field),
					  "original_name",
					  current_name,
					  g_free);

		/* Only reset the text if it's different from what is
		 * currently showing. This causes minimal ripples (e.g.
		 * selection change).
		 */
		displayed_name = gtk_editable_get_chars (GTK_EDITABLE (name_field), 0, -1);
		if (strcmp (displayed_name, current_name) != 0) {
			gtk_entry_set_text (GTK_ENTRY (name_field), current_name);
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
	gtk_widget_set_sensitive (GTK_WIDGET (name_field), 
				  nautilus_file_can_rename (file));
}

static void
name_field_restore_original_name (NautilusEntry *name_field)
{
	const char *original_name;
	char *displayed_name;

	original_name = (const char *) gtk_object_get_data (GTK_OBJECT (name_field),
						      	    "original_name");
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
		/* This can trigger after window destroy, before finalize. */
		if (!GTK_OBJECT_DESTROYED (window)) {
			name_field_restore_original_name (window->details->name_field);
		}
	}

	gtk_object_unref (GTK_OBJECT (window));
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
	
	g_assert (NAUTILUS_IS_ENTRY (name_field));

	file = gtk_object_get_data (GTK_OBJECT (name_field), "nautilus_file");

	g_assert (NAUTILUS_IS_FILE (file));

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
		gtk_object_ref (GTK_OBJECT (window));
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

	return TRUE;
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

static void
property_button_update (GtkToggleButton *button)
{
	NautilusFile *file;
	char *name;
	GList *keywords, *word;

	file = gtk_object_get_data (GTK_OBJECT (button), "nautilus_file");
	name = gtk_object_get_data (GTK_OBJECT (button), "nautilus_property_name");

	/* Handle the case where there's nothing to toggle. */
	if (file == NULL || nautilus_file_is_gone (file) || name == NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
		gtk_toggle_button_set_active (button, FALSE);
		return;
	}

	/* Check and see if it's in there. */
	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, name, (GCompareFunc) strcmp);
	gtk_toggle_button_set_active (button, word != NULL);
}

static void
property_button_toggled (GtkToggleButton *button)
{
	NautilusFile *file;
	char *name;
	GList *keywords, *word;

	file = gtk_object_get_data (GTK_OBJECT (button), "nautilus_file");
	name = gtk_object_get_data (GTK_OBJECT (button), "nautilus_property_name");

	/* Handle the case where there's nothing to toggle. */
	if (file == NULL || nautilus_file_is_gone (file) || name == NULL) {
		return;
	}

	/* Check and see if it's already there. */
	keywords = nautilus_file_get_keywords (file);
	word = g_list_find_custom (keywords, name, (GCompareFunc) strcmp);
	if (gtk_toggle_button_get_active (button)) {
		if (word == NULL) {
			keywords = g_list_prepend (keywords, g_strdup (name));
		}
	} else {
		if (word != NULL) {
			keywords = g_list_remove_link (keywords, word);
			eel_g_list_free_deep (word);
		}
	}
	nautilus_file_set_keywords (file, keywords);
	eel_g_list_free_deep (keywords);
}

static void
update_properties_window_title (GtkWindow *window, NautilusFile *file)
{
	char *name, *title;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (GTK_IS_WINDOW (window));

	name = nautilus_file_get_display_name (file);
	title = g_strdup_printf (_("%s Properties"), name);
  	gtk_window_set_title (window, title);

	g_free (name);	
	g_free (title);
}

static void
properties_window_file_changed_callback (GtkWindow *window, NautilusFile *file)
{
	g_assert (GTK_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_FILE (file));

	if (nautilus_file_is_gone (file)) {
		gtk_widget_destroy (GTK_WIDGET (window));
	} else {
		update_properties_window_title (window, file);
	}
}

static void
value_field_update_internal (GtkLabel *label, 
			     NautilusFile *file, 
			     gboolean ellipsize_text)
{
	const char *attribute_name;
	char *attribute_value;

	g_assert (GTK_IS_LABEL (label));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (!ellipsize_text || EEL_IS_ELLIPSIZING_LABEL (label));

	attribute_name = gtk_object_get_data (GTK_OBJECT (label), "file_attribute");
	attribute_value = nautilus_file_get_string_attribute_with_default (file, attribute_name);

	if (ellipsize_text) {
		eel_ellipsizing_label_set_text (EEL_ELLIPSIZING_LABEL (label), 
						     attribute_value);
	} else {
		gtk_label_set_text (label, attribute_value);
	}
	g_free (attribute_value);	
}

static void
value_field_update (GtkLabel *label, NautilusFile *file)
{
	value_field_update_internal (label, file, FALSE);
}

static void
ellipsizing_value_field_update (GtkLabel *label, NautilusFile *file)
{
	value_field_update_internal (label, file, TRUE);
}

static GtkLabel *
attach_label (GtkTable *table,
	      int row,
	      int column,
	      const char *initial_text,
	      gboolean right_aligned,
	      gboolean bold,
	      gboolean ellipsize_text)
{
	GtkWidget *label_field;

	label_field = ellipsize_text
		? eel_ellipsizing_label_new (initial_text)
		: gtk_label_new (initial_text);	

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
	return attach_label (table, row, column, initial_text, FALSE, FALSE, FALSE);
}

static GtkLabel *
attach_ellipsizing_value_label (GtkTable *table,
				int row,
				int column,
				const char *initial_text)
{
	return attach_label (table, row, column, initial_text, FALSE, FALSE, TRUE);
}

static void
attach_value_field_internal (GtkTable *table,
			     int row,
			     int column,
			     NautilusFile *file,
			     const char *file_attribute_name,
			     gboolean ellipsize_text)
{
	GtkLabel *value_field;

	if (ellipsize_text) {
		value_field = attach_ellipsizing_value_label (table, row, column, "");
	} else {
		value_field = attach_value_label (table, row, column, "");
	}

	/* Stash a copy of the file attribute name in this field for the callback's sake. */
	gtk_object_set_data_full (GTK_OBJECT (value_field),
				  "file_attribute",
				  g_strdup (file_attribute_name),
				  g_free);

	/* Fill in the value. */
	if (ellipsize_text) {
		ellipsizing_value_field_update (value_field, file);
	} else {
		value_field_update (value_field, file);
	}

	/* Connect to signal to update value when file changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       ellipsize_text
					           ? ellipsizing_value_field_update
					           : value_field_update,
					       GTK_OBJECT (value_field));	
}			     

static void
attach_value_field (GtkTable *table,
		    int row,
		    int column,
		    NautilusFile *file,
		    const char *file_attribute_name)
{
	attach_value_field_internal (table, row, column, file, file_attribute_name, FALSE);
}

static void
attach_ellipsizing_value_field (GtkTable *table,
		    	  	int row,
		    		int column,
		    		NautilusFile *file,
		    		const char *file_attribute_name)
{
	attach_value_field_internal (table, row, column, file, file_attribute_name, TRUE);
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
		 _("Changing group"),
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
			    		       	      activate_group_callback,
			    		       	      file_name_pair_new (file, group_name),
			    		       	      (GtkDestroyNotify)file_name_pair_free);

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
		gtk_menu_append (GTK_MENU (new_menu), menu_item);
	}

	/* If current group wasn't in list, we prepend it (with a separator). 
	 * This can happen if the current group is an id with no matching
	 * group in the groups file.
	 */
	if (current_group_index < 0 && current_group_name != NULL) {
		if (groups != NULL) {
			menu_item = gtk_menu_item_new ();
			gtk_widget_show (menu_item);
			gtk_menu_prepend (GTK_MENU (new_menu), menu_item);
		}
		menu_item = create_group_menu_item (file, current_group_name);
		gtk_menu_prepend (GTK_MENU (new_menu), menu_item);
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

static void
attach_group_menu (GtkTable *table,
		   int row,
		   NautilusFile *file)
{
	GtkOptionMenu *option_menu;

	option_menu = attach_option_menu (table, row);

	synch_groups_menu (option_menu, file);

	/* Connect to signal to update menu when file changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       synch_groups_menu,
					       GTK_OBJECT (option_menu));	
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
		 _("Changing owner"),
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
			    		       	      activate_owner_callback,
			    		       	      file_name_pair_new (file, name_array[0]),
			    		       	      (GtkDestroyNotify)file_name_pair_free);
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
		gtk_menu_append (GTK_MENU (new_menu), menu_item);
	}

	/* If owner wasn't in list, we prepend it (with a separator). 
	 * This can happen if the owner is an id with no matching
	 * identifier in the passwords file.
	 */
	if (owner_index < 0 && owner_name != NULL) {
		if (users != NULL) {
			menu_item = gtk_menu_item_new ();
			gtk_widget_show (menu_item);
			gtk_menu_prepend (GTK_MENU (new_menu), menu_item);
		}
		menu_item = create_owner_menu_item (file, owner_name);
		gtk_menu_prepend (GTK_MENU (new_menu), menu_item);
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

static void
attach_owner_menu (GtkTable *table,
		   int row,
		   NautilusFile *file)
{
	GtkOptionMenu *option_menu;

	option_menu = attach_option_menu (table, row);

	synch_user_menu (option_menu, file);

	/* Connect to signal to update menu when file changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       synch_user_menu,
					       GTK_OBJECT (option_menu));	
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
	NautilusRequestStatus status;
	char *text, *temp;
	guint directory_count;
	guint file_count;
	guint total_count;
	guint unreadable_directory_count;
	GnomeVFSFileSize total_size;
	char *size_string;
	gboolean used_two_lines;
	NautilusFile *file;

	g_assert (FM_IS_PROPERTIES_WINDOW (window));

	file = window->details->target_file;
	g_assert (nautilus_file_is_directory (file) || nautilus_file_is_gone (file));

	status = nautilus_file_get_deep_counts (file, 
						&directory_count, 
						&file_count, 
						&unreadable_directory_count, 
						&total_size);

	/* If we've already displayed the total once, don't do another visible
	 * count-up if the deep_count happens to get invalidated. But still display
	 * the new total, since it might have changed.
	 */
	if (window->details->deep_count_finished && status != NAUTILUS_REQUEST_DONE) {
		return;
	}

	text = NULL;
	total_count = file_count + directory_count;
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
		size_string = gnome_vfs_format_file_size_for_display (total_size);
		if (total_count == 1) {
				text = g_strdup_printf (_("1 item, with size %s"), size_string);
		} else {
			text = g_strdup_printf (_("%d items, totalling %s"), total_count, size_string);
		}
		g_free (size_string);

		if (unreadable_directory_count != 0) {
			temp = text;
			text = g_strconcat (temp, "\n", _("(some contents unreadable)"), NULL);
			g_free (temp);
			used_two_lines = TRUE;
		}
	}

	gtk_label_set_text (window->details->directory_contents_value_field, text);
	g_free (text);

	/* Also set the title field here, with a trailing carriage return & space
	 * if the value field has two lines. This is a hack to get the
	 * "Contents:" title to line up with the first line of the 2-line value.
	 * Maybe there's a better way to do this, but I couldn't think of one.
	 */
	text = g_strdup (_("Contents:"));
	if (used_two_lines) {
		temp = text;
		text = g_strconcat (temp, "\n ", NULL);
		g_free (temp);
	}
	gtk_label_set_text (window->details->directory_contents_title_field, text);
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
			= gtk_timeout_add (DIRECTORY_CONTENTS_UPDATE_INTERVAL,
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

	value_field = attach_value_label (table, row, VALUE_COLUMN, "");

	g_assert (window->details->directory_contents_value_field == NULL);
	window->details->directory_contents_value_field = value_field;

	gtk_label_set_line_wrap (value_field, TRUE);

	/* Always recompute from scratch when the window is shown. */
	nautilus_file_recompute_deep_counts (window->details->target_file);

	/* Fill in the initial value. */
	directory_contents_value_field_update (window);

	/* Connect to signal to update value when file changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (window->details->target_file),
					       "updated_deep_count_in_progress",
					       schedule_directory_contents_update,
					       GTK_OBJECT (window));

	return value_field;	
}					

static GtkLabel *
attach_title_field (GtkTable *table,
		     int row,
		     const char *title)
{
	return attach_label (table, row, TITLE_COLUMN, title, TRUE, TRUE, FALSE);
}		      

static guint
append_title_field (GtkTable *table, const char *title)
{
	guint last_row;

	last_row = append_row (table);
	attach_title_field (table, last_row, title);

	return last_row;
}

static guint
append_title_value_pair (GtkTable *table, 
			 const char *title, 
			 NautilusFile *file, 
			 const char *file_attribute_name)
{
	guint last_row;

	last_row = append_title_field (table, title);
	attach_value_field (table, last_row, VALUE_COLUMN, file, file_attribute_name); 

	return last_row;
}

static guint
append_title_and_ellipsizing_value (GtkTable *table,
				    const char *title,
				    NautilusFile *file,
				    const char *file_attribute_name)
{
	guint last_row;

	last_row = append_title_field (table, title);
	attach_ellipsizing_value_field (table, last_row, VALUE_COLUMN, file, file_attribute_name);

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
	update_visibility_of_table_rows
		(window->details->basic_table,
		 nautilus_file_should_show_directory_item_count (window->details->target_file),
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
		 GTK_OBJECT (window));
	
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
	if (is_merged_trash_directory (window->details->target_file)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_file_type (FMPropertiesWindow *window) 
{
	/* The trash on the desktop is one-of-a-kind */
	if (is_merged_trash_directory (window->details->target_file)) {
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
	if (nautilus_file_is_directory (window->details->target_file)) {
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
	if (nautilus_file_is_directory (window->details->target_file)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_link_target (FMPropertiesWindow *window)
{
	if (nautilus_file_is_symbolic_link (window->details->target_file)) {
		return TRUE;
	}

	return FALSE;
}

static void
create_basic_page (FMPropertiesWindow *window)
{
	GtkTable *table;
	GtkWidget *container;
	GtkWidget *icon_pixmap_widget, *icon_aligner, *name_field;
	GtkWidget *button_box, *temp_button;
	char *image_uri;
	NautilusFile *target_file, *original_file;

	target_file = window->details->target_file;
	original_file = window->details->original_file;

	create_page_with_table_in_vbox (window->details->notebook, 
					_("Basic"), 
					1,
					&table, 
					&container);
	window->details->basic_table = table;
	
	/* Icon pixmap */
	icon_pixmap_widget = create_image_widget_for_file (original_file);
	gtk_widget_show (icon_pixmap_widget);
	
	icon_aligner = gtk_alignment_new (1, 0.5, 0, 0);
	gtk_widget_show (icon_aligner);

	gtk_container_add (GTK_CONTAINER (icon_aligner), icon_pixmap_widget);
	gtk_table_attach (table,
			  icon_aligner,
			  TITLE_COLUMN, 
			  TITLE_COLUMN + 1,
			  0, 1,
			  0, 0,
			  0, 0);

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
				   
	/* Attach parameters and signal handler. */
	nautilus_file_ref (original_file);
	gtk_object_set_data_full (GTK_OBJECT (name_field),
				  "nautilus_file",
				  original_file,
				  (GtkDestroyNotify) nautilus_file_unref);

	/* Update name field initially before hooking up changed signal. */
	name_field_update_to_match_file (NAUTILUS_ENTRY (name_field));

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

	gtk_signal_connect (GTK_OBJECT (name_field), "focus_out_event",
      	              	    GTK_SIGNAL_FUNC (name_field_focus_out),
                            window);
                      			    
	gtk_signal_connect (GTK_OBJECT (name_field), "activate",
      	              	    name_field_activate,
                            window);

        /* Start with name field selected, if it's sensitive. */
        if (GTK_WIDGET_SENSITIVE (name_field)) {
		nautilus_entry_select_all (NAUTILUS_ENTRY (name_field));
	        gtk_widget_grab_focus (GTK_WIDGET (name_field));
        }
                      			    
	/* React to name changes from elsewhere. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (target_file),
					       "changed",
					       name_field_update_to_match_file,
					       GTK_OBJECT (name_field));

	if (should_show_file_type (window)) {
		append_title_value_pair (table, _("Type:"), target_file, "type");
	}
	if (nautilus_file_is_directory (target_file)) {
		append_directory_contents_fields (window, table);
	} else {
		append_title_value_pair (table, _("Size:"), target_file, "size");
	}
	append_title_and_ellipsizing_value (table, _("Location:"), target_file, "where");
	if (should_show_link_target (window)) {
		append_title_and_ellipsizing_value (table, _("Link Target:"), target_file, "link_target");
	}
	if (should_show_mime_type (window)) {
		append_title_value_pair (table, _("MIME Type:"), target_file, "mime_type");
	}				  
	
	/* Blank title ensures standard row height */
	append_title_field (table, "");
	
	append_title_value_pair (table, _("Modified:"), target_file, "date_modified");

	if (should_show_accessed_date (window)) {
		append_title_value_pair (table, _("Accessed:"), target_file, "date_accessed");
	}

	if (should_show_custom_icon_buttons (window)) {
		/* add command buttons for setting and clearing custom icons */
		button_box = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (button_box);
		gtk_box_pack_end (GTK_BOX(container), button_box, FALSE, FALSE, 4);  
		
	 	temp_button = gtk_button_new_with_label (_("Select Custom Icon..."));
		gtk_widget_show (temp_button);
		gtk_box_pack_start (GTK_BOX (button_box), temp_button, FALSE, FALSE, 4);  
		eel_gtk_button_set_standard_padding (GTK_BUTTON (temp_button));
		gtk_signal_connect(GTK_OBJECT (temp_button), "clicked", GTK_SIGNAL_FUNC (select_image_button_callback), window);
	 	
	 	temp_button = gtk_button_new_with_label (_("Remove Custom Icon"));
		gtk_widget_show (temp_button);
		gtk_box_pack_start (GTK_BOX(button_box), temp_button, FALSE, FALSE, 4);  
		eel_gtk_button_set_standard_padding (GTK_BUTTON (temp_button));
	 	gtk_signal_connect (GTK_OBJECT (temp_button), "clicked", GTK_SIGNAL_FUNC (remove_image_button_callback), window);

		window->details->remove_image_button = temp_button;
		
		/* de-sensitize the remove button if there isn't a custom image */
		image_uri = nautilus_file_get_metadata 
			(original_file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);
		gtk_widget_set_sensitive (temp_button, image_uri != NULL);
		g_free (image_uri);
	}
}

static void
remove_default_viewport_shadow (GtkViewport *viewport)
{
	g_return_if_fail (GTK_IS_VIEWPORT (viewport));
	
	gtk_viewport_set_shadow_type (viewport, GTK_SHADOW_NONE);
}

static void
create_emblems_page (FMPropertiesWindow *window)
{
	NautilusCustomizationData *customization_data;
	GtkWidget *emblems_table, *button, *scroller;
	char *emblem_name, *dot_pos;
	GdkPixbuf *pixbuf;
	char *label;
	NautilusFile *file;

	file = window->details->target_file;

	/* The emblems wrapped table */
	emblems_table = eel_wrap_table_new (TRUE);

	gtk_widget_show (emblems_table);
	gtk_container_set_border_width (GTK_CONTAINER (emblems_table), GNOME_PAD);
	
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);

	/* Viewport */
 	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroller), 
 					       emblems_table);
	gtk_widget_show (scroller);

	/* Get rid of default lowered shadow appearance. 
	 * This must be done after the widget is realized, due to
	 * an apparent bug in gtk_viewport_set_shadow_type.
	 */
	gtk_signal_connect (GTK_OBJECT (GTK_BIN (scroller)->child), 
			    "realize", 
			    remove_default_viewport_shadow, 
			    NULL);

	gtk_notebook_append_page (window->details->notebook, 
				  scroller, gtk_label_new (_("Emblems")));
	
	/* Use nautilus_customization to make the emblem widgets */
	customization_data = nautilus_customization_data_new ("emblems", TRUE, TRUE,
							      NAUTILUS_ICON_SIZE_SMALL, 
							      NAUTILUS_ICON_SIZE_SMALL);
	
	while (nautilus_customization_data_get_next_element_for_display (customization_data,
									 &emblem_name,
									 &pixbuf,
									 &label) == GNOME_VFS_OK) {	

		/* strip the suffix, if any */
		dot_pos = strrchr(emblem_name, '.');
		if (dot_pos) {
			*dot_pos = '\0';
		}
		
		if (strcmp (emblem_name, "erase") == 0) {
			gdk_pixbuf_unref (pixbuf);
			g_free (label);
			g_free (emblem_name);
			continue;
		}
		
		button = eel_labeled_image_check_button_new (label, pixbuf);
		eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (GTK_BIN (button)->child), STANDARD_EMBLEM_HEIGHT);
		eel_labeled_image_set_spacing (EEL_LABELED_IMAGE (GTK_BIN (button)->child), EMBLEM_LABEL_SPACING);
		
		g_free (label);
		gdk_pixbuf_unref (pixbuf);

		/* Attach parameters and signal handler. */
		gtk_object_set_data_full (GTK_OBJECT (button),
					  "nautilus_property_name",
					  emblem_name,
					  g_free);
				     
		nautilus_file_ref (file);
		gtk_object_set_data_full (GTK_OBJECT (button),
					  "nautilus_file",
					  file,
					  (GtkDestroyNotify) nautilus_file_unref);
		
		gtk_signal_connect (GTK_OBJECT (button),
				    "toggled",
				    property_button_toggled,
				    NULL);

		/* Set initial state of button. */
		property_button_update (GTK_TOGGLE_BUTTON (button));

		/* Update button when file changes in future. */
		gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
						       "changed",
						       property_button_update,
						       GTK_OBJECT (button));

		gtk_container_add (GTK_CONTAINER (emblems_table), button);
	}
	gtk_widget_show_all (emblems_table);
}

static void
add_permissions_column_label (GtkTable *table, 
			      NautilusFile *file,
			      int column, 
			      const char *title_text)
{
	GtkWidget *label;

	label = gtk_label_new (title_text);
	eel_gtk_label_make_bold (GTK_LABEL (label));
	gtk_widget_set_sensitive (GTK_WIDGET (label), 
				  nautilus_file_can_set_permissions (file));

	/* Text is centered in table cell by default, which is what we want here. */
	gtk_widget_show (label);
	
	gtk_table_attach_defaults (table, label,
			  	   column, column + 1,
			  	   PERMISSIONS_CHECKBOXES_TITLE_ROW, 
			  	   PERMISSIONS_CHECKBOXES_TITLE_ROW + 1);
}	

static void
update_permissions_check_button_state (GtkWidget *check_button, NautilusFile *file)
{
	GnomeVFSFilePermissions file_permissions, permission;
	guint toggled_signal_id;

	g_assert (GTK_IS_CHECK_BUTTON (check_button));
	g_assert (NAUTILUS_IS_FILE (file));

	if (nautilus_file_is_gone (file)) {
		return;
	}
	
	g_assert (nautilus_file_can_get_permissions (file));

	toggled_signal_id = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (check_button),
						 		  "toggled_signal_id"));
	permission = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (check_button),
							   "permission"));
	g_assert (toggled_signal_id > 0);
	g_assert (permission != 0);

	file_permissions = nautilus_file_get_permissions (file);
	gtk_widget_set_sensitive (GTK_WIDGET (check_button), 
				  nautilus_file_can_set_permissions (file));

	/* Don't react to the "toggled" signal here to avoid recursion. */
	gtk_signal_handler_block (GTK_OBJECT (check_button),
				  toggled_signal_id);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				      (file_permissions & permission) != 0);
	gtk_signal_handler_unblock (GTK_OBJECT (check_button),
				    toggled_signal_id);
}

static void
permission_change_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	g_assert (callback_data == NULL);
	
	/* Report the error if it's an error. */
	fm_report_error_setting_permissions (file, result, NULL);
}

static void
permissions_check_button_toggled (GtkToggleButton *toggle_button, gpointer user_data)
{
	NautilusFile *file;
	GnomeVFSFilePermissions permissions, permission_mask;

	g_assert (NAUTILUS_IS_FILE (user_data));

	file = NAUTILUS_FILE (user_data);
	g_assert (nautilus_file_can_get_permissions (file));
	g_assert (nautilus_file_can_set_permissions (file));

	permission_mask = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (toggle_button),
					  "permission"));

	/* Modify the file's permissions according to the state of this check button. */
	permissions = nautilus_file_get_permissions (file);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle_button))) {
		/* Turn this specific permission bit on. */
		permissions |= permission_mask;
	} else {
		/* Turn this specific permission bit off. */
		permissions &= ~permission_mask;
	}

	/* Try to change file permissions. If this fails, complain to user. */
	nautilus_file_set_permissions
		(file, permissions,
		 permission_change_callback, NULL);
}

static void
set_up_permissions_checkbox (GtkWidget *check_button, 
			     NautilusFile *file,
			     GnomeVFSFilePermissions permission)
{
	guint toggled_signal_id;

	toggled_signal_id = gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
      	              	    			GTK_SIGNAL_FUNC (permissions_check_button_toggled),
                            			file);

	/* Load up the check_button with data we'll need when updating its state. */
        gtk_object_set_data (GTK_OBJECT (check_button), 
        		     "toggled_signal_id", 
        		     GINT_TO_POINTER (toggled_signal_id));
        gtk_object_set_data (GTK_OBJECT (check_button), 
        		     "permission", 
        		     GINT_TO_POINTER (permission));
	
	/* Set initial state. */
        update_permissions_check_button_state (check_button, file);

        /* Update state later if file changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       update_permissions_check_button_state,
					       GTK_OBJECT (check_button));
}

static void
add_permissions_checkbox (GtkTable *table, 
			  NautilusFile *file,
			  int row, int column, 
			  GnomeVFSFilePermissions permission_to_check)
{
	GtkWidget *check_button;

	check_button = gtk_check_button_new ();
	gtk_widget_show (check_button);
	gtk_table_attach (table, check_button,
			  column, column + 1,
			  row, row + 1,
			  0, 0,
			  0, 0);

	set_up_permissions_checkbox (check_button, file, permission_to_check);
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

	check_button = gtk_check_button_new_with_label (label_text);
	gtk_widget_show (check_button);

	gtk_table_attach (table, check_button,
			  VALUE_COLUMN, VALUE_COLUMN + 1,
			  last_row, last_row + 1,
			  GTK_FILL, 0,
			  0, 0);

	set_up_permissions_checkbox (check_button, window->details->target_file, permission_to_check);
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
append_special_execution_flags (FMPropertiesWindow *window,
				GtkTable *table)
{
	remember_special_flags_widget (window, append_special_execution_checkbox 
		(window, table, _("Set User ID"), GNOME_VFS_PERM_SUID));

	window->details->first_special_flags_row = table->nrows - 1;

	remember_special_flags_widget (window, GTK_WIDGET (attach_title_field 
		(table, table->nrows - 1, _("Special Flags:"))));

	remember_special_flags_widget (window, append_special_execution_checkbox 
		(window, table, _("Set Group ID"), GNOME_VFS_PERM_SGID));
	remember_special_flags_widget (window, append_special_execution_checkbox 
		(window, table, _("Sticky"), GNOME_VFS_PERM_STICKY));

	remember_special_flags_widget (window, append_separator (table));
	++window->details->num_special_flags_rows;

	update_visibility_of_special_flags_widgets (window);
	eel_preferences_add_callback_while_alive 
		(NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
		 update_visibility_of_special_flags_widgets_wrapper,
		 window,
		 GTK_OBJECT (window));
	
}

static void
create_permissions_page (FMPropertiesWindow *window)
{
	GtkWidget *vbox;
	GtkTable *page_table, *check_button_table;
	char *file_name, *prompt_text;
	NautilusFile *file;
	guint last_row;
	guint checkbox_titles_row;

	file = window->details->target_file;

	vbox = create_page_with_vbox (window->details->notebook, _("Permissions"));

	if (nautilus_file_can_get_permissions (file)) {
		if (!nautilus_file_can_set_permissions (file)) {
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

		attach_title_field (page_table, last_row, _("File Owner:"));
		if (nautilus_file_can_set_owner (file)) {
			/* Option menu in this case. */
			attach_owner_menu (page_table, last_row, file);
		} else {
			/* Static text in this case. */
			attach_value_field (page_table, last_row, VALUE_COLUMN, file, "owner"); 
		}

		last_row = append_title_field (page_table, _("File Group:"));
		if (nautilus_file_can_set_group (file)) {
			/* Option menu in this case. */
			attach_group_menu (page_table, last_row, file);
		} else {
			/* Static text in this case. */
			attach_value_field (page_table, last_row, VALUE_COLUMN, file, "group"); 
		}

		append_separator (page_table);

		/* This next empty label is a hack to make the title row
		 * in the main table the same height as the title row in
		 * the checkboxes sub-table so the other row titles will
		 * line up horizontally with the checkbox rows.
		 */
		checkbox_titles_row = append_title_field (page_table, "");

		append_title_field (page_table, _("Owner:"));
		append_title_field (page_table, _("Group:"));
		append_title_field (page_table, _("Others:"));
		
		/* Make separate table for grid of checkboxes so it can be
		 * homogeneous; we don't want overall table to be homogeneous though.
		 */
		check_button_table = GTK_TABLE (gtk_table_new 
						   (PERMISSIONS_CHECKBOXES_ROW_COUNT, 
						    PERMISSIONS_CHECKBOXES_COLUMN_COUNT, 
						    TRUE));
		apply_standard_table_padding (check_button_table);
		gtk_widget_show (GTK_WIDGET (check_button_table));
		gtk_table_attach (page_table, GTK_WIDGET (check_button_table),
				  VALUE_COLUMN, VALUE_COLUMN + 1, 
				  checkbox_titles_row, checkbox_titles_row + PERMISSIONS_CHECKBOXES_ROW_COUNT,
				  0, 0,
				  0, 0);

		add_permissions_column_label (check_button_table, file,
					      PERMISSIONS_CHECKBOXES_READ_COLUMN,
					      _("Read"));

		add_permissions_column_label (check_button_table, file,
					      PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					      _("Write"));

		add_permissions_column_label (check_button_table, file,
					      PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					      _("Execute"));

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_USER_READ);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_USER_WRITE);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_USER_EXEC);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_GROUP_READ);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_GROUP_WRITE);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_GROUP_EXEC);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_OTHER_READ);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_OTHER_WRITE);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_CHECKBOXES_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_OTHER_EXEC);

		append_separator (page_table);

		append_special_execution_flags (window, page_table);

		append_title_value_pair (page_table, _("Text View:"), file, "permissions");		
		append_title_value_pair (page_table, _("Number View:"), file, "octal_permissions");
		append_title_value_pair (page_table, _("Last Changed:"), file, "date_permissions");
		
	} else {
		file_name = nautilus_file_get_display_name (file);
		prompt_text = g_strdup_printf (_("The permissions of \"%s\" could not be determined."), file_name);
		g_free (file_name);
		add_prompt (GTK_VBOX (vbox), prompt_text, TRUE);
		g_free (prompt_text);
	}
}

static gboolean
should_show_emblems (FMPropertiesWindow *window) 
{
	/* FIXME bugzilla.gnome.org 45643:
	 * Emblems aren't displayed on the the desktop Trash icon, so
	 * we shouldn't pretend that they work by showing them here.
	 * When bug 5643 is fixed we can remove this case.
	 */
	if (is_merged_trash_directory (window->details->target_file)) {
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
	if (is_merged_trash_directory (window->details->target_file)) {
		return FALSE;
	}

	return TRUE;
}

static StartupData *
startup_data_new (NautilusFile *original_file, 
		  NautilusFile *target_file,
		  FMDirectoryView *directory_view)
{
	StartupData *data;

	data = g_new0 (StartupData, 1);
	data->original_file = nautilus_file_ref (original_file);
	data->target_file = nautilus_file_ref (target_file);
	data->directory_view = directory_view;

	return data;
}

static void
startup_data_free (StartupData *data)
{
	nautilus_file_unref (data->original_file);
	nautilus_file_unref (data->target_file);
	g_free (data);
}

static FMPropertiesWindow *
create_properties_window (StartupData *startup_data)
{
	FMPropertiesWindow *window;
	GList *attributes;

	window = FM_PROPERTIES_WINDOW (gtk_widget_new (fm_properties_window_get_type (), NULL));

	window->details->original_file = nautilus_file_ref (startup_data->original_file);
	window->details->target_file = nautilus_file_ref (startup_data->target_file);
	
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gtk_window_set_wmclass (GTK_WINDOW (window), "file_properties", "Nautilus");

	/* Set initial window title */
	update_properties_window_title (GTK_WINDOW (window), window->details->target_file);

	/* Start monitoring the file attributes we display. Note that some
	 * of the attributes are for the original file, and some for the
	 * target file.
	 */
	attributes = nautilus_icon_factory_get_required_file_attributes ();
	attributes = g_list_prepend (attributes,
				     NAUTILUS_FILE_ATTRIBUTE_DISPLAY_NAME);
	nautilus_file_monitor_add (window->details->original_file, window, attributes);
	g_list_free (attributes);

	attributes = NULL;
	if (nautilus_file_is_directory (window->details->target_file)) {
		attributes = g_list_prepend (attributes,
					     NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS);
	}
	attributes = g_list_prepend (attributes,
				     NAUTILUS_FILE_ATTRIBUTE_METADATA);
	nautilus_file_monitor_add (window->details->target_file, window, attributes);
	g_list_free (attributes);

	/* React to future property changes and file deletions. */
	window->details->file_changed_handler_id =
		gtk_signal_connect_object (GTK_OBJECT (window->details->target_file),
					   "changed",
					   properties_window_file_changed_callback,
					   GTK_OBJECT (window));

	/* Create the notebook tabs. */
	window->details->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_widget_show (GTK_WIDGET (window->details->notebook));
	gtk_container_add (GTK_CONTAINER (window), 
			   GTK_WIDGET (window->details->notebook));

	/* Create the pages. */
	create_basic_page (window);

	if (should_show_emblems (window)) {
		create_emblems_page (window);
	}

	if (should_show_permissions (window)) {
		create_permissions_page (window);
	}

	return window;
}

static NautilusFile *
get_target_file (NautilusFile *file)
{
	NautilusFile *target_file;
	char *uri;
	char *uri_to_display;
	char *local_path;

	target_file = NULL;
	if (nautilus_file_is_nautilus_link (file)) {
		/* Note: This will only work on local files. For now
		 * that seems fine since the links we care about are
		 * all on the desktop.
		 */
		uri = nautilus_file_get_uri (file);
		local_path = gnome_vfs_get_local_path_from_uri (uri);
		if (local_path != NULL) {
			switch (nautilus_link_local_get_link_type (local_path)) {
			case NAUTILUS_LINK_MOUNT:
			case NAUTILUS_LINK_TRASH:
			case NAUTILUS_LINK_HOME:
				/* map to linked URI for these types of links */
				uri_to_display = nautilus_link_local_get_link_uri (local_path);
				target_file = nautilus_file_get (uri_to_display);
				g_free (uri_to_display);
				break;
			case NAUTILUS_LINK_GENERIC:
				/* don't for these types */
				break;
			}
			g_free (local_path);
		}
		
		g_free (uri);
	}

	if (target_file != NULL) {
		return target_file;
	}

	/* Ref passed-in file here since we've decided to use it. */
	nautilus_file_ref (file);
	return file;
}

static void
create_properties_window_callback (NautilusFile *file, gpointer callback_data)
{
	FMPropertiesWindow *new_window;
	StartupData *startup_data;

	startup_data = (StartupData *)callback_data;

	new_window = create_properties_window (startup_data);

	g_hash_table_insert (windows, startup_data->original_file, new_window);

	remove_pending_file (startup_data, FALSE, TRUE, TRUE);

/* FIXME bugzilla.gnome.org 42151:
 * See comment elsewhere in this file about bug 2151.
 */
#ifdef UNDO_ENABLED
	nautilus_undo_share_undo_manager (GTK_OBJECT (new_window),
					  GTK_OBJECT (callback_data));
#endif	
	eel_gtk_window_present (GTK_WINDOW (new_window));
}

static void
cancel_create_properties_window_callback (gpointer callback_data)
{
	remove_pending_file ((StartupData *)callback_data, TRUE, FALSE, TRUE);
}

static void
directory_view_destroyed_callback (FMDirectoryView *view, gpointer callback_data)
{
	g_assert (view == ((StartupData *)callback_data)->directory_view);
	
	remove_pending_file ((StartupData *)callback_data, TRUE, TRUE, FALSE);
}

static void
remove_pending_file (StartupData *startup_data,
		     gboolean cancel_call_when_ready,
		     gboolean cancel_timed_wait,
		     gboolean cancel_destroy_handler)
{
	if (cancel_call_when_ready) {
		nautilus_file_cancel_call_when_ready 
			(startup_data->target_file, create_properties_window_callback, startup_data);
	}
	if (cancel_timed_wait) {
		eel_timed_wait_stop 
			(cancel_create_properties_window_callback, startup_data);
	}
	if (cancel_destroy_handler) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (startup_data->directory_view),
					       directory_view_destroyed_callback,
					       startup_data);
	}
	g_hash_table_remove (pending_files, startup_data->original_file);
	startup_data_free (startup_data);
}

void
fm_properties_window_present (NautilusFile *original_file, FMDirectoryView *directory_view)
{
	GtkWindow *existing_window;
	GtkWidget *parent_window;
	NautilusFile *target_file;
	StartupData *startup_data;
	GList attribute_list;

	g_return_if_fail (NAUTILUS_IS_FILE (original_file));
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (directory_view));

	/* Create the hash tables first time through. */
	if (windows == NULL) {
		windows = eel_g_hash_table_new_free_at_exit
			(NULL, NULL, "property windows");
	}
	
	if (pending_files == NULL) {
		pending_files = eel_g_hash_table_new_free_at_exit
			(NULL, NULL, "pending property window files");
	}
	
	/* Look to see if there's already a window for this file. */
	existing_window = g_hash_table_lookup (windows, original_file);
	if (existing_window != NULL) {
		eel_gtk_window_present (existing_window);
		return;
	}

	/* Look to see if we're already waiting for a window for this file. */
	if (g_hash_table_lookup (pending_files, original_file) != NULL) {
		return;
	}

	target_file = get_target_file (original_file);
	startup_data = startup_data_new (original_file, target_file, directory_view);
	nautilus_file_unref (target_file);

	/* Wait until we can tell whether it's a directory before showing, since
	 * some one-time layout decisions depend on that info. 
	 */
	
	g_hash_table_insert (pending_files, target_file, target_file);
	gtk_signal_connect (GTK_OBJECT (directory_view),
			    "destroy",
			    directory_view_destroyed_callback,
			    startup_data);

	parent_window = gtk_widget_get_ancestor (GTK_WIDGET (directory_view), GTK_TYPE_WINDOW);
	eel_timed_wait_start
		(cancel_create_properties_window_callback,
		 startup_data,
		 _("Cancel Showing Properties Window?"),
		 _("Creating Properties window"),
		 parent_window == NULL ? NULL : GTK_WINDOW (parent_window));
	attribute_list.data = NAUTILUS_FILE_ATTRIBUTE_IS_DIRECTORY;
	attribute_list.next = NULL;
	attribute_list.prev = NULL;
	nautilus_file_call_when_ready
		(target_file, &attribute_list,
		 create_properties_window_callback, startup_data);
}

static void
real_shutdown (GtkObject *object)
{
	FMPropertiesWindow *window;

	window = FM_PROPERTIES_WINDOW (object);

	/* Disconnect file-changed handler here to avoid infinite loop
	 * of change notifications when file is removed; see bug 4911.
	 */
	gtk_signal_disconnect (GTK_OBJECT (window->details->target_file), 
			       window->details->file_changed_handler_id);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, shutdown, (object));
}

static void
real_destroy (GtkObject *object)
{
	FMPropertiesWindow *window;

	window = FM_PROPERTIES_WINDOW (object);

	g_hash_table_remove (windows, window->details->original_file);

	nautilus_file_monitor_remove (window->details->original_file, window);
	nautilus_file_unref (window->details->original_file);

	nautilus_file_monitor_remove (window->details->target_file, window);
	nautilus_file_unref (window->details->target_file);	
	
	g_list_free (window->details->directory_contents_widgets);
	g_list_free (window->details->special_flags_widgets);

	if (window->details->update_directory_contents_timeout_id != 0) {
		gtk_timeout_remove (window->details->update_directory_contents_timeout_id);
	}


	/* Note that file_changed_handler_id is disconnected in shutdown,
	 * and details are freed in finalize 
	 */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
real_finalize (GtkObject *object)
{
	FMPropertiesWindow *window;

	window = FM_PROPERTIES_WINDOW (object);

	/* Note that file_changed_handler_id is disconnected in shutdown */
	g_free (window->details->pending_name);
	g_free (window->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, finalize, (object));
}

/* icon selection callback to set the image of the file object to the selected file */
static void
set_icon_callback (const char* icon_path, FMPropertiesWindow *properties_window)
{
	NautilusFile *file;
	char *icon_uri;
	
	g_return_if_fail (properties_window != NULL);
	g_return_if_fail (FM_IS_PROPERTIES_WINDOW (properties_window));

	if (icon_path != NULL) {
		file = properties_window->details->original_file;
		icon_uri = gnome_vfs_get_uri_from_local_path (icon_path);
		nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL, icon_uri);
		g_free (icon_uri);
		nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_ICON_SCALE, NULL, NULL);

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

	dialog = eel_gnome_icon_selector_new (_("Select an icon:"),
						   NULL,
						   GTK_WINDOW (properties_window),
						   (EelIconSelectionFunction) set_icon_callback,
						   properties_window);						   
}

static void
remove_image_button_callback (GtkWidget *widget, FMPropertiesWindow *properties_window)
{
	g_assert (FM_IS_PROPERTIES_WINDOW (properties_window));

	nautilus_file_set_metadata (properties_window->details->original_file,
				    NAUTILUS_METADATA_KEY_ICON_SCALE,
				    NULL, NULL);
	nautilus_file_set_metadata (properties_window->details->original_file,
				    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
				    NULL, NULL);
	
	gtk_widget_set_sensitive (widget, FALSE);
}


