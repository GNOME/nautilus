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

#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtksizegroup.h>
#include <gtk/gtknotebook.h>

#include <libgnome/gnome-help.h>
#include <libgnome/gnome-i18n.h>

#include <glade/glade.h>

#include <eel/eel-gconf-extensions.h>
#include <eel/eel-preferences-glade.h>

#include <libnautilus-private/nautilus-global-preferences.h>

/* string enum preferences */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET "default_view_optionmenu"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET "iconview_zoom_optionmenu"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET "listview_zoom_optionmenu"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET "sort_order_optionmenu"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_TEXT_WIDGET "preview_text_optionmenu"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_IMAGE_WIDGET "preview_image_optionmenu"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_SOUND_WIDGET "preview_sound_optionmenu"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET "preview_folder_optionmenu"

/* bool preferences */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET "sort_folders_first_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_COMPACT_LAYOUT_WIDGET "compact_layout_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET "trash_confirm_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET "trash_delete_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_OPEN_NEW_WINDOW_WIDGET "new_window_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_MANUAL_LAYOUT_WIDGET "manual_layout_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET "hidden_files_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TREE_VIEW_FOLDERS_WIDGET "treeview_folders_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_REVERSE_WIDGET "sort_reverse_checkbutton"

/* int enums */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET "preview_image_size_optionmenu"

static const char *default_view_values[] = {
	"icon_view",
	"list_view",
	NULL
};

static const char *zoom_values[] = {
	"smallest",
	"smaller",
	"small",
	"standard",
	"large",
	"larger",
	"largest",
	NULL
};

static const char *sort_order_values[] = {
	"name",
	"size",
	"type",
	"modification_date",
	"emblems",
	NULL
};

static const char *preview_values[] = {
	"always",
	"local_only",
	"never",
	NULL
};

static const char *click_behavior_components[] = {
	"single_click_radiobutton",
	"double_click_radiobutton",
	NULL
};

static const char *click_behavior_values[] = {
	"single",
	"double",
	NULL
};

static const char *executable_text_components[] = {
	"scripts_execute_radiobutton",
	"scripts_view_radiobutton",
	"scripts_confirm_radiobutton",
	NULL
};

static const char *executable_text_values[] = {
	"launch",
	"display",
	"ask",
	NULL
};

static int thumbnail_limit_values[] = {
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

static const char *icon_captions_components[] = {
	"captions_0_optionmenu",
	"captions_1_optionmenu",
	"captions_2_optionmenu",
	NULL
};

static const char *icon_captions_values[] = {
	"size",
	"type",
	"date_modified",
	"date_accessed",
	"owner",
	"group",
	"permissions",
	"octal_permissions",
	"mime_type",
	"none",
	NULL
};

static GladeXML *
nautilus_file_management_properties_dialog_create (void)
{
	GladeXML *xml_dialog;

	xml_dialog = glade_xml_new (GLADEDIR "/nautilus-file-management-properties.glade",
				    NULL, NULL);

	return xml_dialog;
}

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
}

static void
nautilus_file_management_properties_dialog_set_icons (GtkWindow *window)
{
	GList *icon_list;
	GList *l;
	guint i;
	GdkPixbuf *pixbuf;
	char *path;
	const char *icon_filenames[] = { "nautilus-file-management-properties.png" };
	
	icon_list = NULL;
	for (i = 0; i < G_N_ELEMENTS (icon_filenames); i++) {
		path = g_build_filename (NAUTILUS_PIXMAPDIR, icon_filenames[i], NULL);
		pixbuf = gdk_pixbuf_new_from_file (path, NULL);
		g_free (path);
		if (pixbuf != NULL) {
			icon_list = g_list_prepend (icon_list, pixbuf);
		}
	}

	gtk_window_set_icon_list (window, icon_list);
	
	for (l = icon_list; l != NULL; l = l->next) {
		g_object_unref (G_OBJECT (l->data));
	}
	g_list_free (icon_list);
}

static void
preferences_show_help (GtkWindow *parent,
		       char const *helpfile,
		       char const *sect_id)
{
	GError *error = NULL;
	GtkWidget *dialog;

	g_return_if_fail (helpfile != NULL);
	g_return_if_fail (sect_id != NULL);

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
			section = "gosnautilus-57";
			break;
		case 2:
			section = "gosnautilus-439";
			break;
		case 3:
			section = "gosnautilus-60";
			break;
		}
		preferences_show_help (GTK_WINDOW (parent), "wgosnautilus.xml", section);
	} else if (response_id == GTK_RESPONSE_CLOSE) {
		/* remove gconf monitors */
		eel_gconf_monitor_remove ("/apps/nautilus/icon_view");
		eel_gconf_monitor_remove ("/apps/nautilus/list_view");
		eel_gconf_monitor_remove ("/apps/nautilus/preferences");
		eel_gconf_monitor_remove ("/desktop/gnome/file_views");

		g_object_unref (xml_dialog);
	}
}

