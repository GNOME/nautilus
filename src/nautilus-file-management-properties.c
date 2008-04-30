/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-management-properties.c - Functions to create and show the nautilus preference dialog.

   Copyright (C) 2002 Jan Arne Petersen

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

   Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
*/

#include <config.h>

#include "nautilus-file-management-properties.h"

#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <libgnome/gnome-help.h>
#include <glib/gi18n.h>

#include <glade/glade.h>

#include <eel/eel-gconf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-preferences-glade.h>

#include <libnautilus-private/nautilus-column-chooser.h>
#include <libnautilus-private/nautilus-column-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-module.h>

#include <libnautilus-private/nautilus-autorun.h>

/* string enum preferences */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET "default_view_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET "icon_view_zoom_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_COMPACT_VIEW_ZOOM_WIDGET "compact_view_zoom_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET "list_view_zoom_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET "sort_order_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DATE_FORMAT_WIDGET "date_format_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_TEXT_WIDGET "preview_text_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_IMAGE_WIDGET "preview_image_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_SOUND_WIDGET "preview_sound_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET "preview_folder_combobox"

/* bool preferences */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET "sort_folders_first_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_COMPACT_LAYOUT_WIDGET "compact_layout_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LABELS_BESIDE_ICONS_WIDGET "labels_beside_icons_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ALL_COLUMNS_SAME_WIDTH "all_columns_same_width_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ALWAYS_USE_BROWSER_WIDGET "always_use_browser_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ALWAYS_USE_LOCATION_ENTRY_WIDGET "always_use_location_entry_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET "trash_confirm_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET "trash_delete_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_OPEN_NEW_WINDOW_WIDGET "new_window_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET "hidden_files_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TREE_VIEW_FOLDERS_WIDGET "treeview_folders_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTOMOUNT_OPEN "media_automount_open_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTORUN_NEVER "media_autorun_never_checkbutton"

/* int enums */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET "preview_image_size_combobox"

static const char * const default_view_values[] = {
	"icon_view",
	"compact_view",
	"list_view",
	NULL
};

static const char * const zoom_values[] = {
	"smallest",
	"smaller",
	"small",
	"standard",
	"large",
	"larger",
	"largest",
	NULL
};

static const char * const sort_order_values[] = {
	"name",
	"size",
	"type",
	"modification_date",
	"emblems",
	NULL
};

static const char * const date_format_values[] = {
	"locale",
	"iso",
	"informal",
	NULL
};

static const char * const preview_values[] = {
	"always",
	"local_only",
	"never",
	NULL
};

static const char * const click_behavior_components[] = {
	"single_click_radiobutton",
	"double_click_radiobutton",
	NULL
};

static const char * const click_behavior_values[] = {
	"single",
	"double",
	NULL
};

static const char * const executable_text_components[] = {
	"scripts_execute_radiobutton",
	"scripts_view_radiobutton",
	"scripts_confirm_radiobutton",
	NULL
};

static const char * const executable_text_values[] = {
	"launch",
	"display",
	"ask",
	NULL
};

static const int thumbnail_limit_values[] = {
	102400,
	512000,
	1048576,
	3145728,
	5242880,
	10485760,
	104857600,
	1073741824,
	-1
};

static const char * const icon_captions_components[] = {
	"captions_0_combobox",
	"captions_1_combobox",
	"captions_2_combobox",
	NULL
};

static void
nautilus_file_management_properties_size_group_create (GladeXML *xml_dialog,
						       char *prefix,
						       int items)
{
	GtkSizeGroup *size_group;
	int i;
	char *item_name;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	for (i = 0; i < items; i++) {	
		item_name = g_strdup_printf ("%s_%d", prefix, i);
		gtk_size_group_add_widget (size_group,
					   glade_xml_get_widget (xml_dialog, item_name));
		g_free (item_name);
	}
	g_object_unref (G_OBJECT (size_group));
}

static void
preferences_show_help (GtkWindow *parent,
		       char const *helpfile,
		       char const *sect_id)
{
	GError *error = NULL;
	GtkWidget *dialog;

	g_assert (helpfile != NULL);
	g_assert (sect_id != NULL);

	gnome_help_display_desktop (NULL,
				    "user-guide",
				    helpfile, sect_id, &error);

	if (error) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("There was an error displaying help: \n%s"),
						 error->message);

		g_signal_connect (G_OBJECT (dialog),
				  "response", G_CALLBACK (gtk_widget_destroy),
				  NULL);
		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}


