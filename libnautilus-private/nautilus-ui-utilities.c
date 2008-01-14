/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-ui-utilities.c - helper functions for GtkUIManager stuff

   Copyright (C) 2004 Red Hat, Inc.

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

   Authors: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-ui-utilities.h"
#include "nautilus-icon-info.h"
#include <gio/gio.h>

#include <gtk/gtkenums.h>
#include <eel/eel-debug.h>

void
nautilus_ui_unmerge_ui (GtkUIManager *ui_manager,
			guint *merge_id,
			GtkActionGroup **action_group)
{
	if (*merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  *merge_id);
		*merge_id = 0;
	}
	if (*action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    *action_group);
		*action_group = NULL;
	}
}
     
void
nautilus_ui_prepare_merge_ui (GtkUIManager *ui_manager,
			      const char *name,
			      guint *merge_id,
			      GtkActionGroup **action_group)
{
	*merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	*action_group = gtk_action_group_new (name);
	gtk_action_group_set_translation_domain (*action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (ui_manager, *action_group, 0);
	g_object_unref (*action_group); /* owned by ui manager */
}


char *
nautilus_get_ui_directory (void)
{
	return g_strdup (DATADIR "/nautilus/ui");
}

char *
nautilus_ui_file (const char *partial_path)
{
	char *path;

	path = g_build_filename (DATADIR "/nautilus/ui", partial_path, NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	}
	g_free (path);
	return NULL;
}

const char *
nautilus_ui_string_get (const char *filename)
{
	static GHashTable *ui_cache = NULL;
	char *ui;
	char *path;

	if (ui_cache == NULL) {
		ui_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
		eel_debug_call_at_shutdown_with_data ((GFreeFunc)g_hash_table_destroy, ui_cache);
	}

	ui = g_hash_table_lookup (ui_cache, filename);
	if (ui == NULL) {
		path = nautilus_ui_file (filename);
		if (path == NULL || !g_file_get_contents (path, &ui, NULL, NULL)) {
			g_warning ("Unable to load ui file %s\n", filename); 
		} 
		g_free (path);
		g_hash_table_insert (ui_cache,
				     g_strdup (filename),
				     ui);
	}
	
	return ui;
}

static void
extension_action_callback (GtkAction *action,
			   gpointer callback_data)
{
	nautilus_menu_item_activate (NAUTILUS_MENU_ITEM (callback_data));
}

static void
extension_action_sensitive_callback (NautilusMenuItem *item,
                                     GParamSpec *arg1,
                                     gpointer user_data)
{
	gboolean value;
	
	g_object_get (G_OBJECT (item),
	              "sensitive", &value,
		      NULL);

	gtk_action_set_sensitive (GTK_ACTION (user_data), value);
}

GtkAction *
nautilus_action_from_menu_item (NautilusMenuItem *item)
{
	char *name, *label, *tip, *icon_name;
	gboolean sensitive, priority;
	GtkAction *action;
	GdkPixbuf *pixbuf;
	NautilusIconInfo *info;
	
	g_object_get (G_OBJECT (item), 
		      "name", &name, "label", &label, 
		      "tip", &tip, "icon", &icon_name,
		      "sensitive", &sensitive,
		      "priority", &priority,
		      NULL);
	
	action = gtk_action_new (name,
				 label,
				 tip,
				 icon_name);
	
	if (icon_name != NULL) {
		info = nautilus_icon_info_lookup_from_name (icon_name,
							    nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU));

		pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (info,
									  nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU));
		if (pixbuf != NULL) {
			g_object_set_data_full (G_OBJECT (action), "menu-icon",
						pixbuf,
						g_object_unref);
		}
		g_object_unref (info);
	}
	
	gtk_action_set_sensitive (action, sensitive);
	g_object_set (action, "is-important", priority, NULL);
	
	g_signal_connect_data (action, "activate",
			       G_CALLBACK (extension_action_callback),
			       g_object_ref (item), 
			       (GClosureNotify)g_object_unref, 0);
	
	g_free (name);
	g_free (label);
	g_free (tip);
	g_free (icon_name);
	
	return action;
}

GtkAction *
nautilus_toolbar_action_from_menu_item (NautilusMenuItem *item)
{
	char *name, *label, *tip, *icon_name;
	gboolean sensitive, priority;
	GtkAction *action;
	GdkPixbuf *pixbuf;
	NautilusIconInfo *info;
	
	g_object_get (G_OBJECT (item), 
		      "name", &name, "label", &label, 
		      "tip", &tip, "icon", &icon_name,
		      "sensitive", &sensitive,
		      "priority", &priority,
		      NULL);
	
	action = gtk_action_new (name,
				 label,
				 tip,
				 icon_name);
	
	if (icon_name != NULL) {
		info = nautilus_icon_info_lookup_from_name (icon_name, 24);

		pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (info, 24);
		if (pixbuf != NULL) {
			g_object_set_data_full (G_OBJECT (action), "toolbar-icon",
						pixbuf,
						g_object_unref);
		}
		g_object_unref (info);
	}
	
	gtk_action_set_sensitive (action, sensitive);
	g_object_set (action, "is-important", priority, NULL);
	
	g_signal_connect_data (action, "activate",
			       G_CALLBACK (extension_action_callback),
			       g_object_ref (item), 
			       (GClosureNotify)g_object_unref, 0);

	g_signal_connect_object (item, "notify::sensitive",
				 G_CALLBACK (extension_action_sensitive_callback),
				 action,
				 0);

	g_free (name);
	g_free (label);
	g_free (tip);
	g_free (icon_name);

	return action;
}
