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

#include <libgnomevfs/gnome-vfs.h>
#include <gnome.h>

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktable.h>
#include <gtk/gtkvbox.h>

#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-string.h>

static GHashTable *windows;

enum {
	BASIC_PAGE_ICON_AND_NAME_ROW,
	BASIC_PAGE_LOCATION_ROW,
	BASIC_PAGE_TYPE_ROW,
	BASIC_PAGE_SIZE_ROW,
	BASIC_PAGE_DATE_ROW,
	BASIC_PAGE_ROW_COUNT
};

enum {
	PERMISSIONS_PAGE_TITLE_ROW,
	PERMISSIONS_PAGE_OWNER_ROW,
	PERMISSIONS_PAGE_GROUP_ROW,
	PERMISSIONS_PAGE_OTHERS_ROW,
	PERMISSIONS_PAGE_INTENTIONALLY_BLANK_ROW,
	PERMISSIONS_PAGE_FULL_STRING_ROW,
	PERMISSIONS_PAGE_ROW_COUNT
};	

enum {
	PERMISSIONS_CHECKBOXES_READ_COLUMN,
	PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
	PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
	PERMISSIONS_CHECKBOXES_COLUMN_COUNT
};	

enum {
	MORE_PAGE_MIME_TYPE_ROW,
	MORE_PAGE_ROW_COUNT
};

enum {
	TITLE_COLUMN,
	VALUE_COLUMN,
	COLUMN_COUNT
};

static void name_field_update_to_match_file (NautilusEntry *name_field);

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

	/* Separator between check buttons and prompt. */
  	separator_line = gtk_hseparator_new ();
  	gtk_widget_show (separator_line);
  	gtk_box_pack_end (GTK_BOX (vbox), separator_line, TRUE, TRUE, GNOME_PAD_BIG);
}		   

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
rename_callback (NautilusFile *file, GnomeVFSResult result, gpointer callback_data)
{
	char *new_name;

	new_name = callback_data;

	/* Complain to user if rename failed. */
	fm_report_error_renaming_file (file, new_name, result);
	
	g_free (new_name);
}