static void
nautilus_file_management_properties_dialog_response_cb (GtkDialog *parent,
							int response_id,
							GladeXML *xml_dialog)
{
	char *section;

	if (response_id == GTK_RESPONSE_HELP) {
		switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (glade_xml_get_widget (xml_dialog, "notebook1")))) {
		default:
		case 0:
			section = "gosnautilus-438";
			break;
		case 1:
			section = "gosnautilus-56";
			break;
		case 2:
			section = "gosnautilus-439";
			break;
		case 3:
			section = "gosnautilus-490";
			break;
		case 4:
			section = "gosnautilus-60";
		}
		preferences_show_help (GTK_WINDOW (parent), "user-guide.xml", section);
	} else if (response_id == GTK_RESPONSE_CLOSE) {
		/* remove gconf monitors */
		eel_gconf_monitor_remove ("/apps/nautilus/icon_view");
		eel_gconf_monitor_remove ("/apps/nautilus/list_view");
		eel_gconf_monitor_remove ("/apps/nautilus/preferences");
		eel_gconf_monitor_remove ("/desktop/gnome/file_views");
	}
}

static void
columns_changed_callback (NautilusColumnChooser *chooser,
			  gpointer callback_data)
{
	char **visible_columns;
	char **column_order;

	nautilus_column_chooser_get_settings (NAUTILUS_COLUMN_CHOOSER (chooser),
					      &visible_columns,
					      &column_order);

	eel_preferences_set_string_array (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS, visible_columns);
	eel_preferences_set_string_array (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER, column_order);

	g_strfreev (visible_columns);
	g_strfreev (column_order);
}

static void
free_column_names_array (GPtrArray *column_names)
{
	g_ptr_array_foreach (column_names, (GFunc) g_free, NULL);
	g_ptr_array_free (column_names, TRUE);
}

static void
create_icon_caption_combo_box_items (GtkComboBox *combo_box,
			             GList *columns)
{
	GList *l;
	GPtrArray *column_names;

	column_names = g_ptr_array_new ();

	gtk_combo_box_append_text (combo_box, _("None"));
	g_ptr_array_add (column_names, g_strdup ("none"));

	for (l = columns; l != NULL; l = l->next) {
		NautilusColumn *column;
		char *name;
		char *label;

		column = NAUTILUS_COLUMN (l->data);

		g_object_get (G_OBJECT (column), 
			      "name", &name, "label", &label, 
			      NULL);

		/* Don't show name here, it doesn't make sense */
		if (!strcmp (name, "name")) {
			g_free (name);
			g_free (label);
			continue;
		}

		gtk_combo_box_append_text (combo_box, label);
		g_ptr_array_add (column_names, name);

		g_free (label);
	}
	g_object_set_data_full (G_OBJECT (combo_box), "column_names",
			        column_names,
			        (GDestroyNotify) free_column_names_array);
}

static void
icon_captions_changed_callback (GtkComboBox *combo_box,
				gpointer user_data)
{
	GPtrArray *captions;
	GladeXML *xml;
	int i;
	
	xml = GLADE_XML (user_data);

	captions = g_ptr_array_new ();

	for (i = 0; icon_captions_components[i] != NULL; i++) {
		GtkWidget *combo_box;
		int active;
		GPtrArray *column_names;
		char *name;

		combo_box = glade_xml_get_widget
			    (GLADE_XML (xml), icon_captions_components[i]);
		active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

		column_names = g_object_get_data (G_OBJECT (combo_box),
						  "column_names");

		name = g_ptr_array_index (column_names, active);
		g_ptr_array_add (captions, name);
	}
	g_ptr_array_add (captions, NULL);

	eel_preferences_set_string_array (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
					  (char **)captions->pdata);
	g_ptr_array_free (captions, TRUE);
}

static void
update_caption_combo_box (GladeXML *xml,
			  const char *combo_box_name,
			  const char *name)
{
	GtkWidget *combo_box;
	int i;
	GPtrArray *column_names;

	combo_box = glade_xml_get_widget (xml, combo_box_name);

	g_signal_handlers_block_by_func
		(combo_box,
		 G_CALLBACK (icon_captions_changed_callback),
		 xml);

	column_names = g_object_get_data (G_OBJECT (combo_box), 
					  "column_names");

	for (i = 0; i < column_names->len; ++i) {
		if (!strcmp (name, g_ptr_array_index (column_names, i))) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), i);
			break;
		}
	}

	g_signal_handlers_unblock_by_func
		(combo_box,
		 G_CALLBACK (icon_captions_changed_callback),
		 xml);
}

