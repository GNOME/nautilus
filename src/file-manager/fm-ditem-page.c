/*
 *  fm-ditem-page.c: Desktop item editing support
 * 
 *  Copyright (C) 2004 James Willcox
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: James Willcox <jwillcox@gnome.org>
 *
 */

#include <config.h>
#include "fm-ditem-page.h"

#include <string.h>

#include <eel/eel-glib-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-file-info.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libgnome/gnome-desktop-item.h>

typedef struct ItemEntry {
	const char *field;
	const char *description;
	gboolean localized;
	gboolean filename;
} ItemEntry;

enum {
	TARGET_URI_LIST
};

static const GtkTargetEntry target_table[] = {
        { "text/uri-list",  0, TARGET_URI_LIST }
};


static ItemEntry *
item_entry_new (const char *field,
		const char *description,
		gboolean localized,
		gboolean filename)
{
	ItemEntry *entry;

	entry = g_new0 (ItemEntry, 1);
	entry->field = field;
	entry->description = description;
	entry->localized = localized;
	entry->filename = filename;
	
	return entry;
}

static void
item_entry_free (ItemEntry *entry)
{
	g_free (entry);
}

static void
fm_ditem_page_url_drag_data_received (GtkWidget *widget, GdkDragContext *context,
				      int x, int y,
				      GtkSelectionData *selection_data,
				      guint info, guint time,
				      GtkEntry *entry)
{
	char **uris;
	gboolean exactly_one;
	char *path;
	
	uris = g_strsplit (selection_data->data, "\r\n", 0);
        exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');

	if (!exactly_one) {
		g_strfreev (uris);
		return;
	}

	path = g_filename_from_uri (uris[0], NULL, NULL);
	if (path != NULL) {
		gtk_entry_set_text (entry, path);
		g_free (path);
	} else {
		gtk_entry_set_text (entry, uris[0]);
	}
	
	g_strfreev (uris);
}

static void
fm_ditem_page_exec_drag_data_received (GtkWidget *widget, GdkDragContext *context,
				       int x, int y,
				       GtkSelectionData *selection_data,
				       guint info, guint time,
				       GtkEntry *entry)
{
	char **uris;
	gboolean exactly_one;
	NautilusFile *file;
	GnomeDesktopItem *item;
	char *uri;
	const char *exec;
	
	uris = g_strsplit (selection_data->data, "\r\n", 0);
        exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');

	if (!exactly_one) {
		g_strfreev (uris);
		return;
	}

	file = nautilus_file_get_by_uri (uris[0]);

	g_return_if_fail (file != NULL);
	
	uri = nautilus_file_get_uri (file);
	if (nautilus_file_is_mime_type (file, "application/x-desktop")) {
		item = gnome_desktop_item_new_from_uri (uri,
							GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
							NULL);
		if (item != NULL &&
		    gnome_desktop_item_get_entry_type (item) == GNOME_DESKTOP_ITEM_TYPE_APPLICATION) {
			
			exec = gnome_desktop_item_get_string (item, GNOME_DESKTOP_ITEM_EXEC);
			gtk_entry_set_text (entry,
					    exec?exec:"");
			gnome_desktop_item_unref (item);
			
			gtk_widget_grab_focus (GTK_WIDGET (entry));
		}
	} else {
		gtk_entry_set_text (entry,
				    uri?uri:"");
	}
	
	g_free (uri);
	
	nautilus_file_unref (file);
	
	g_strfreev (uris);
}