static void
name_field_done_editing (NautilusEntry *name_field)
{
	NautilusFile *file;
	char *new_name;
	
	g_assert (NAUTILUS_IS_ENTRY (name_field));

	file = gtk_object_get_data (GTK_OBJECT (name_field), "nautilus_file");

	g_assert (NAUTILUS_IS_FILE (file));

	new_name = gtk_entry_get_text (GTK_ENTRY (name_field));

	/* Special case: silently revert text if new text is empty. */
	if (strlen (new_name) == 0) {
		name_field_update_to_match_file (NAUTILUS_ENTRY (name_field));
	} else {
		nautilus_file_rename (file, new_name,
				      rename_callback, g_strdup (new_name));
	}

	g_free (new_name);
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

	original_name = (char *) gtk_object_get_data (GTK_OBJECT (name_field),
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
install_value_field (GtkTable *table,
		     int row,
		     int start_column,
		     int column_span,
		     NautilusFile *file,
		     const char *file_attribute_name)
{
	GtkWidget *value_field;

	value_field = gtk_label_new ("");
	/* Move widget to left edge (justifying text not the right thing here). */
	gtk_misc_set_alignment (GTK_MISC (value_field), 0, 0.5);
	gtk_widget_show (value_field);
	gtk_table_attach (GTK_TABLE (table), value_field,
			  start_column, start_column + column_span,
			  row, row + 1,
			  GTK_FILL, 0,
			  0, 0);

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
install_title_value_pair (GtkTable *table, 
			  int row, 
			  const char *title, 
			  NautilusFile *file, 
			  const char *file_attribute_name)
{
	GtkWidget *title_field;

	title_field = gtk_label_new (title);
	/* Move widget to right edge (justifying text not the right thing here). */
	gtk_misc_set_alignment (GTK_MISC (title_field), 1, 0.5);
	gtk_widget_show (title_field);
	gtk_table_attach (GTK_TABLE (table), title_field,
			  TITLE_COLUMN, TITLE_COLUMN + 1,
			  row, row + 1,
			  GTK_FILL, 0,
			  0, 0);

	install_value_field (table, row, VALUE_COLUMN, 1, file, file_attribute_name); 
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
				GtkWidget **return_table, 
				GtkWidget **return_vbox)
{
	GtkWidget *table;
	GtkWidget *vbox;

	vbox = create_page_with_vbox (notebook, title);
	table = create_attribute_value_table (GTK_VBOX (vbox), row_count);

	if (return_table != NULL) {
		*return_table = table;
	}

	if (return_vbox != NULL) {
		*return_vbox = vbox;
	}
}			

static void
create_basic_page (GtkNotebook *notebook, NautilusFile *file)
{
	GtkWidget *table;
	GtkWidget *icon_pixmap_widget, *name_field;

	create_page_with_table_in_vbox (notebook, 
					_("Basic"), 
					BASIC_PAGE_ROW_COUNT, 
					&table, 
					NULL);

	/* Icon pixmap */
	icon_pixmap_widget = create_pixmap_widget_for_file (file);
	gtk_widget_show (icon_pixmap_widget);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   icon_pixmap_widget,
				   TITLE_COLUMN, 
				   TITLE_COLUMN + 1,
				   BASIC_PAGE_ICON_AND_NAME_ROW, 
				   BASIC_PAGE_ICON_AND_NAME_ROW + 1);

	/* Name field */
	name_field = nautilus_entry_new ();
	gtk_widget_show (name_field);
	gtk_table_attach_defaults (GTK_TABLE (table),
				   name_field,
				   VALUE_COLUMN, 
				   VALUE_COLUMN + 1,
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
                      			    
	/* React to name changes from elsewhere. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       name_field_update_to_match_file,
					       GTK_OBJECT (name_field));

	install_title_value_pair (GTK_TABLE (table), BASIC_PAGE_LOCATION_ROW,
				  _("Location:"), file, "parent_uri");
	install_title_value_pair (GTK_TABLE (table), BASIC_PAGE_TYPE_ROW,
				  _("Type:"), file, "type");
	install_title_value_pair (GTK_TABLE (table), BASIC_PAGE_SIZE_ROW,
				  _("Size:"), file, "size");
	install_title_value_pair (GTK_TABLE (table), BASIC_PAGE_DATE_ROW,
				  _("Date Modified:"), file, "date_modified");
}

static GtkWidget *
create_image_widget_for_emblem (const char *emblem_name)
{
	NautilusScalableIcon *icon;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *image_widget;

	icon = nautilus_icon_factory_get_emblem_icon_by_name (emblem_name);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(icon,
		 NAUTILUS_ICON_SIZE_STANDARD,
		 NAUTILUS_ICON_SIZE_STANDARD,
		 NAUTILUS_ICON_SIZE_STANDARD,
		 NAUTILUS_ICON_SIZE_STANDARD);
	nautilus_scalable_icon_unref (icon);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);
	gdk_pixbuf_unref (pixbuf);

	image_widget = gtk_pixmap_new (pixmap, mask);
	gtk_widget_show (image_widget);

	return image_widget;
}

static void
remove_default_viewport_shadow (GtkViewport *viewport)
{
	g_return_if_fail (GTK_IS_VIEWPORT (viewport));
	
	gtk_viewport_set_shadow_type (viewport, GTK_SHADOW_NONE);
}

/* utility routine to build the list of available property names */

static GList *
get_property_names_from_uri (const char *directory_uri, GList *property_list)
{
	char *keyword, *dot_pos;
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;
			
	result = gnome_vfs_directory_list_load (&list, directory_uri, GNOME_VFS_FILE_INFO_GETMIMETYPE, NULL, NULL);
	if (result != GNOME_VFS_OK) {
		return property_list;
	}
	
	for (current_file_info = gnome_vfs_directory_list_first(list); current_file_info != NULL; 
	    current_file_info = gnome_vfs_directory_list_next(list)) {
		if (nautilus_str_has_prefix(current_file_info->mime_type, "image/")) {
			keyword = g_strdup(current_file_info->name);
			
			/* strip image type suffix */
			dot_pos = strrchr(keyword, '.');
			if (dot_pos)
				*dot_pos = '\0';
			
			property_list = g_list_prepend(property_list, keyword);
		}
	}

	gnome_vfs_directory_list_destroy(list);	
	return property_list;
}

static GList *
get_property_names (void)
{
	char *directory_uri;
	GList *property_list;
	
	property_list = get_property_names_from_uri
		("file://" NAUTILUS_PREFIX "/share/nautilus/emblems", NULL);

	directory_uri = g_strdup_printf ("file://%s/emblems", nautilus_get_user_directory ());
	property_list = get_property_names_from_uri (directory_uri, property_list);
	g_free (directory_uri);

	return g_list_sort(property_list, (GCompareFunc) nautilus_strcmp);		
}

static void
create_emblems_page (GtkNotebook *notebook, NautilusFile *file)
{
	GList *property_names, *save_property_names;
	GtkWidget *emblems_table, *button, *scroller;
	GtkWidget *image_widget, *label, *image_and_label_table;
	int i, property_count;

	property_names = get_property_names();	
	save_property_names = property_names;
	property_count = g_list_length(property_names);
	
	emblems_table = gtk_table_new ((property_count + 1) / 2,
				       2,
				       TRUE);
	gtk_widget_show (emblems_table);
	gtk_container_set_border_width (GTK_CONTAINER (emblems_table), GNOME_PAD);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
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

	gtk_notebook_append_page (notebook, scroller, gtk_label_new (_("Emblems")));
	
	/* The check buttons themselves. */
	for (i = 0; i < property_count; i++) {
		button = gtk_check_button_new ();

		/* Make 3-column homogeneous table with 1/3 for image, 2/3 text.
		 * This allows text to line up nicely.
		 */
		image_and_label_table = gtk_table_new (1, 3, TRUE);
		gtk_widget_show (image_and_label_table);
		
		image_widget = create_image_widget_for_emblem (property_names->data);
		gtk_table_attach_defaults (GTK_TABLE (image_and_label_table), image_widget,
					   0, 1,
					   0, 1);
					
		label = gtk_label_new (_(property_names->data));
		/* Move label to left edge. */
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_widget_show (label);
		gtk_table_attach (GTK_TABLE (image_and_label_table), label,
				  1, 3,
				  0, 1,
				  GTK_FILL, 0,
				  0, 0);

		gtk_container_add (GTK_CONTAINER (button), image_and_label_table);
		gtk_widget_show (button);

		/* Attach parameters and signal handler. */
		gtk_object_set_data_full (GTK_OBJECT (button),
				     "nautilus_property_name",
				     g_strdup ((char *)property_names->data),
				     (GtkDestroyNotify) g_free);
				     
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

		if (i < property_count / 2) {
			gtk_table_attach_defaults (GTK_TABLE (emblems_table), button,
					  	   0, 1,
					  	   i, i+1);
		} else {
			gtk_table_attach_defaults (GTK_TABLE (emblems_table), button,
						   1, 2,
						   i - (property_count / 2),
						   i - (property_count / 2) + 1);
		}
		property_names = property_names->next;
	}
	nautilus_g_list_free_deep (save_property_names);
}

static void
permissions_label_update (GtkLabel *label, NautilusFile *file)
{
	const char *attribute_name;
	const char *pattern;
	char *attribute_value;
	char *label_text;

	g_assert (GTK_IS_LABEL (label));
	g_assert (NAUTILUS_IS_FILE (file));

	attribute_name = gtk_object_get_data (GTK_OBJECT (label), "file_attribute");
	pattern = gtk_object_get_data (GTK_OBJECT (label), "pattern");

	/* The pattern must be a plain string if there's no attribute,
	 * or a pattern with exactly one %s if there is an attribute.
	 */
	if (attribute_name == NULL) {
		gtk_label_set_text (label, pattern);
	} else {
		attribute_value = nautilus_file_get_string_attribute (file, attribute_name);
		label_text = g_strdup_printf (pattern, attribute_value);
		gtk_label_set_text (label, label_text);
		g_free (label_text);
		g_free (attribute_value);
	}
}

static void
add_permissions_row_label (GtkTable *table, 
			   NautilusFile *file, 
			   int row, 
			   const char *pattern, 
			   const char *attribute_name)
{
	GtkWidget *label;

	label = gtk_label_new ("");

	/* Move widget to right edge (justifying text not the right thing here). */
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	gtk_widget_show (label);
	
	gtk_table_attach (table, label,
			  0, 0 + 1,
			  row, row + 1,
			  GTK_FILL, 0,
			  0, 0);

	/* Stash copies of the pattern and file attribute name in the label 
	 * for the callback's sake. 
	 */
	if (attribute_name != NULL) {
		gtk_object_set_data_full (GTK_OBJECT (label),
					  "file_attribute",
					  g_strdup (attribute_name),
					  (GtkDestroyNotify) g_free);
	}
	gtk_object_set_data_full (GTK_OBJECT (label),
				  "pattern",
				  g_strdup (pattern),
				  (GtkDestroyNotify) g_free);

	/* Fill in the value. */
	permissions_label_update (GTK_LABEL (label), file);

	/* Connect to signal to update value when file changes, if
	 * there's an attribute that might change. 
	 */
	if (attribute_name != NULL) {
		gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
						       "changed",
						       permissions_label_update,
						       GTK_OBJECT (label));
	}
}			   

static void
add_permissions_column_label (GtkTable *table, 
			      int column, 
			      const char *title_text)
{
	GtkWidget *label;

	label = gtk_label_new (title_text);

	/* Text is centered in table cell by default, which is what we want here. */
	gtk_widget_show (label);
	
	gtk_table_attach_defaults (table, label,
			  	   column, column + 1,
			  	   PERMISSIONS_PAGE_TITLE_ROW, 
			  	   PERMISSIONS_PAGE_TITLE_ROW + 1);
}	

static void
update_permissions_check_button_state (GtkWidget *check_button, NautilusFile *file)
{
	GnomeVFSFilePermissions file_permissions, permission;
	guint toggled_signal_id;

	g_assert (GTK_IS_CHECK_BUTTON (check_button));
	g_assert (NAUTILUS_IS_FILE (file));
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
	fm_report_error_setting_permissions (file, result);
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
add_permissions_checkbox (GtkTable *table, 
			  NautilusFile *file,
			  int row, int column, 
			  GnomeVFSFilePermissions permission_to_check)
{
	GtkWidget *check_button;
	guint toggled_signal_id;

	check_button = gtk_check_button_new ();
	gtk_widget_show (check_button);
	gtk_table_attach (table, check_button,
			  column, column + 1,
			  row, row + 1,
			  0, 0,
			  0, 0);

	toggled_signal_id = gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
      	              	    			GTK_SIGNAL_FUNC (permissions_check_button_toggled),
                            			file);

	/* Load up the check_button with data we'll need when updating its state. */
        gtk_object_set_data (GTK_OBJECT (check_button), 
        		     "toggled_signal_id", 
        		     GINT_TO_POINTER (toggled_signal_id));
        gtk_object_set_data (GTK_OBJECT (check_button), 
        		     "permission", 
        		     GINT_TO_POINTER (permission_to_check));
	
	/* Set initial state. */
        update_permissions_check_button_state (check_button, file);

        /* Update state later if file changes. */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       update_permissions_check_button_state,
					       GTK_OBJECT (check_button));
}

static void
create_permissions_page (GtkNotebook *notebook, NautilusFile *file)
{
	GtkWidget *vbox;
	GtkTable *row_label_table, *check_button_table;
	GtkWidget *check_buttons_hbox;
	char *file_name, *prompt_text;

	vbox = create_page_with_vbox (notebook, _("Permissions"));

	if (nautilus_file_can_get_permissions (file)) {
		if (!nautilus_file_can_set_permissions (file)) {
			add_prompt_and_separator (
				GTK_VBOX (vbox), 
				_("You are not the owner, so you can't change these permissions."));
		}

		check_buttons_hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_set_spacing (GTK_BOX (check_buttons_hbox), GNOME_PAD);
		gtk_widget_show (check_buttons_hbox);
		gtk_box_pack_start (GTK_BOX (vbox), 
				    GTK_WIDGET (check_buttons_hbox), 
				    FALSE, FALSE, 0);

		row_label_table = GTK_TABLE (gtk_table_new 
						(PERMISSIONS_PAGE_ROW_COUNT, 
						 1, TRUE));
		apply_standard_table_padding (row_label_table);
		gtk_widget_show (GTK_WIDGET (row_label_table));
		gtk_box_pack_start (GTK_BOX (check_buttons_hbox), 
				    GTK_WIDGET (row_label_table), 
				    FALSE, FALSE, 0);

		check_button_table = GTK_TABLE (gtk_table_new 
						   (PERMISSIONS_PAGE_ROW_COUNT, 
						    PERMISSIONS_CHECKBOXES_COLUMN_COUNT, 
						    TRUE));
		apply_standard_table_padding (check_button_table);
		gtk_widget_show (GTK_WIDGET (check_button_table));
		gtk_box_pack_start (GTK_BOX (check_buttons_hbox), 
				    GTK_WIDGET (check_button_table), 
				    FALSE, FALSE, 0);

		add_permissions_row_label (row_label_table, file, 
					   PERMISSIONS_PAGE_OWNER_ROW, 
					   _("owner (%s):"), "owner");
		add_permissions_row_label (row_label_table, file, 
					   PERMISSIONS_PAGE_GROUP_ROW, 
					   _("group (%s):"), "group");
		add_permissions_row_label (row_label_table, file, 
					   PERMISSIONS_PAGE_OTHERS_ROW, 
					   _("others:"), NULL);
		
		add_permissions_row_label (row_label_table, file, 
					   PERMISSIONS_PAGE_FULL_STRING_ROW, 
					   _("standard format:"), NULL);
		
		add_permissions_column_label (check_button_table, 
					      PERMISSIONS_CHECKBOXES_READ_COLUMN,
					      _("read"));

		add_permissions_column_label (check_button_table, 
					      PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					      _("write"));

		add_permissions_column_label (check_button_table, 
					      PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					      _("execute"));

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_USER_READ);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_USER_WRITE);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_OWNER_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_USER_EXEC);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_GROUP_READ);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_GROUP_WRITE);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_GROUP_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_GROUP_EXEC);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_READ_COLUMN,
					  GNOME_VFS_PERM_OTHER_READ);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_WRITE_COLUMN,
					  GNOME_VFS_PERM_OTHER_WRITE);

		add_permissions_checkbox (check_button_table, file, 
					  PERMISSIONS_PAGE_OTHERS_ROW,
					  PERMISSIONS_CHECKBOXES_EXECUTE_COLUMN,
					  GNOME_VFS_PERM_OTHER_EXEC);

		install_value_field (check_button_table, 
				     PERMISSIONS_PAGE_FULL_STRING_ROW, 
				     PERMISSIONS_CHECKBOXES_READ_COLUMN, 
				     PERMISSIONS_CHECKBOXES_COLUMN_COUNT - PERMISSIONS_CHECKBOXES_READ_COLUMN, 
				     file, "permissions"); 
	} else {
		file_name = nautilus_file_get_name (file);
		prompt_text = g_strdup_printf (_("The permissions of \"%s\" could not be determined."), file_name);
		g_free (file_name);
		add_prompt (GTK_VBOX (vbox), prompt_text, TRUE);
		g_free (prompt_text);
	}
}

static void
create_more_page (GtkNotebook *notebook, NautilusFile *file)
{
	GtkWidget *table;
	GtkWidget *vbox;

	create_page_with_table_in_vbox (notebook, 
					_("More"), 
					MORE_PAGE_ROW_COUNT, 
					&table, 
					&vbox);

	install_title_value_pair (GTK_TABLE (table), MORE_PAGE_MIME_TYPE_ROW,
				  _("MIME type:"), file, "mime_type");

	add_prompt_and_separator (
		GTK_VBOX (vbox),
		_("Tell sullivan@eazel.com all your good ideas for what should go here."));
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
	create_permissions_page (GTK_NOTEBOOK (notebook), file);
	create_more_page (GTK_NOTEBOOK (notebook), file);

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