static void
update_icon_captions_from_gconf (GladeXML *xml)
{
	char **captions;
	int i, j;

	captions = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);
	if (captions == NULL)
		return;

	for (i = 0, j = 0; 
	     icon_captions_components[i] != NULL;
	     i++) {
		char *data;

		if (captions[j]) {
			data = captions[j];
			++j;
		} else {
			data = "none";
		}

		update_caption_combo_box (xml, 
					  icon_captions_components[i],
					  data);
	}

	g_strfreev (captions);
}

static void
nautilus_file_management_properties_dialog_setup_icon_caption_page (GladeXML *xml_dialog)
{
	GList *columns;
	int i;
	gboolean writable;
	
	writable = eel_preferences_key_is_writable (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

	columns = nautilus_get_all_columns ();
	
	for (i = 0; icon_captions_components[i] != NULL; i++) {
		GtkWidget *combo_box;
		
		combo_box = glade_xml_get_widget (xml_dialog, 
						    icon_captions_components[i]);

		create_icon_caption_combo_box_items (GTK_COMBO_BOX (combo_box), columns);
		gtk_widget_set_sensitive (combo_box, writable);

		g_signal_connect (combo_box, "changed", 
				  G_CALLBACK (icon_captions_changed_callback),
				  xml_dialog);
	}

	nautilus_column_list_free (columns);

	update_icon_captions_from_gconf (xml_dialog);
}

static void
create_date_format_menu (GladeXML *xml_dialog)
{
	GtkWidget *combo_box;
	gchar *date_string;
	time_t now_raw;
	struct tm* now;
	combo_box = glade_xml_get_widget (xml_dialog,
					  NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DATE_FORMAT_WIDGET);

	now_raw = time (NULL);
	now = localtime (&now_raw);

	date_string = eel_strdup_strftime ("%c", now);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box), date_string);
	g_free (date_string);

	date_string = eel_strdup_strftime ("%Y-%m-%d %H:%M:%S", now);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box), date_string);
	g_free (date_string);

	date_string = eel_strdup_strftime (_("today at %-I:%M:%S %p"), now);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box), date_string);
	g_free (date_string);
}

static void
set_columns_from_gconf (NautilusColumnChooser *chooser)
{
	char **visible_columns;
	char **column_order;
	
	visible_columns = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
	column_order = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

	nautilus_column_chooser_set_settings (NAUTILUS_COLUMN_CHOOSER (chooser), 
					      visible_columns,
					      column_order);

	g_strfreev (visible_columns);
	g_strfreev (column_order);
}

static void 
use_default_callback (NautilusColumnChooser *chooser, 
		      gpointer user_data)
{
	eel_preferences_unset (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
	eel_preferences_unset (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
	set_columns_from_gconf (chooser);
}

static void
nautilus_file_management_properties_dialog_setup_list_column_page (GladeXML *xml_dialog)
{
	GtkWidget *chooser;
	GtkWidget *box;
	
	chooser = nautilus_column_chooser_new ();
	g_signal_connect (chooser, "changed", 
			  G_CALLBACK (columns_changed_callback), chooser);
	g_signal_connect (chooser, "use_default", 
			  G_CALLBACK (use_default_callback), chooser);

	set_columns_from_gconf (NAUTILUS_COLUMN_CHOOSER (chooser));

	gtk_widget_show (chooser);
	box = glade_xml_get_widget (xml_dialog, "list_columns_vbox");
	
	gtk_box_pack_start (GTK_BOX (box), chooser, TRUE, TRUE, 0);
}

static void
nautilus_file_management_properties_dialog_update_media_sensitivity (GladeXML *xml_dialog)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (xml_dialog, "media_handling_vbox"), 
				  ! eel_preferences_get_boolean (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_NEVER));
}

static void 
other_type_combo_box_changed (GtkComboBox *combo_box, GtkComboBox *action_combo_box)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	char *x_content_type;

	x_content_type = NULL;

	if (!gtk_combo_box_get_active_iter (combo_box, &iter)) {
		goto out;
	}

	model = gtk_combo_box_get_model (combo_box);
	if (model == NULL) {
		goto out;
	}

	gtk_tree_model_get (model, &iter, 
			    2, &x_content_type,
			    -1);

	nautilus_autorun_prepare_combo_box (GTK_WIDGET (action_combo_box),
					    x_content_type,
					    TRUE,
					    TRUE,
					    NULL, NULL);
out:
	g_free (x_content_type);
}