static void
save_entry (GtkEntry *entry, GnomeDesktopItem *item)
{
	GError *error;
	ItemEntry *item_entry;
	const char *val;

	item_entry = g_object_get_data (G_OBJECT (entry), "item_entry");
	val = gtk_entry_get_text (entry);

	if (item_entry->localized) {
		gnome_desktop_item_set_localestring (item, item_entry->field, val);
	} else {
		gnome_desktop_item_set_string (item, item_entry->field, val);
	}

	error = NULL;

	if (!gnome_desktop_item_save (item, NULL, TRUE, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
entry_activate_cb (GtkWidget *entry,
		    GnomeDesktopItem *item)
{
	save_entry (GTK_ENTRY (entry), item);
}

static gboolean
entry_focus_out_cb (GtkWidget *entry,
		    GdkEventFocus *event,
		    GnomeDesktopItem *item)
{
	save_entry (GTK_ENTRY (entry), item);
	return FALSE;
}

static GtkWidget *
build_table (GnomeDesktopItem *item,
	     GtkSizeGroup *label_size_group,
	     GList *entries) {
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry;
	GList *l;
	const char *val;
	int i;
	
	table = gtk_table_new (g_list_length (entries) + 1, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	i = 0;
	
	for (l = entries; l; l = l->next) {
		ItemEntry *item_entry = (ItemEntry *)l->data;
		char *label_text;

		label_text = g_strdup_printf ("%s:", item_entry->description);
		label = gtk_label_new (label_text);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		g_free (label_text);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_size_group_add_widget (label_size_group, label);

		entry = gtk_entry_new ();

		if (item_entry->localized) {
			val = gnome_desktop_item_get_localestring (item,
								   item_entry->field);
		} else {
			val = gnome_desktop_item_get_string (item, item_entry->field);
		}
		
		gtk_entry_set_text (GTK_ENTRY (entry), val?val:"");

		gtk_table_attach (GTK_TABLE (table), label,
				  0, 1, i, i+1, GTK_FILL, GTK_FILL,
				  0, 0);
		gtk_table_attach (GTK_TABLE (table), entry,
				  1, 2, i, i+1, GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL,
				  0, 0);
		g_signal_connect (entry, "activate",
				  G_CALLBACK (entry_activate_cb),
				  item);
		g_signal_connect (entry, "focus_out_event",
				  G_CALLBACK (entry_focus_out_cb),
				  item);
		
		g_object_set_data_full (G_OBJECT (entry), "item_entry", item_entry,
					(GDestroyNotify)item_entry_free);

		if (item_entry->filename) {
			gtk_drag_dest_set (GTK_WIDGET (entry),
					   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
					   target_table, G_N_ELEMENTS (target_table),
					   GDK_ACTION_COPY | GDK_ACTION_MOVE);
			
			g_signal_connect (entry, "drag_data_received",
					  G_CALLBACK (fm_ditem_page_url_drag_data_received),
					  entry);
		} else if (strcmp (item_entry->field,
				   GNOME_DESKTOP_ITEM_EXEC) == 0) {
			gtk_drag_dest_set (GTK_WIDGET (entry),
					   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
					   target_table, G_N_ELEMENTS (target_table),
					   GDK_ACTION_COPY | GDK_ACTION_MOVE);
			
			g_signal_connect (entry, "drag_data_received",
					  G_CALLBACK (fm_ditem_page_exec_drag_data_received),
					  entry);
		}
		
		i++;
	}

	/* append dummy row */
	label = gtk_label_new ("");
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1, i, i+1, GTK_FILL, GTK_FILL,
			  0, 0);
	gtk_size_group_add_widget (label_size_group, label);


	gtk_widget_show_all (table);
	return table;
}

static void
box_weak_cb (gpointer user_data,
	     GObject *box)
{
	GnomeDesktopItem *item;

	item = (GnomeDesktopItem *) user_data;
	gnome_desktop_item_unref (item);
}


static void
create_page (GnomeDesktopItem *item, GtkWidget *box)
{
	GtkWidget *table;
	GList *entries;
	GtkSizeGroup *label_size_group;
	GnomeDesktopItemType item_type;
	
	entries = NULL;

	item_type = gnome_desktop_item_get_entry_type (item);
	
	if (item_type == GNOME_DESKTOP_ITEM_TYPE_LINK) {
		entries = g_list_prepend (entries,
					  item_entry_new (GNOME_DESKTOP_ITEM_COMMENT,
							  _("Comment"), TRUE, FALSE));
		entries = g_list_prepend (entries,
					  item_entry_new (GNOME_DESKTOP_ITEM_URL,
							  _("URL"), FALSE, TRUE));
		entries = g_list_prepend (entries,
					  item_entry_new (GNOME_DESKTOP_ITEM_GENERIC_NAME,
							  _("Description"), TRUE, FALSE));
	} else if (item_type == GNOME_DESKTOP_ITEM_TYPE_APPLICATION) {
		entries = g_list_prepend (entries,
					  item_entry_new (GNOME_DESKTOP_ITEM_COMMENT,
							  _("Comment"), TRUE, FALSE));
		entries = g_list_prepend (entries,
					  item_entry_new (GNOME_DESKTOP_ITEM_EXEC,
							  _("Command"), FALSE, FALSE));
		entries = g_list_prepend (entries,
					  item_entry_new (GNOME_DESKTOP_ITEM_GENERIC_NAME,
							  _("Description"), TRUE, FALSE));
	} else {
		/* we only handle launchers and links */

		/* ensure that we build an empty table with a dummy row at the end */
		goto build_table;
	}

	gnome_desktop_item_ref (item);

	g_object_weak_ref (G_OBJECT (box),
			   box_weak_cb, item);


build_table:
	label_size_group = g_object_get_data (G_OBJECT (box), "label-size-group");

	table = build_table (item, label_size_group, entries);
	g_list_free (entries);
	
	gtk_box_pack_start (GTK_BOX (box), table, FALSE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (box));
}


static void
ditem_read_cb (GObject *source_object,
	       GAsyncResult *res,
	       gpointer user_data)
{
	GnomeDesktopItem *item;
	GtkWidget *box;
	gsize file_size;
	char *file_contents;

	box = GTK_WIDGET (user_data);
	
	if (g_file_load_contents_finish (G_FILE (source_object),
					 res,
					 &file_contents, &file_size,
					 NULL, NULL)) {
		item = gnome_desktop_item_new_from_string (g_object_get_data (G_OBJECT (box),
									      "uri"),
							   file_contents,
							   file_size,
							   0, NULL);
		g_free (file_contents);
		if (item == NULL) {
			return;
		}
		
		/* for some reason, this isn't done automatically */
		gnome_desktop_item_set_location (item, g_object_get_data (G_OBJECT (box), "uri"));
		
		create_page (item, box);
		gnome_desktop_item_unref (item);
	}
	g_object_unref (box);
}

static void
fm_ditem_page_create_begin (const char *uri,
			    GtkWidget *box)
{
	GFile *location;

	location = g_file_new_for_uri (uri);
	g_object_set_data_full (G_OBJECT (box), "uri", g_strdup (uri), g_free);
	g_file_load_contents_async (location, NULL, ditem_read_cb, g_object_ref (box));
	g_object_unref (location);
}

GtkWidget *
fm_ditem_page_make_box (GtkSizeGroup *label_size_group,
			GList *files)
{
	NautilusFileInfo *info;
	char *uri;
	GtkWidget *box;

	g_assert (fm_ditem_page_should_show (files));

	box = gtk_vbox_new (FALSE, 6);
	g_object_set_data_full (G_OBJECT (box), "label-size-group",
				label_size_group, (GDestroyNotify) g_object_unref);

	info = NAUTILUS_FILE_INFO (files->data);

	uri = nautilus_file_info_get_uri (info);
	fm_ditem_page_create_begin (uri, box);
	g_free (uri);

	return box;
}

gboolean
fm_ditem_page_should_show (GList *files)
{
	NautilusFileInfo *info;

	if (!files || files->next) {
		return FALSE;
	}

	info = NAUTILUS_FILE_INFO (files->data);

	if (!nautilus_file_info_is_mime_type (info, "application/x-desktop")) {
		return FALSE;
	}

	return TRUE;
}

