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

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "fm-properties-window.h"

#include "fm-error-reporting.h"

#include <string.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktable.h>
#include <gtk/gtkvbox.h>

#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-string.h>

static GHashTable *windows;

static const char * const property_names[] =
{
	"certified",
	"changed",
	"confidential",
	"encrypted",
	"important",
	"new",
	"personal",
	"remote"
};

enum {
	BASIC_PAGE_ICON_AND_NAME_ROW,
	BASIC_PAGE_LOCATION_ROW,
	BASIC_PAGE_TYPE_ROW,
	BASIC_PAGE_SIZE_ROW,
	BASIC_PAGE_DATE_ROW,
	BASIC_PAGE_ROW_COUNT
};

enum {
	BASIC_PAGE_TITLE_COLUMN,
	BASIC_PAGE_VALUE_COLUMN,
	BASIC_PAGE_COLUMN_COUNT
};

static void
get_pixmap_and_mask_for_properties_window (NautilusFile *file,
					   GdkPixmap **pixmap_return,
					   GdkBitmap **mask_return)
{
	GdkPixbuf *pixbuf;

	g_assert (NAUTILUS_IS_FILE (file));
	
	pixbuf = nautilus_icon_factory_get_pixbuf_for_file (file, NAUTILUS_ICON_SIZE_STANDARD);
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap_return, mask_return, 128);
	gdk_pixbuf_unref (pixbuf);
}

static void
update_properties_window_icon (GtkPixmap *pixmap_widget)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	NautilusFile *file;

	g_assert (GTK_IS_PIXMAP (pixmap_widget));

	file = gtk_object_get_data (GTK_OBJECT (pixmap_widget), "nautilus_file");

	g_assert (NAUTILUS_IS_FILE (file));

	get_pixmap_and_mask_for_properties_window (file, &pixmap, &mask);
	gtk_pixmap_set (pixmap_widget, pixmap, mask);
}

static GtkWidget *
create_pixmap_widget_for_file (NautilusFile *file)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *widget;

	get_pixmap_and_mask_for_properties_window (file, &pixmap, &mask);
	widget = gtk_pixmap_new (pixmap, mask);

	gtk_object_set_data_full (GTK_OBJECT (widget),
				  "nautilus_file",
				  file,
				  (GtkDestroyNotify) nautilus_file_unref);

	/* React to icon theme changes. */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_properties_window_icon,
					       GTK_OBJECT (widget));

	/* Name changes can also change icon (since name is determined by MIME type) */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       update_properties_window_icon,
					       GTK_OBJECT (widget));
	return widget;
}

static void
name_field_done_editing (NautilusEntry *name_field)
{
	NautilusFile *file;
	GnomeVFSResult rename_result;
	const char *new_name;
	char *original_name;
	gboolean empty_name;
	
	g_assert (NAUTILUS_IS_ENTRY (name_field));

	file = gtk_object_get_data (GTK_OBJECT (name_field), "nautilus_file");

	g_assert (NAUTILUS_IS_FILE (file));
	if (nautilus_file_is_gone (file)) {
		return;
	}

	new_name = gtk_entry_get_text (GTK_ENTRY (name_field));

	/* Special case: silently revert text if new text is empty. */
	if (strlen (new_name) == 0) {
		empty_name = TRUE;
	} else {
		rename_result = nautilus_file_rename (file, new_name);

		if (rename_result == GNOME_VFS_OK) {
			return;
		}
	}	

	/* Rename failed; restore old name, complain to user. */
	original_name = nautilus_file_get_name (file);
	gtk_entry_set_text (GTK_ENTRY (name_field), original_name);


	if (!empty_name) {
		fm_report_error_renaming_file (original_name,
			 		       new_name,
			 		       rename_result);
	}
	
	g_free (original_name);
}

static gboolean
name_field_focus_out (NautilusEntry *name_field,
		      GdkEventFocus *event,
		      gpointer user_data)
{
	name_field_done_editing (name_field);
	gtk_editable_select_region (GTK_EDITABLE (name_field), -1, -1);

	return TRUE;
}

static gboolean
name_field_focus_in (NautilusEntry *name_field,
		      GdkEventFocus *event,
		      gpointer user_data)
{
	nautilus_entry_select_all (name_field);
	return TRUE;
}