static  void
nautilus_file_management_properties_dialog_setup (GladeXML *xml_dialog, GtkWindow *window)
{
	GtkWidget *dialog;

	/* setup gconf stuff */
	eel_gconf_monitor_add ("/apps/nautilus/icon_view");
	eel_gconf_preload_cache ("/apps/nautilus/icon_view", GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_monitor_add ("/apps/nautilus/list_view");
	eel_gconf_preload_cache ("/apps/nautilus/list_view", GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_monitor_add ("/apps/nautilus/preferences");
	eel_gconf_preload_cache ("/apps/nautilus/preferences", GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_monitor_add ("/desktop/gnome/file_views");
	eel_gconf_preload_cache ("/desktop/gnome/file_views", GCONF_CLIENT_PRELOAD_ONELEVEL);

	/* setup UI */
	nautilus_file_management_properties_size_group_create (xml_dialog, 
							       "views_label",
							       4);
	nautilus_file_management_properties_size_group_create (xml_dialog,
							       "captions_label",
							       3);
	nautilus_file_management_properties_size_group_create (xml_dialog,
							       "preview_label",
							       5);

	/* setup preferences */
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_COMPACT_LAYOUT_WIDGET,
					    NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET,
					    NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST); 
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET,
					    NAUTILUS_PREFERENCES_CONFIRM_TRASH);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET,
					    NAUTILUS_PREFERENCES_ENABLE_DELETE);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_OPEN_NEW_WINDOW_WIDGET,
					    NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_MANUAL_LAYOUT_WIDGET,
					    NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET,
					    NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
	eel_preferences_glade_connect_bool_slave (xml_dialog,
						  NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET,
						  NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TREE_VIEW_FOLDERS_WIDGET,
					    NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES);
	eel_preferences_glade_connect_bool (xml_dialog,
					    NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_REVERSE_WIDGET,
					    NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER);
	eel_preferences_glade_connect_bool_slave (xml_dialog,
						  NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_REVERSE_WIDGET,
						  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER);

	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET,
							       NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
							       default_view_values);
	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET,						     
							       NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
							       zoom_values);
	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET,
							       NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
							       zoom_values);
	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET,
							       NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
							       sort_order_values);
	eel_preferences_glade_connect_string_enum_option_menu_slave (xml_dialog,
								     NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET,
								     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER);
	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_TEXT_WIDGET,
							       NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
							       preview_values);
	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_IMAGE_WIDGET,
							       NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
							       preview_values);
	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_SOUND_WIDGET,
							       NAUTILUS_PREFERENCES_PREVIEW_SOUND,
							       preview_values);
	eel_preferences_glade_connect_string_enum_option_menu (xml_dialog,
							       NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET,
							       NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
							       preview_values);

	eel_preferences_glade_connect_string_enum_radio_button (xml_dialog,
								click_behavior_components,
								NAUTILUS_PREFERENCES_CLICK_POLICY,
								click_behavior_values);
	eel_preferences_glade_connect_string_enum_radio_button (xml_dialog,
								executable_text_components,
								NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
								executable_text_values);

	eel_preferences_glade_connect_list_enum (xml_dialog,
						 icon_captions_components,
						 NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
						 icon_captions_values);

	
	eel_preferences_glade_connect_int_enum (xml_dialog,
						NAUTILUS_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET,
						NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
						thumbnail_limit_values);

	/* UI callbacks */
	dialog = glade_xml_get_widget (xml_dialog, "file_management_dialog");
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (nautilus_file_management_properties_dialog_response_cb),
			  xml_dialog);

	nautilus_file_management_properties_dialog_set_icons (GTK_WINDOW (dialog));

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

	xml_dialog = nautilus_file_management_properties_dialog_create ();
	
	g_signal_connect (G_OBJECT (glade_xml_get_widget (xml_dialog, "file_management_dialog")),
			  "response", close_callback, NULL);
	g_signal_connect (G_OBJECT (glade_xml_get_widget (xml_dialog, "file_management_dialog")),
			  "delete_event", G_CALLBACK (delete_event_callback), close_callback);

	nautilus_file_management_properties_dialog_setup (xml_dialog, window);
}