static void
nautilus_file_management_properties_dialog_setup_media_page (GladeXML *xml_dialog)
{
	unsigned int n;
	GList *l;
	GList *content_types;
	GtkWidget *other_type_combo_box;
	GtkListStore *other_type_list_store;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;
	const char *s[] = {"media_audio_cdda_combobox",   "x-content/audio-cdda",
			   "media_video_dvd_combobox",    "x-content/video-dvd",
			   "media_music_player_combobox", "x-content/audio-player",
			   "media_dcf_combobox",          "x-content/image-dcf",
			   "media_software_combobox",     "x-content/software",
			   NULL};

	for (n = 0; s[n*2] != NULL; n++) {
		nautilus_autorun_prepare_combo_box (glade_xml_get_widget (xml_dialog, s[n*2]), s[n*2 + 1],
						    TRUE, TRUE, NULL, NULL); 
	}

	other_type_combo_box = glade_xml_get_widget (xml_dialog, "media_other_type_combobox");

	other_type_list_store = gtk_list_store_new (3, 
						    GDK_TYPE_PIXBUF, 
						    G_TYPE_STRING, 
						    G_TYPE_STRING);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (other_type_list_store),
					      1, GTK_SORT_ASCENDING);


	content_types = g_content_types_get_registered ();

	for (l = content_types; l != NULL; l = l->next) {
		char *content_type = l->data;
		char *description;
		GIcon *icon;
		NautilusIconInfo *icon_info;
		GdkPixbuf *pixbuf;
		int icon_size;

		if (!g_str_has_prefix (content_type, "x-content/"))
			continue;
		for (n = 0; s[n*2] != NULL; n++) {
			if (strcmp (content_type, s[n*2 + 1]) == 0) {
				goto skip;
			}
		}

		icon_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);

		description = g_content_type_get_description (content_type);
		gtk_list_store_append (other_type_list_store, &iter);
		icon = g_content_type_get_icon (content_type);
		if (icon != NULL) {
			icon_info = nautilus_icon_info_lookup (icon, icon_size);
			g_object_unref (icon);
			pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (icon_info, icon_size);
			g_object_unref (icon_info);
		} else {
			pixbuf = NULL;
		}

		gtk_list_store_set (other_type_list_store, &iter, 
				    0, pixbuf, 
				    1, description, 
				    2, content_type, 
				    -1);
		if (pixbuf != NULL)
			g_object_unref (pixbuf);
		g_free (description);
	skip:
		;
	}
	g_list_foreach (content_types, (GFunc) g_free, NULL);
	g_list_free (content_types);

	gtk_combo_box_set_model (GTK_COMBO_BOX (other_type_combo_box), GTK_TREE_MODEL (other_type_list_store));
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
					"pixbuf", 0,
					NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
					"text", 1,
					NULL);

	g_signal_connect (G_OBJECT (other_type_combo_box),
			  "changed",
			  G_CALLBACK (other_type_combo_box_changed),
			  glade_xml_get_widget (xml_dialog, "media_other_action_combobox"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (other_type_combo_box), 0);

	nautilus_file_management_properties_dialog_update_media_sensitivity (xml_dialog);
}