static void
name_field_activate (NautilusEntry *name_field)
{
	g_assert (NAUTILUS_IS_ENTRY (name_field));

	/* Accept changes. */
	name_field_done_editing (name_field);

	nautilus_entry_select_all_at_idle (name_field);
}

static void
name_field_update_to_match_file (NautilusEntry *name_field)
{
	NautilusFile *file;
	char *original_name, *current_name;

	file = gtk_object_get_data (GTK_OBJECT (name_field), "nautilus_file");

	if (file == NULL || nautilus_file_is_gone (file)) {
		gtk_widget_set_sensitive (GTK_WIDGET (name_field), FALSE);
		gtk_entry_set_text (GTK_ENTRY (name_field), "");
		return;
	}

	original_name = (char *)gtk_object_get_data (GTK_OBJECT (name_field),
						     "original_name");

	/* If the file name has changed since the original name was stored,
	 * update the text in the text field, possibly (deliberately) clobbering
	 * an edit in progress. If the name hasn't changed (but some other
	 * aspect of the file might have), then don't clobber changes.
	 */
	current_name = nautilus_file_get_name (file);
	if (nautilus_strcmp (original_name, current_name) != 0) {
		gtk_object_set_data_full (GTK_OBJECT (name_field),
					  "original_name",
					  current_name,
					  g_free);

		/* Only reset the text if it's different from what is
		 * currently showing. This causes minimal ripples (e.g.
		 * selection change).
		 */
		if (strcmp (gtk_entry_get_text (GTK_ENTRY (name_field)), current_name) != 0) {
			gtk_entry_set_text (GTK_ENTRY (name_field), current_name);
		}
	} else {
		g_free (original_name);
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
			keywords = g_list_append (keywords, g_strdup (name));
		}
	} else {
		if (word != NULL) {
			keywords = g_list_remove_link (keywords, word);
		}
	}
	nautilus_file_set_keywords (file, keywords);
}

static void
update_properties_window_title (GtkWindow *window, NautilusFile *file)
{
	char *name, *title;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (GTK_IS_WINDOW (window));

	name = nautilus_file_get_name (file);
	title = g_strdup_printf (_("Nautilus: %s Properties"), name);
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
value_field_update (GtkLabel *label, NautilusFile *file)
{
	const char *attribute_name;
	char *attribute_value;

	g_assert (GTK_IS_LABEL (label));
	g_assert (NAUTILUS_IS_FILE (file));

	attribute_name = gtk_object_get_data (GTK_OBJECT (label), "file_attribute");
	attribute_value = nautilus_file_get_string_attribute (file, attribute_name);
	gtk_label_set_text (label, attribute_value);
	g_free (attribute_value);
}

static void
install_title_value_pair (GtkTable *table, 
			  int row, 
			  const char *title, 
			  NautilusFile *file, 
			  const char *file_attribute_name)
{
	GtkWidget *title_field, *value_field;

	title_field = gtk_label_new (title);
	/* Move widget to right edge (justifying text not the right thing here). */
	gtk_misc_set_alignment (GTK_MISC (title_field), 1, 0.5);
	gtk_widget_show (title_field);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   title_field,
				   BASIC_PAGE_TITLE_COLUMN, 
				   BASIC_PAGE_TITLE_COLUMN + 1,
				   row, 
				   row + 1);

	value_field = gtk_label_new ("");
	/* Move widget to left edge (justifying text not the right thing here). */
	gtk_misc_set_alignment (GTK_MISC (value_field), 0, 0.5);
	gtk_widget_show (value_field);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   value_field,
				   BASIC_PAGE_VALUE_COLUMN, 
				   BASIC_PAGE_VALUE_COLUMN + 1,
				   row, 
				   row + 1);

	/* Stash a copy of the file attribute name in this field for the callback's sake. */
	gtk_object_set_data_full (GTK_OBJECT (value_field),
				  "file_attribute",
				  g_strdup (file_attribute_name),
				  (GtkDestroyNotify) g_free);

	/* Fill in the value. */
	value_field_update (GTK_LABEL (value_field), file);

	/* Connect to signal to update value when file changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       value_field_update,
					       GTK_OBJECT (value_field));
			  
}

static void
create_basic_page (GtkNotebook *notebook, NautilusFile *file)
{
	GtkWidget *icon_pixmap_widget, *name_field;
	GtkWidget *basic_page_table;

	basic_page_table = gtk_table_new (BASIC_PAGE_ROW_COUNT, BASIC_PAGE_COLUMN_COUNT, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (basic_page_table), GNOME_PAD);
	gtk_table_set_col_spacings (GTK_TABLE (basic_page_table), GNOME_PAD);
	gtk_widget_show (basic_page_table);
	gtk_container_add (GTK_CONTAINER (notebook), basic_page_table);
	gtk_container_set_border_width (GTK_CONTAINER (basic_page_table), GNOME_PAD);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
				    gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0),
				    gtk_label_new (_("Basic")));


	/* Icon pixmap */
	icon_pixmap_widget = create_pixmap_widget_for_file (file);
	gtk_widget_show (icon_pixmap_widget);
	gtk_table_attach_defaults (GTK_TABLE (basic_page_table),
				   icon_pixmap_widget,
				   BASIC_PAGE_TITLE_COLUMN, 
				   BASIC_PAGE_TITLE_COLUMN + 1,
				   BASIC_PAGE_ICON_AND_NAME_ROW, 
				   BASIC_PAGE_ICON_AND_NAME_ROW + 1);

	/* Name field */
	name_field = nautilus_entry_new ();
	gtk_widget_show (name_field);
	gtk_table_attach_defaults (GTK_TABLE (basic_page_table),
				   name_field,
				   BASIC_PAGE_VALUE_COLUMN, 
				   BASIC_PAGE_VALUE_COLUMN + 1,
				   BASIC_PAGE_ICON_AND_NAME_ROW, 
				   BASIC_PAGE_ICON_AND_NAME_ROW + 1);

	/* Attach parameters and signal handler. */
	nautilus_file_ref (file);
	gtk_object_set_data_full (GTK_OBJECT (name_field),
				  "nautilus_file",
				  file,
				  (GtkDestroyNotify) nautilus_file_unref);

	/* Update name field initially before hooking up changed signal. */
	name_field_update_to_match_file (NAUTILUS_ENTRY (name_field));

	gtk_signal_connect (GTK_OBJECT (name_field), "focus_in_event",
      	              	    GTK_SIGNAL_FUNC (name_field_focus_in),
                            NULL);
                      			    
	gtk_signal_connect (GTK_OBJECT (name_field), "focus_out_event",
      	              	    GTK_SIGNAL_FUNC (name_field_focus_out),
                            NULL);
                      			    
	gtk_signal_connect (GTK_OBJECT (name_field), "activate",
      	              	    GTK_SIGNAL_FUNC (name_field_activate),
                            NULL);

        /* Start with name field selected. */
        gtk_widget_grab_focus (GTK_WIDGET (name_field));

        /* Enable undo in name field */
        nautilus_entry_enable_undo (NAUTILUS_ENTRY(name_field), TRUE);
        nautilus_entry_enable_undo_key (NAUTILUS_ENTRY(name_field), TRUE);
                      			    
	/* React to name changes from elsewhere. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       name_field_update_to_match_file,
					       GTK_OBJECT (name_field));

	/* File uri */
	install_title_value_pair (GTK_TABLE (basic_page_table), BASIC_PAGE_LOCATION_ROW,
				  _("Location:"), file, "parent_uri");
	
	/* File type */
	install_title_value_pair (GTK_TABLE (basic_page_table), BASIC_PAGE_TYPE_ROW,
				  _("Type:"), file, "type");
	
	/* File size */
	install_title_value_pair (GTK_TABLE (basic_page_table), BASIC_PAGE_SIZE_ROW,
				  _("Size:"), file, "size");
	
	/* File date */
	install_title_value_pair (GTK_TABLE (basic_page_table), BASIC_PAGE_DATE_ROW,
				  _("Date Modified:"), file, "date_modified");
}