static  void
nautilus_file_management_properties_dialog_setup (GladeXML *xml_dialog, GtkWindow *window)
{
	GtkWidget *dialog;

	/* setup gconf stuff */
	eel_gconf_monitor_add ("/apps/nautilus/icon_view");
	eel_gconf_preload_cache ("/apps/nautilus/icon_view", GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_monitor_add ("/apps/nautilus/compact_view");
	eel_gconf_preload_cache ("/apps/nautilus/compact_view", GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_monitor_add ("/apps/nautilus/list_view");
	eel_gconf_preload_cache ("/apps/nautilus/list_view", GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_monitor_add ("/apps/nautilus/preferences");
	eel_gconf_preload_cache ("/apps/nautilus/preferences", GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_monitor_add ("/desktop/gnome/file_views");
	eel_gconf_preload_cache ("/desktop/gnome/file_views", GCONF_CLIENT_PRELOAD_ONELEVEL);

	/* setup UI */
	nautilus_file_management_properties_size_group_create (xml_dialog, 
							       "views_label",
							       5);
	nautilus_file_management_properties_size_group_create (xml_dialog,
							       "captions_label",
							       3);
	nautilus_file_management_properties_size_group_create (xml_dialog,
							       "preview_label",
							       5);
	create_date_format_menu (xml_dialog);

	/* setup preferences */
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_COMPACT_LAYOUT_WIDGET,
					    NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LABELS_BESIDE_ICONS_WIDGET,
					    NAUTILUS_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ALL_COLUMNS_SAME_WIDTH,
					    NAUTILUS_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET,
					    NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST); 
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ALWAYS_USE_BROWSER_WIDGET,
					    NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER);

	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTOMOUNT_OPEN,
					    NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT_OPEN);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTORUN_NEVER,
					    NAUTILUS_PREFERENCES_MEDIA_AUTORUN_NEVER);

	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET,
					    NAUTILUS_PREFERENCES_CONFIRM_TRASH);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET,
					    NAUTILUS_PREFERENCES_ENABLE_DELETE);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET,
					    NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
	eel_preferences_glade_connect_bool_slave (xml_dialog,
						  NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET,
						  NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TREE_VIEW_FOLDERS_WIDGET,
					    NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES);

	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET,
							     NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
							     (const char **) default_view_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET,						     
							     NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
							     (const char **) zoom_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_COMPACT_VIEW_ZOOM_WIDGET,
							     NAUTILUS_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL,
							     (const char **) zoom_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET,
							     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
							     (const char **) zoom_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET,
							     NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
							     (const char **) sort_order_values);
	eel_preferences_glade_connect_string_enum_combo_box_slave (xml_dialog,
								   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET,
								   NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_TEXT_WIDGET,
							     NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
							     (const char **) preview_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_IMAGE_WIDGET,
							     NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
							     (const char **) preview_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_SOUND_WIDGET,
							     NAUTILUS_PREFERENCES_PREVIEW_SOUND,
							     (const char **) preview_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET,
							     NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
							     (const char **) preview_values);
	eel_preferences_glade_connect_string_enum_combo_box (xml_dialog,
							     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DATE_FORMAT_WIDGET,
							     NAUTILUS_PREFERENCES_DATE_FORMAT,
							     (const char **) date_format_values);

	eel_preferences_glade_connect_string_enum_radio_button (xml_dialog,
								(const char **) click_behavior_components,
								NAUTILUS_PREFERENCES_CLICK_POLICY,
								(const char **) click_behavior_values);
	eel_preferences_glade_connect_string_enum_radio_button (xml_dialog,
								(const char **) executable_text_components,
								NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
								(const char **) executable_text_values);

	eel_preferences_glade_connect_int_enum (xml_dialog,
						NAUTILUS_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET,
						NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
						(const int *) thumbnail_limit_values);


	nautilus_file_management_properties_dialog_setup_icon_caption_page (xml_dialog);
	nautilus_file_management_properties_dialog_setup_list_column_page (xml_dialog);
	nautilus_file_management_properties_dialog_setup_media_page (xml_dialog);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_NEVER,
				      (EelPreferencesCallback ) nautilus_file_management_properties_dialog_update_media_sensitivity,
				      g_object_ref (xml_dialog));

	
	/* UI callbacks */
	dialog = glade_xml_get_widget (xml_dialog, "file_management_dialog");
	g_signal_connect_data (G_OBJECT (dialog), "response",
			       G_CALLBACK (nautilus_file_management_properties_dialog_response_cb),
			       g_object_ref (xml_dialog),
			       (GClosureNotify)g_object_unref,
			       0);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "file-manager");

	if (window) {
		gtk_window_set_screen (GTK_WINDOW (dialog), gtk_window_get_screen(window));
	}

	gtk_widget_show (dialog);
}

static gboolean
delete_event_callback (GtkWidget       *widget,
		       GdkEventAny     *event,
		       gpointer         data)
{
	void (*response_callback) (GtkDialog *dialog,
				   gint response_id);

	response_callback = data;

	response_callback (GTK_DIALOG (widget), GTK_RESPONSE_CLOSE);
	
	return TRUE;
}

void
nautilus_file_management_properties_dialog_show (GCallback close_callback, GtkWindow *window)
{
	GladeXML *xml_dialog;

	xml_dialog = glade_xml_new (GLADEDIR "/nautilus-file-management-properties.glade",
				    NULL, NULL);
	
	g_signal_connect (G_OBJECT (glade_xml_get_widget (xml_dialog, "file_management_dialog")),
			  "response", close_callback, NULL);
	g_signal_connect (G_OBJECT (glade_xml_get_widget (xml_dialog, "file_management_dialog")),
			  "delete_event", G_CALLBACK (delete_event_callback), close_callback);

	nautilus_file_management_properties_dialog_setup (xml_dialog, window);

	g_object_unref (xml_dialog);
}