static void
create_emblems_page (GtkNotebook *notebook, NautilusFile *file)
{
	GtkWidget *emblems_page_vbox, *prompt, *separator_line, *button;
	GtkWidget *check_buttons_box, *left_buttons_box, *right_buttons_box;
	int i;

	emblems_page_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (emblems_page_vbox);
	gtk_container_add (GTK_CONTAINER (notebook), emblems_page_vbox);
	gtk_container_set_border_width (GTK_CONTAINER (emblems_page_vbox), GNOME_PAD);
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
				    gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1),
				    gtk_label_new (_("Emblems")));
	

	/* The prompt. */
	prompt = gtk_label_new (_("This is a placeholder for the final emblems design. For now, you can turn some hard-wired properties with corresponding emblems on and off."));
	gtk_widget_show (prompt);
	gtk_box_pack_start (GTK_BOX (emblems_page_vbox), prompt, FALSE, FALSE, 0);
   	gtk_label_set_justify (GTK_LABEL (prompt), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (prompt), TRUE);

	/* Separator between prompt and check buttons. */
  	separator_line = gtk_hseparator_new ();
  	gtk_widget_show (separator_line);
  	gtk_box_pack_start (GTK_BOX (emblems_page_vbox), separator_line, TRUE, TRUE, 8);

	/* Holder for two columns of check buttons. */
	check_buttons_box = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (check_buttons_box);
	gtk_box_pack_start (GTK_BOX (emblems_page_vbox), check_buttons_box, FALSE, FALSE, 0);

	/* Left column of check buttons. */
	left_buttons_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (left_buttons_box);
	gtk_box_pack_start (GTK_BOX (check_buttons_box), left_buttons_box, FALSE, FALSE, 0);

	/* Right column of check buttons. */
	right_buttons_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (right_buttons_box);
	gtk_box_pack_start (GTK_BOX (check_buttons_box), right_buttons_box, FALSE, FALSE, 0);

	/* The check buttons themselves. */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (property_names); i++) {
		button = gtk_check_button_new_with_label (_(property_names[i]));
		gtk_widget_show (button);

		/* Attach parameters and signal handler. */
		gtk_object_set_data (GTK_OBJECT (button),
				     "nautilus_property_name",
				     (char *) property_names[i]);
		nautilus_file_ref (file);
		gtk_object_set_data_full (GTK_OBJECT (button),
					  "nautilus_file",
					  file,
					  (GtkDestroyNotify) nautilus_file_unref);
		gtk_signal_connect (GTK_OBJECT (button),
				    "toggled",
				    property_button_toggled,
				    NULL);

		property_button_update (GTK_TOGGLE_BUTTON (button));

		if (i < NAUTILUS_N_ELEMENTS (property_names) / 2) {
			gtk_box_pack_start (GTK_BOX (left_buttons_box), button, FALSE, FALSE, 0);
		} else {
			gtk_box_pack_start (GTK_BOX (right_buttons_box), button, FALSE, FALSE, 0);
		}
	}
}

static GtkWindow *
create_properties_window (NautilusFile *file)
{
	GtkWindow *window;
	GtkWidget *notebook;

	/* Create the window. */
	window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (window, FALSE, FALSE, FALSE);

	/* Set initial window title */
	update_properties_window_title (window, file);

	/* React to future name changes and file deletions. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       properties_window_file_changed_callback,
					       GTK_OBJECT (window));

	/* Create the notebook tabs. */
	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_container_add (GTK_CONTAINER (window), notebook);

	/* Create the pages. */
	create_basic_page (GTK_NOTEBOOK (notebook), file);
	create_emblems_page (GTK_NOTEBOOK (notebook), file);

	return window;
}

static void
remove_properties_window_from_hash_table (gpointer data, gpointer user_data)
{
	NautilusFile *file;

	g_assert (GTK_IS_WINDOW (data));
	g_assert (NAUTILUS_IS_FILE (user_data));

	file = NAUTILUS_FILE (user_data);
	g_hash_table_remove (windows, file);
	nautilus_file_unref (file);
}

GtkWindow *
fm_properties_window_get_or_create (NautilusFile *file)
{
	GtkWindow *window;

	/* Create the hash table first time through. */
	if (windows == NULL) {
		windows = g_hash_table_new (g_direct_hash, g_direct_equal);
	}

	/* Look to see if object is already in the hash table. */
	window = g_hash_table_lookup (windows, file);

	if (window == NULL) {
		window = create_properties_window (file);
		nautilus_file_ref (file);
		g_hash_table_insert (windows, file, window);
	
		gtk_signal_connect (GTK_OBJECT (window),
				    "destroy",
				    remove_properties_window_from_hash_table,
				    file);
	}

	return window;
}
